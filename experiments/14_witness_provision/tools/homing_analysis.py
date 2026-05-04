#!/usr/bin/env python3
"""
homing_analysis.py — Phase 1 sensorless-homing algorithm.

Takes a captured `diag` log (CW + CCW sweep at the diagnostic PID) and
estimates the encoder angle of "up" (the unstable equilibrium opposite the
gravity rest of the unbalanced pointer).

Math:

  Steady-state torque balance for the pointer in motion:
      tau_motor + tau_gravity - sign(omega) * |tau_friction| = 0

  Solving for motor torque (which is proportional to motor current):
      tau_m_CW(p)  =  m*g*r*sin(p + c) + |tau_f|     (omega > 0, +CW)
      tau_m_CCW(p) =  m*g*r*sin(p + c) - |tau_f|     (omega < 0, -CCW)

  Where p is the encoder angle (user frame, after `hold` latched zero) and
  c is the unknown offset to "up": p + c = 0 means pointer is straight up.

  Sum cancels friction, gives 2x gravity:
      tau_m_CW + tau_m_CCW = 2 * m*g*r*sin(p + c)

  Fit a sinusoid to the binned average of (I_CW + I_CCW)/2 vs p, recover
  the phase c, and "up" is at encoder angle p = -c (mod 360 deg).

  The lag signal `er` carries the same gravity dependence (motor lags more
  where gravity opposes its motion, less where gravity helps). Same algorithm
  works on either signal; we run both and report whichever has the cleaner
  fit (highest R^2).

Usage:
    python tools/homing_analysis.py <diag.log>
"""

import argparse
import math
import sys
from pathlib import Path


# ---------- log parser (shared with phase0_plot.py) ----------

LOG_FIELDS = ("ph", "Tg", "sp", "p", "er", "a", "cur", "tmp", "v")


def parse_log(text):
    state = {}
    events = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith(("[->]", ">>", "===", "#")):
            continue
        if not line.startswith("t="):
            continue
        for kv in line.split(","):
            if "=" not in kv:
                continue
            k, v = kv.split("=", 1)
            state[k.strip()] = v.strip()
        try:
            t = int(state.get("t", "0"))
        except ValueError:
            continue
        snap = {"t": t}
        for k in LOG_FIELDS:
            if k in state:
                snap[k] = state[k]
        events.append(snap)
    return events


def f(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def split_segments(events):
    cw, ccw = [], []
    cur_phase = None
    for ev in events:
        if "ph" in ev:
            cur_phase = ev["ph"]
        if cur_phase == "C":
            cw.append(ev)
        elif cur_phase == "A":
            ccw.append(ev)
    return cw, ccw


# ---------- binning ----------

BIN_DEG = 5
BIN_COUNT = 360 // BIN_DEG


def bin_signal(events, signal):
    """Average values of `signal` per BIN_DEG-wide bin of encoder angle."""
    sums = [0.0] * BIN_COUNT
    counts = [0] * BIN_COUNT
    for ev in events:
        p = f(ev.get("p"))
        v = f(ev.get(signal))
        if p is None or v is None:
            continue
        b = int((p % 360.0) / BIN_DEG) % BIN_COUNT
        sums[b] += v
        counts[b] += 1
    return [(sums[i] / counts[i] if counts[i] else None) for i in range(BIN_COUNT)]


# ---------- sinusoid fit ----------
#
# Fit y(theta) = a*sin(theta) + b*cos(theta) + dc  via linear least squares
# (theta in radians; theta = encoder bin centre converted to radians).
#
# Then amplitude A = sqrt(a^2 + b^2), phase shift c such that
#     y(theta) = A * sin(theta + c) + dc
# where c = atan2(b, a).
#
# Zero of y(theta + c) = 0 -> theta = -c gives one zero crossing; +pi gives
# the other. The unstable equilibrium "up" is the zero crossing with positive
# slope (since just past up, gravity creates a positive motor-torque demand).

def fit_sinusoid(thetas_rad, ys):
    """Least-squares fit for y = a*sin(theta) + b*cos(theta) + dc.

    Returns (a, b, dc, r2)."""
    n = len(ys)
    if n < 4:
        return None
    sin_t = [math.sin(t) for t in thetas_rad]
    cos_t = [math.cos(t) for t in thetas_rad]

    # Build normal equations for linear LS.
    sxx = sum(s*s for s in sin_t)
    syy = sum(c*c for c in cos_t)
    sxy = sum(s*c for s, c in zip(sin_t, cos_t))
    sx = sum(sin_t)
    sy = sum(cos_t)
    sxz = sum(s*y for s, y in zip(sin_t, ys))
    syz = sum(c*y for c, y in zip(cos_t, ys))
    sz = sum(ys)

    # 3x3 matrix M and rhs r:
    #   [sxx  sxy  sx]   [a]    [sxz]
    #   [sxy  syy  sy] * [b] =  [syz]
    #   [sx   sy   n ]   [dc]   [sz ]
    M = [[sxx, sxy, sx],
         [sxy, syy, sy],
         [sx,  sy,  n ]]
    r = [sxz, syz, sz]
    sol = solve3x3(M, r)
    if sol is None:
        return None
    a, b, dc = sol

    # R^2
    y_mean = sum(ys) / n
    ss_tot = sum((y - y_mean)**2 for y in ys)
    ss_res = sum((y - (a*math.sin(t) + b*math.cos(t) + dc))**2
                 for t, y in zip(thetas_rad, ys))
    r2 = 1 - (ss_res / ss_tot if ss_tot > 1e-12 else 0)
    return a, b, dc, r2


def solve3x3(M, r):
    """Naive 3x3 linear solve. Returns None if singular."""
    # Gauss elimination
    A = [row[:] + [r[i]] for i, row in enumerate(M)]
    n = 3
    for i in range(n):
        # Pivot
        max_row = max(range(i, n), key=lambda k: abs(A[k][i]))
        if abs(A[max_row][i]) < 1e-12:
            return None
        A[i], A[max_row] = A[max_row], A[i]
        # Eliminate
        for j in range(i + 1, n):
            factor = A[j][i] / A[i][i]
            for k in range(i, n + 1):
                A[j][k] -= factor * A[i][k]
    # Back-substitute
    sol = [0.0] * n
    for i in range(n - 1, -1, -1):
        s = A[i][n]
        for j in range(i + 1, n):
            s -= A[i][j] * sol[j]
        sol[i] = s / A[i][i]
    return sol


# ---------- algorithm wrapper ----------

def estimate_up_angle(events, signal, verbose=False):
    """Return (up_deg, info_dict) — estimated encoder angle of "up", plus
    diagnostic info.  Returns (None, info) if the fit failed."""
    cw, ccw = split_segments(events)
    if not cw or not ccw:
        return None, {"reason": "missing CW or CCW segment"}

    cw_avg = bin_signal(cw, signal)
    ccw_avg = bin_signal(ccw, signal)

    # Gravity component: (I_CW + I_CCW) / 2 (sign convention may flip per
    # signal; for `cur` and `er` both, the simple sum cancels friction).
    gravity = []
    thetas = []
    for i in range(BIN_COUNT):
        if cw_avg[i] is None or ccw_avg[i] is None:
            continue
        g = (cw_avg[i] + ccw_avg[i]) / 2
        bin_centre_deg = (i + 0.5) * BIN_DEG
        gravity.append(g)
        thetas.append(math.radians(bin_centre_deg))

    if len(gravity) < 4:
        return None, {"reason": "not enough overlapping bins",
                      "n_bins": len(gravity)}

    fit = fit_sinusoid(thetas, gravity)
    if fit is None:
        return None, {"reason": "sinusoid fit failed"}
    a, b, dc, r2 = fit

    A = math.hypot(a, b)
    # phase: y = A * sin(theta + c) where c = atan2(b, a)
    c = math.atan2(b, a)
    # zero crossings of sin(theta + c) at theta = -c and theta = -c + pi.
    # "Up" = unstable equilibrium = the zero where sin' is positive.
    # sin'(theta + c) = cos(theta + c). At theta = -c: cos(0) = +1 (positive
    # slope). At theta = -c + pi: cos(pi) = -1 (negative slope).
    # So "up" is at theta_rad = -c.
    up_rad = -c
    up_deg = math.degrees(up_rad) % 360.0

    info = {
        "signal": signal,
        "n_bins_used": len(gravity),
        "amplitude": A,
        "dc_offset": dc,
        "phase_c_rad": c,
        "phase_c_deg": math.degrees(c) % 360.0,
        "r2": r2,
        "up_deg": up_deg,
    }
    return up_deg, info


# ---------- main ----------

def main(argv):
    p = argparse.ArgumentParser(prog="homing_analysis.py")
    p.add_argument("log", help="path to a captured diag log")
    p.add_argument("--signal", default=None,
                   help="force one of er|cur|a (default: try all and pick "
                        "the one with the highest R^2)")
    args = p.parse_args(argv)

    text = Path(args.log).read_text()
    events = parse_log(text)
    print(f"Parsed {len(events)} delta-log lines.")

    signals = [args.signal] if args.signal else ["er", "cur", "a"]
    results = []
    for sig in signals:
        up, info = estimate_up_angle(events, sig)
        results.append((sig, up, info))

    print()
    print("=== fit results ===")
    print(f"{'signal':>8s}  {'n_bins':>6s}  {'amplitude':>10s}  "
          f"{'dc_offset':>10s}  {'R^2':>6s}  {'up_deg':>8s}")
    for sig, up, info in results:
        if up is None:
            print(f"{sig:>8s}  FAILED: {info.get('reason')}")
            continue
        print(f"{sig:>8s}  {info['n_bins_used']:>6d}  "
              f"{info['amplitude']:>10.3f}  {info['dc_offset']:>10.3f}  "
              f"{info['r2']:>+6.3f}  {info['up_deg']:>8.2f}")

    # Pick best by R^2
    valid = [(sig, up, info) for sig, up, info in results if up is not None]
    if not valid:
        print("All fits failed.")
        return 1
    best = max(valid, key=lambda x: x[2]["r2"])
    print()
    print(f"Best signal: {best[0]} (R^2 = {best[2]['r2']:+.3f})")
    print(f"Estimated 'up' encoder angle: {best[1]:.2f} deg")
    print()
    print("Interpretation: if the user's hand-calibrated 'up' was at p=0, the")
    print("estimate should be close to 0 (or 360 — they are equivalent).")
    print("Report-back deviation is the algorithm's error on this run.")

    # Wraparound-aware error vs zero for convenience
    err_vs_zero = best[1]
    if err_vs_zero > 180:
        err_vs_zero -= 360
    print(f"Deviation from p=0: {err_vs_zero:+.2f} deg")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
