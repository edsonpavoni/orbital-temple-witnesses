#!/usr/bin/env python3
"""
test1.py — single-move precision benchmark.

Test definition (from the user):
    1. Release the motor and ask the user to position the pointer at visual 0.
    2. When user says "ready":
       a. Send `0` (latch zero, hold at 0 deg).
       b. Hold for hold_zero_sec.
       c. Send `goto90` (move to +90 deg with current trajectory parameters).
       d. Wait for the move to complete and observe settling.
       e. Leave the motor holding at 90 deg. Do NOT auto-release.

Workflow (each iteration):
    test1.py setup --label NAME [--kp N --ki N --kd N --mc N --mt N
                                  --easing-curve N --easing on|off
                                  --hold-zero N --settle N]
        -> applies parameters via the lab.py bridge
        -> sends `x` to release the motor
        -> writes parameter stash to /tmp/lab_pos.test1.json
        -> prints "ready for user to position pointer at visual 0"

    (user manually positions the pointer, then says "ready")

    test1.py run
        -> reads the stash
        -> clears the bridge log
        -> sends `0`, waits hold_zero seconds (motor holds at 0)
        -> sends `goto90`, waits mt/1000 + settle seconds
        -> captures the log, parses metrics
        -> prints a summary
        -> writes per-run log to tools/results/test1_<label>.log
        -> appends row to tools/results/test1_log.csv
        -> does NOT release; motor stays at 90 deg

    test1.py release
        -> sends `x`. For between-test cleanup.

The bridge (tools/lab.py start, run in background) must already be running.
"""

import argparse
import csv
import json
import os
import subprocess
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
LAB = str(THIS_DIR / "lab.py")
RESULTS_DIR = THIS_DIR / "results"
STASH = "/tmp/lab_pos.test1.json"
LOG_FILE = "/tmp/lab_pos.log"
CSV_FILE = RESULTS_DIR / "test1_log.csv"

PYTHON = sys.executable  # use whichever Python is running this script


# ---------- bridge helpers ----------

def lab(*args) -> str:
    """Invoke lab.py with the given args and return stdout."""
    out = subprocess.check_output([PYTHON, LAB, *args], text=True)
    return out


def lab_send(text: str) -> None:
    subprocess.check_call([PYTHON, LAB, "send", text])


def bridge_alive() -> bool:
    out = lab("status")
    return "RUNNING" in out


# ---------- log parser ----------

def parse_log(text: str):
    """Walk the log, carry-forward delta-encoded fields, return ordered events.

    Returns:
      events: list of (t_ms, snapshot_dict)
      cmd_marks: list of (t_ms_at_marker, cmd_text)  -- approximate (uses last-seen t)
    """
    events = []
    cmd_marks = []
    state = {}
    last_t = None

    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("[->]"):
            cmd_marks.append((last_t, line[4:].strip()))
            continue
        if line.startswith(">>") or line.startswith("===") or line.startswith("#"):
            continue
        if not line.startswith("t="):
            continue
        # delta-log line
        for kv in line.split(","):
            if "=" not in kv:
                continue
            k, v = kv.split("=", 1)
            state[k.strip()] = v.strip()
        # parse t
        try:
            last_t = int(state.get("t", "0"))
        except ValueError:
            continue
        # snapshot relevant fields
        snap = {"t": last_t}
        for k in ("st", "ra", "Tg", "sp", "p", "er", "a", "tr",
                 "tmp", "v", "mc", "e", "eon", "mt", "hold", "step",
                 "kP", "kI", "kD"):
            if k in state:
                snap[k] = state[k]
        events.append(snap)

    return events, cmd_marks


def f(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


# ---------- metrics ----------

def compute_metrics(events, cmd_marks, target_deg: float):
    """Extract:
       - move_duration_ms: time between goto and tr returning to '-'
       - max_lag_during_move: max |er| while st=M
       - overshoot_deg: max(p - target) during the first 2 s after move ends
       - undershoot_deg: min(p - target) during the same window (signed)
       - settling_time_ms: time after move-end for |er| <= 0.5 deg sustained 200 ms
       - peak_temp: max tmp in the test
       - peak_speed_rpm: max |a|
       - vin_min: min Vin
    """
    # Find anchor times
    t_zero = None
    t_goto = None
    for t, cmd in cmd_marks:
        if cmd == "0" and t_zero is None:
            t_zero = t
        if cmd.startswith("goto") and t_goto is None:
            t_goto = t

    if t_goto is None:
        return None

    # Find end-of-move: first event after t_goto where tr == "-"
    t_move_end = None
    max_lag = 0.0
    for ev in events:
        if ev["t"] < t_goto:
            continue
        if ev.get("st") == "M":
            er = f(ev.get("er"))
            if er is not None and abs(er) > max_lag:
                max_lag = abs(er)
        if t_move_end is None and ev.get("tr") == "-" and ev["t"] > t_goto + 100:
            t_move_end = ev["t"]
            break

    # Post-move window: 2 s after end
    overshoot = 0.0    # max(p - target)
    undershoot = 0.0   # min(p - target)
    peak_temp = 0
    peak_speed = 0.0
    vin_min = None
    settling_time = None

    if t_move_end is not None:
        window_end = t_move_end + 2500
        last_in_band_start = None
        for ev in events:
            if ev["t"] < t_move_end - 200:
                continue
            tmp = f(ev.get("tmp"))
            if tmp is not None and tmp > peak_temp:
                peak_temp = tmp
            a = f(ev.get("a"))
            if a is not None and abs(a) > peak_speed:
                peak_speed = abs(a)
            v = f(ev.get("v"))
            if v is not None:
                vin_min = v if vin_min is None else min(vin_min, v)

            if ev["t"] < t_move_end:
                continue
            p = f(ev.get("p"))
            if p is None:
                continue
            delta = p - target_deg
            if delta > overshoot:
                overshoot = delta
            if delta < undershoot:
                undershoot = delta

            # settling detection: |delta| <= 0.5 sustained 200 ms
            if abs(delta) <= 0.5:
                if last_in_band_start is None:
                    last_in_band_start = ev["t"]
                elif (ev["t"] - last_in_band_start) >= 200 and settling_time is None:
                    settling_time = last_in_band_start - t_move_end
            else:
                last_in_band_start = None

            if ev["t"] >= window_end:
                break

    move_duration = (t_move_end - t_goto) if t_move_end else None

    return {
        "t_zero_ms": t_zero,
        "t_goto_ms": t_goto,
        "t_move_end_ms": t_move_end,
        "move_duration_ms": move_duration,
        "max_lag_during_move_deg": max_lag,
        "overshoot_deg": overshoot,
        "max_undershoot_deg": undershoot,
        "settling_time_ms": settling_time,  # may be None if it never settles in the window
        "peak_temp_C": peak_temp,
        "peak_speed_rpm": peak_speed,
        "vin_min_V": vin_min,
    }


# ---------- subcommand: setup ----------

EASING_NAMES = {"linear": 0, "in_out": 1, "ease_in": 2, "ease_out": 3,
                "0": 0, "1": 1, "2": 2, "3": 3}


def cmd_setup(args):
    if not bridge_alive():
        print("ERROR: bridge not running. start it first:", file=sys.stderr)
        print("  python tools/lab.py start  (with run_in_background)", file=sys.stderr)
        return 2

    # Resolve easing
    easing = EASING_NAMES.get(str(args.easing_curve).lower())
    if easing is None:
        print(f"ERROR: unknown easing '{args.easing_curve}'", file=sys.stderr)
        return 2

    # Release first so the motor doesn't fight us while we change params
    lab_send("x")
    time.sleep(0.2)

    # Apply each parameter (if specified). Skip None to leave motor's value alone.
    if args.kp is not None: lab_send(f"kp{args.kp}"); time.sleep(0.1)
    if args.ki is not None: lab_send(f"ki{args.ki}"); time.sleep(0.1)
    if args.kd is not None: lab_send(f"kd{args.kd}"); time.sleep(0.1)
    if args.mc is not None: lab_send(f"c{args.mc}"); time.sleep(0.1)
    if args.mt is not None: lab_send(f"mt{args.mt}"); time.sleep(0.1)
    if args.easing_curve is not None: lab_send(f"e{easing}"); time.sleep(0.1)
    lab_send("eon" if args.easing else "eoff"); time.sleep(0.1)

    stash = {
        "label": args.label,
        "kp": args.kp,
        "ki": args.ki,
        "kd": args.kd,
        "mc": args.mc,
        "mt": args.mt,
        "easing_curve": easing,
        "easing_on": bool(args.easing),
        "hold_zero_sec": args.hold_zero,
        "settle_sec": args.settle,
        "target_deg": 90.0,
        "ts_setup": time.time(),
    }
    Path(STASH).write_text(json.dumps(stash, indent=2))

    print()
    print(f"=== test1 setup: '{args.label}' ===")
    for k, v in stash.items():
        if k != "label" and k != "ts_setup":
            print(f"  {k:16s} = {v}")
    print()
    print("Motor is RELEASED. Please position the pointer at visual 0.")
    print("When ready, run:  test1.py run")
    return 0


# ---------- subcommand: run ----------

def cmd_run(args):
    if not bridge_alive():
        print("ERROR: bridge not running.", file=sys.stderr)
        return 2

    if not Path(STASH).exists():
        print("ERROR: no stash. run `test1.py setup` first.", file=sys.stderr)
        return 2

    stash = json.loads(Path(STASH).read_text())
    label = stash["label"]
    hold_zero = float(stash.get("hold_zero_sec") or 3.0)
    settle = float(stash.get("settle_sec") or 3.0)
    mt_ms = stash.get("mt")
    target = float(stash.get("target_deg", 90.0))

    # Approximate move duration: stash.mt if set, else assume 1500 ms
    move_ms = float(mt_ms) if mt_ms else 1500.0

    # Clear the log so this test's data is isolated
    lab("clear")

    print(f"=== test1 run: '{label}' ===")
    print(f"  pressing 0 ...")
    lab_send("0")
    time.sleep(0.4)  # let firmware print the "Zero set" line

    print(f"  holding zero for {hold_zero:.1f} s ...")
    time.sleep(hold_zero)

    print(f"  goto {target:.0f} ...")
    lab_send(f"goto{target:.0f}")

    wait_total = (move_ms / 1000.0) + settle
    print(f"  capturing for {wait_total:.1f} s (move {move_ms:.0f} ms + settle {settle:.1f} s) ...")
    time.sleep(wait_total)

    # Pull the log
    log_text = lab("log")

    # Save raw log
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    raw_path = RESULTS_DIR / f"test1_{label}.log"
    raw_path.write_text(log_text)

    # Parse and analyse
    events, cmd_marks = parse_log(log_text)
    metrics = compute_metrics(events, cmd_marks, target)

    print()
    print("--- metrics ---")
    if metrics is None:
        print("  (no goto command found in log — something went wrong)")
    else:
        for k, v in metrics.items():
            print(f"  {k:28s} = {v}")

    # Write to CSV history
    csv_exists = CSV_FILE.exists()
    fields = ["ts", "label",
              "kp", "ki", "kd", "mc", "mt",
              "easing_curve", "easing_on", "hold_zero_sec", "settle_sec",
              "target_deg",
              "move_duration_ms", "max_lag_during_move_deg",
              "overshoot_deg", "max_undershoot_deg",
              "settling_time_ms", "peak_temp_C", "peak_speed_rpm", "vin_min_V"]
    row = {"ts": time.strftime("%Y-%m-%d %H:%M:%S"),
           "label": label,
           "kp": stash.get("kp"),
           "ki": stash.get("ki"),
           "kd": stash.get("kd"),
           "mc": stash.get("mc"),
           "mt": stash.get("mt"),
           "easing_curve": stash.get("easing_curve"),
           "easing_on": stash.get("easing_on"),
           "hold_zero_sec": stash.get("hold_zero_sec"),
           "settle_sec": stash.get("settle_sec"),
           "target_deg": target}
    if metrics:
        for k in ("move_duration_ms", "max_lag_during_move_deg",
                  "overshoot_deg", "max_undershoot_deg",
                  "settling_time_ms", "peak_temp_C", "peak_speed_rpm",
                  "vin_min_V"):
            row[k] = metrics.get(k)

    with open(CSV_FILE, "a", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        if not csv_exists:
            w.writeheader()
        w.writerow(row)

    print()
    print(f"  raw log saved -> {raw_path}")
    print(f"  results row -> {CSV_FILE}")
    print()
    print("Motor is HELD at 90 deg. Use `test1.py release` between tests.")
    return 0


# ---------- subcommand: release ----------

def cmd_release(_args):
    if not bridge_alive():
        print("ERROR: bridge not running.", file=sys.stderr)
        return 2
    lab_send("x")
    print("Released.")
    return 0


# ---------- argparse ----------

def main(argv):
    p = argparse.ArgumentParser(prog="test1.py")
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("setup", help="apply params, release motor, prep")
    s.add_argument("--label", required=True, help="short identifier for this run")
    s.add_argument("--kp", type=int, default=None)
    s.add_argument("--ki", type=int, default=None)
    s.add_argument("--kd", type=int, default=None)
    s.add_argument("--mc", type=int, default=None, help="position max current (mA)")
    s.add_argument("--mt", type=int, default=None, help="move time (ms)")
    s.add_argument("--easing-curve", default="in_out",
                   help="linear|in_out|ease_in|ease_out (or 0-3)")
    s.add_argument("--easing", choices=["on", "off"], default="on")
    s.add_argument("--hold-zero", type=float, default=3.0,
                   help="seconds to hold at 0 before goto90")
    s.add_argument("--settle", type=float, default=3.0,
                   help="extra capture seconds after move duration")

    sub.add_parser("run", help="execute the test (after user positions pointer)")
    sub.add_parser("release", help="send `x`")

    args = p.parse_args(argv)
    # Convert easing on/off to bool
    if hasattr(args, "easing"):
        args.easing = (args.easing == "on")

    if args.cmd == "setup":
        return cmd_setup(args)
    if args.cmd == "run":
        return cmd_run(args)
    if args.cmd == "release":
        return cmd_release(args)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
