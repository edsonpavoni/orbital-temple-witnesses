#!/usr/bin/env python3
"""
test3.py — recipe validation across the full amplitude range.

Once a recipe (kp/ki/kd/mc/mt/easing) has been chosen, run a series of
out-and-back moves at increasing amplitude to see if the recipe holds for
every angle the sculpture might be asked to reach. The motor stays in
position-mode hold the entire time; zero is calibrated once at the start.

Sequence (default step=45, max=360):

    0 -> 45  -> 0
    0 -> 90  -> 0
    0 -> 135 -> 0
    ...
    0 -> 360 -> 0

Each move is observed for `mt + settle` ms (default mt=3000, settle=3000).
Per-move metrics are computed (lag, overshoot, final error, settling time,
ringdown crossings, peak speed, peak temp) and saved to a CSV plus per-move
raw logs.

Workflow:

    test3.py calibrate
        -> release motor; user positions pointer at visual 0 by hand
    (user says "ready")
    test3.py go [--step 45] [--max 360]
        -> latches zero, runs the full sweep, prints summary
    test3.py release

The recipe is built in (kp=30M, ki=1000, kd=40M, mc=1000, mt=3000, easing
in_out, easing on) — derived from test 2H. Override on the CLI if desired.
"""

import argparse
import csv
import subprocess
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
LAB = str(THIS_DIR / "lab.py")
RESULTS_DIR = THIS_DIR / "results"
CSV_FILE = RESULTS_DIR / "test3_log.csv"

PYTHON = sys.executable

# Recipe locked in by test 2H.
RECIPE = {
    "kp": 30_000_000,
    "ki": 1_000,
    "kd": 40_000_000,
    "mc": 1000,
    "mt": 3000,
    "easing_curve": 1,    # in_out
    "easing_on": True,
}

# Empirical: at the recipe's PID, A=90 with mt=3000 ms produced an actual
# measured peak speed of ~12.1 RPM (test 2H). To keep peak speed roughly
# constant across amplitudes, scale mt linearly with distance.
MT_PER_DEG_MS = 3000.0 / 90.0   # 33.33 ms/deg


def compute_mt(distance_deg):
    """Return the move-time (ms) that should yield ~constant peak RPM."""
    return max(500, int(round(abs(distance_deg) * MT_PER_DEG_MS)))

EASING_NAMES = {"linear": 0, "in_out": 1, "ease_in": 2, "ease_out": 3,
                "0": 0, "1": 1, "2": 2, "3": 3}


def lab(*args):
    return subprocess.check_output([PYTHON, LAB, *args], text=True)


def lab_send(text):
    subprocess.check_call([PYTHON, LAB, "send", text])


def bridge_alive():
    return "RUNNING" in lab("status")


# ---------- log parser (shared logic with test2) ----------

def parse_log(text):
    events = []
    state = {}
    last_t = None
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
            last_t = int(state.get("t", "0"))
        except ValueError:
            continue
        snap = {"t": last_t}
        for k in ("st", "Tg", "sp", "p", "er", "a", "tr", "tmp", "v"):
            if k in state:
                snap[k] = state[k]
        events.append(snap)
    return events


def f(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def metrics_for_move(events, target):
    """Find the move (st=M) and analyse it. Target is the absolute angle the
    motor was commanded toward."""
    t_goto = None
    prev_st = None
    for ev in events:
        st = ev.get("st")
        if st == "M" and prev_st != "M":
            t_goto = ev["t"]
        prev_st = st
    if t_goto is None:
        return None

    t_end = None
    max_lag = 0.0
    peak_speed_during_move = 0.0
    p_at_end = None
    for ev in events:
        if ev["t"] < t_goto:
            continue
        if ev.get("st") == "M":
            er = f(ev.get("er"))
            a = f(ev.get("a"))
            if er is not None and abs(er) > max_lag:
                max_lag = abs(er)
            if a is not None and abs(a) > peak_speed_during_move:
                peak_speed_during_move = abs(a)
        if t_end is None and ev.get("tr") == "-" and ev["t"] > t_goto + 100:
            t_end = ev["t"]
            p_at_end = f(ev.get("p"))
            break
    if t_end is None:
        return None

    window_end = t_end + 2500
    overshoot = 0.0
    undershoot = 0.0
    final_p = p_at_end
    settling_ms = None
    last_in_band_start = None
    zero_crossings = 0
    last_sign = None
    peak_post_speed = 0.0
    peak_temp = 0

    for ev in events:
        if ev["t"] < t_end:
            continue
        if ev["t"] > window_end:
            break
        p = f(ev.get("p"))
        a = f(ev.get("a"))
        tmp = f(ev.get("tmp"))
        if a is not None and abs(a) > peak_post_speed:
            peak_post_speed = abs(a)
        if tmp is not None and tmp > peak_temp:
            peak_temp = tmp
        if p is None:
            continue
        final_p = p
        delta = p - target
        if delta > overshoot:
            overshoot = delta
        if delta < undershoot:
            undershoot = delta
        sign = 1 if delta > 0 else (-1 if delta < 0 else 0)
        if last_sign is not None and sign != 0 and last_sign != 0 and sign != last_sign:
            zero_crossings += 1
        if sign != 0:
            last_sign = sign
        if abs(delta) <= 0.5:
            if last_in_band_start is None:
                last_in_band_start = ev["t"]
            elif (ev["t"] - last_in_band_start) >= 200 and settling_ms is None:
                settling_ms = last_in_band_start - t_end
        else:
            last_in_band_start = None

    return {
        "move_duration_ms": t_end - t_goto,
        "max_lag_during_move_deg": round(max_lag, 2),
        "peak_speed_during_move_rpm": round(peak_speed_during_move, 1),
        "overshoot_deg": round(overshoot, 2),
        "max_undershoot_deg": round(undershoot, 2),
        "final_pos_deg": round(final_p, 2) if final_p is not None else None,
        "final_error_deg": round((final_p - target) if final_p is not None else 0.0, 2),
        "settling_ms": settling_ms,
        "ringdown_zero_crossings": zero_crossings,
        "peak_post_speed_rpm": round(peak_post_speed, 1),
        "peak_temp_C": peak_temp,
    }


def apply_params(p):
    if p.get("kp") is not None: lab_send(f"kp{int(p['kp'])}"); time.sleep(0.05)
    if p.get("ki") is not None: lab_send(f"ki{int(p['ki'])}"); time.sleep(0.05)
    if p.get("kd") is not None: lab_send(f"kd{int(p['kd'])}"); time.sleep(0.05)
    if p.get("mc") is not None: lab_send(f"c{int(p['mc'])}"); time.sleep(0.05)
    if p.get("mt") is not None: lab_send(f"mt{int(p['mt'])}"); time.sleep(0.05)
    if p.get("easing_curve") is not None: lab_send(f"e{int(p['easing_curve'])}"); time.sleep(0.05)
    if p.get("easing_on") is not None: lab_send("eon" if p["easing_on"] else "eoff"); time.sleep(0.05)


# ---------- subcommands ----------

def cmd_calibrate(_args):
    if not bridge_alive():
        print("ERROR: bridge not running.", file=sys.stderr)
        return 2
    lab_send("x")
    time.sleep(0.2)
    print()
    print("=== test3 calibrate ===")
    print("Motor RELEASED. Position the pointer at visual 0 by hand.")
    print("When ready, run:  test3.py go")
    return 0


def cmd_release(_args):
    if not bridge_alive():
        print("ERROR: bridge not running.", file=sys.stderr)
        return 2
    lab_send("x")
    print("Released.")
    return 0


def cmd_go(args):
    if not bridge_alive():
        print("ERROR: bridge not running.", file=sys.stderr)
        return 2

    recipe = dict(RECIPE)
    if args.kp is not None: recipe["kp"] = args.kp
    if args.ki is not None: recipe["ki"] = args.ki
    if args.kd is not None: recipe["kd"] = args.kd
    if args.mc is not None: recipe["mc"] = args.mc
    if args.mt is not None: recipe["mt"] = args.mt
    if args.easing_curve is not None:
        recipe["easing_curve"] = EASING_NAMES.get(args.easing_curve.lower(), 1)

    print()
    print("=== test3 go ===")
    print("Recipe:")
    for k, v in recipe.items():
        print(f"   {k:14s} = {v}")
    print(f"Step: {args.step} deg   Max: {args.max} deg   Settle: {args.settle} s")

    # Build amplitude list
    amplitudes = []
    a = args.step
    while a <= args.max + 1:
        amplitudes.append(a)
        a += args.step
    print(f"Amplitudes: {amplitudes}")
    print()

    # Apply recipe once (mt will be overridden per-move below)
    apply_params(recipe)
    time.sleep(0.3)

    # Latch zero from current encoder position
    lab("clear")
    lab_send("0")
    time.sleep(0.5)

    settle_s = float(args.settle)
    rows = []
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)

    for amp in amplitudes:
        for direction, target in (("out", float(amp)), ("back", 0.0)):
            label = f"amp{amp}_{direction}"
            distance = float(amp)   # both directions cover the same |delta|
            mt_ms = compute_mt(distance)
            move_s = mt_ms / 1000.0
            # Push the per-move mt to the firmware just before the move
            lab_send(f"mt{mt_ms}")
            time.sleep(0.05)
            print(f"--- {label}: goto {target:.0f} (mt={mt_ms}ms) ---")
            lab("clear")
            lab_send(f"goto{int(target)}")
            time.sleep(move_s + settle_s)

            log_text = lab("log")
            (RESULTS_DIR / f"test3_{label}.log").write_text(log_text)

            events = parse_log(log_text)
            m = metrics_for_move(events, target)
            if m is None:
                print("   (no move detected)")
                continue
            short = (f"   move={m['move_duration_ms']}ms  "
                     f"lag={m['max_lag_during_move_deg']}deg  "
                     f"over={m['overshoot_deg']}  "
                     f"under={m['max_undershoot_deg']}  "
                     f"final={m['final_pos_deg']} (err={m['final_error_deg']})  "
                     f"settle={m['settling_ms']}ms  "
                     f"rings={m['ringdown_zero_crossings']}  "
                     f"peak={m['peak_speed_during_move_rpm']}rpm  "
                     f"tmp={m['peak_temp_C']}C")
            print(short)
            rows.append({
                "amplitude_deg": amp,
                "direction": direction,
                "target_deg": target,
                **m,
            })

    print()
    print("--- complete; releasing ---")
    lab_send("x")

    # Write CSV
    fields = ["ts", "amplitude_deg", "direction", "target_deg",
              "move_duration_ms", "max_lag_during_move_deg",
              "peak_speed_during_move_rpm",
              "overshoot_deg", "max_undershoot_deg",
              "final_pos_deg", "final_error_deg",
              "settling_ms", "ringdown_zero_crossings",
              "peak_post_speed_rpm", "peak_temp_C"]
    csv_exists = CSV_FILE.exists()
    with open(CSV_FILE, "a", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        if not csv_exists:
            w.writeheader()
        ts = time.strftime("%Y-%m-%d %H:%M:%S")
        for r in rows:
            w.writerow({"ts": ts, **r})

    # Print summary table
    print()
    print("=== summary ===")
    print(f"{'amp':>4s} {'dir':>5s} {'target':>7s} {'lag':>6s} {'over':>6s} "
          f"{'under':>7s} {'final':>7s} {'fin_err':>8s} {'settle':>7s} "
          f"{'rings':>5s} {'peak':>5s} {'tmp':>4s}")
    for r in rows:
        print(f"{int(r['amplitude_deg']):>4d} {str(r['direction']):>5s} "
              f"{float(r['target_deg']):>7.1f} {float(r['max_lag_during_move_deg']):>6.2f} "
              f"{float(r['overshoot_deg']):>6.2f} {float(r['max_undershoot_deg']):>7.2f} "
              f"{float(r['final_pos_deg'] or 0):>7.2f} "
              f"{float(r['final_error_deg']):>8.2f} "
              f"{str(r['settling_ms']):>7s} "
              f"{int(r['ringdown_zero_crossings']):>5d} "
              f"{float(r['peak_speed_during_move_rpm']):>5.1f} "
              f"{int(r['peak_temp_C']):>4d}")
    print()
    print(f"Per-move logs: {RESULTS_DIR}/test3_amp<deg>_<out|back>.log")
    print(f"CSV: {CSV_FILE}")
    return 0


def main(argv):
    p = argparse.ArgumentParser(prog="test3.py")
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("calibrate")
    sub.add_parser("release")

    s = sub.add_parser("go")
    s.add_argument("--step", type=int, default=45)
    s.add_argument("--max", type=int, default=360)
    s.add_argument("--settle", type=float, default=3.0,
                   help="seconds to wait after move duration")
    s.add_argument("--kp", type=int, default=None)
    s.add_argument("--ki", type=int, default=None)
    s.add_argument("--kd", type=int, default=None)
    s.add_argument("--mc", type=int, default=None)
    s.add_argument("--mt", type=int, default=None)
    s.add_argument("--easing-curve", default=None)

    args = p.parse_args(argv)
    if args.cmd == "calibrate":
        return cmd_calibrate(args)
    if args.cmd == "release":
        return cmd_release(args)
    if args.cmd == "go":
        return cmd_go(args)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
