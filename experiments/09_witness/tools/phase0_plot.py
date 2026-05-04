#!/usr/bin/env python3
"""
phase0_plot.py — visualise a diagnostic-sweep log.

Reads the compressed delta-log captured during the two-direction `diag`
sweep, splits it into the CW and CCW segments, and prints simple ASCII
plots of each tracked signal vs encoder angle.

The point is not pretty graphics — it is to answer three concrete questions:

  1. Is the gravity sinusoid visible in any of (er, a, cur)?
  2. Which signal looks cleanest?
  3. After sign-flipping the CCW data, do the two sweeps phase-agree?

Usage:
    python tools/phase0_plot.py path/to/diag.log

The log is whatever you captured between `send diag` and the next session.
Lines with `ph=C` are the CW sweep; `ph=A` are the CCW sweep; `ph=P` is
the pause between them; `ph=H` is the hold before/after.
"""

import argparse
import math
import sys
from pathlib import Path


# ---------- delta-log parser ----------

LOG_FIELDS = ("ph", "Tg", "sp", "p", "er", "a", "cur", "tmp", "v")


def parse_log(text):
    """Walk the compressed log, carry forward unchanged fields, return a
    list of snapshot dicts (one per delta-log line)."""
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


# ---------- segment splitting ----------

def split_segments(events):
    """Return (cw_events, ccw_events) — the slices of the log corresponding
    to the CW and CCW sweeps (ph='C' and ph='A' respectively)."""
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


# ---------- ASCII plot ----------

def ascii_plot(xs, ys, width=70, height=15, label="", xlabel="", ylabel=""):
    """Tiny scatter plot for a stream of (x, y) pairs. Both axes auto-scale."""
    pairs = [(x, y) for x, y in zip(xs, ys) if x is not None and y is not None]
    if not pairs:
        print(f"  ({label}: no data)")
        return

    xs2 = [p[0] for p in pairs]
    ys2 = [p[1] for p in pairs]
    xmin, xmax = min(xs2), max(xs2)
    ymin, ymax = min(ys2), max(ys2)
    if xmax - xmin < 1e-6: xmax = xmin + 1
    if ymax - ymin < 1e-6: ymax = ymin + 1

    grid = [[" "] * width for _ in range(height)]
    for x, y in pairs:
        col = int((x - xmin) / (xmax - xmin) * (width - 1))
        row = int((ymax - y) / (ymax - ymin) * (height - 1))
        col = max(0, min(width - 1, col))
        row = max(0, min(height - 1, row))
        grid[row][col] = "*"

    # Add zero-line if y range straddles zero
    if ymin < 0 < ymax:
        zero_row = int((ymax - 0) / (ymax - ymin) * (height - 1))
        for c in range(width):
            if grid[zero_row][c] == " ":
                grid[zero_row][c] = "-"

    print()
    print(f"  {label}")
    print(f"  {ylabel:>8s} {ymax:>+7.2f} +" + "-" * width + "+")
    for row in grid:
        print(f"           {'':>7s}  |" + "".join(row) + "|")
    print(f"           {ymin:>+7.2f} +" + "-" * width + "+")
    print(f"           {xmin:>+9.1f}{' ' * (width - 9)}{xmax:>+9.1f}  ({xlabel})")


# ---------- segment summary ----------

def segment_stats(events, target_field):
    """Mean, min, max, peak-to-peak for one numeric field over a segment."""
    vals = [f(e.get(target_field)) for e in events]
    vals = [v for v in vals if v is not None]
    if not vals:
        return None
    mn, mx = min(vals), max(vals)
    return {
        "n": len(vals),
        "min": mn,
        "max": mx,
        "p2p": mx - mn,
        "mean": sum(vals) / len(vals),
    }


def fold_to_360(angle):
    """Map an angle in degrees to [0, 360)."""
    return angle % 360.0


# ---------- gravity-signal check ----------

def gravity_check(cw_events, ccw_events, signal):
    """For each phase point on a 5-degree grid, average the signal value at
    that absolute angle. CCW values are sign-flipped on the assumption that
    friction (anti-symmetric) inverts and gravity (symmetric) does not.

    Returns (cw_at_angle, ccw_at_angle) — two arrays indexed by 5-deg bin.
    Their AGREEMENT is the gravity signal; their SUM (after CCW unflipped) is
    the friction signal."""
    bin_count = 72   # 5-deg bins
    cw_sum = [0.0] * bin_count
    cw_n = [0] * bin_count
    ccw_sum = [0.0] * bin_count
    ccw_n = [0] * bin_count

    for ev in cw_events:
        p = f(ev.get("p"))
        v = f(ev.get(signal))
        if p is None or v is None:
            continue
        b = int(fold_to_360(p) / 5.0) % bin_count
        cw_sum[b] += v
        cw_n[b] += 1

    for ev in ccw_events:
        p = f(ev.get("p"))
        v = f(ev.get(signal))
        if p is None or v is None:
            continue
        b = int(fold_to_360(p) / 5.0) % bin_count
        # For 'er': lag in CCW has opposite sign convention vs CW because the
        # setpoint and pointer direction are flipped. Sign-flip so that the
        # gravity contribution (position-locked) ends up in phase across both.
        ccw_sum[b] += -v if signal == "er" else v
        ccw_n[b] += 1

    cw_avg = [(cw_sum[i] / cw_n[i] if cw_n[i] else None) for i in range(bin_count)]
    ccw_avg = [(ccw_sum[i] / ccw_n[i] if ccw_n[i] else None) for i in range(bin_count)]

    angles = [i * 5.0 for i in range(bin_count)]
    return angles, cw_avg, ccw_avg


# ---------- main ----------

def main(argv):
    p = argparse.ArgumentParser(prog="phase0_plot.py")
    p.add_argument("log", help="path to a captured diag log")
    args = p.parse_args(argv)

    text = Path(args.log).read_text()
    events = parse_log(text)
    cw, ccw = split_segments(events)

    print(f"Parsed {len(events)} delta-log lines.")
    print(f"  CW segment: {len(cw)} samples")
    print(f"  CCW segment: {len(ccw)} samples")

    if not cw or not ccw:
        print("ERROR: missing CW or CCW segment. Make sure `diag` ran to "
              "completion before capturing.")
        return 1

    # Per-segment stats per signal
    print()
    print("=== signal stats (per segment) ===")
    for signal in ("er", "a", "cur"):
        cs = segment_stats(cw, signal)
        ns = segment_stats(ccw, signal)
        cs_str = f"n={cs['n']}, range [{cs['min']:+.2f}, {cs['max']:+.2f}], p2p {cs['p2p']:.2f}, mean {cs['mean']:+.2f}" if cs else "no data"
        ns_str = f"n={ns['n']}, range [{ns['min']:+.2f}, {ns['max']:+.2f}], p2p {ns['p2p']:.2f}, mean {ns['mean']:+.2f}" if ns else "no data"
        print(f"  {signal:>4s}   CW : {cs_str}")
        print(f"  {signal:>4s}   CCW: {ns_str}")

    # ASCII plots: signal vs encoder angle, both directions overlaid via folding
    for signal in ("er", "a", "cur"):
        cw_xs = [f(e.get("p")) for e in cw]
        cw_xs = [x % 360.0 if x is not None else None for x in cw_xs]
        cw_ys = [f(e.get(signal)) for e in cw]
        ascii_plot(cw_xs, cw_ys,
                   label=f"{signal.upper()} during CW vs encoder angle (mod 360)",
                   xlabel="encoder deg", ylabel=signal)

        ccw_xs = [f(e.get("p")) for e in ccw]
        ccw_xs = [x % 360.0 if x is not None else None for x in ccw_xs]
        ccw_ys = [f(e.get(signal)) for e in ccw]
        ascii_plot(ccw_xs, ccw_ys,
                   label=f"{signal.upper()} during CCW vs encoder angle (mod 360)",
                   xlabel="encoder deg", ylabel=signal)

    # Gravity-signal check: bin the data, compare CW and (sign-flipped) CCW
    print()
    print("=== gravity-signal check (5-deg bins) ===")
    print("If gravity dominates, CW and (sign-flipped) CCW averages should")
    print("track each other closely. Listing bins where both have data:")
    for signal in ("er", "a", "cur"):
        angles, cw_avg, ccw_avg = gravity_check(cw, ccw, signal)
        # Compute correlation-ish metric between cw and ccw across bins
        pairs = [(c, n) for c, n in zip(cw_avg, ccw_avg) if c is not None and n is not None]
        if not pairs:
            print(f"  {signal}: no overlap")
            continue
        cs = [p[0] for p in pairs]
        ns = [p[1] for p in pairs]
        cm = sum(cs) / len(cs)
        nm = sum(ns) / len(ns)
        # Pearson r
        num = sum((c - cm) * (n - nm) for c, n in zip(cs, ns))
        den = math.sqrt(sum((c - cm)**2 for c in cs) * sum((n - nm)**2 for n in ns))
        r = num / den if den > 1e-9 else 0.0
        cw_p2p = max(cs) - min(cs)
        print(f"  {signal:>4s}: bins={len(pairs)}  CW p2p={cw_p2p:.2f}  "
              f"CW vs (sign-flip CCW for er) Pearson r = {r:+.3f}")

    print()
    print("Interpretation guide:")
    print("  - p2p (peak-to-peak) tells you the signal amplitude across one rev")
    print("  - r close to +1 => CW and (sign-flipped) CCW agree => gravity dominates")
    print("  - r close to  0 => no consistent signal => friction / noise dominates")
    print("  - r close to -1 => something is mis-flipped (sign convention error)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
