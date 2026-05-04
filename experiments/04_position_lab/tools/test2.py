#!/usr/bin/env python3
"""
test2.py — autonomous parameter sweep for the goto-90 move.

Workflow (one user interaction at the start, then fully automatic):

  1.  test2.py calibrate
      Releases the motor. User positions the pointer at visual 0 by hand.

  2.  user says "ready"

  3.  test2.py go <plan.csv>
      Sends `0` to latch the zero. Then for each row in the plan file:
        a. Applies the row's parameters.
        b. Sends goto90, captures the move + settle, parses metrics.
        c. Restores "safe return" parameters (test 1B's values: kp=1.5M,
           ki=30, kd=40M, mc=600, mt=3000, easing on cosine S-curve).
        d. Sends goto0 and waits ~6 s for the pointer to come back near zero.
      Finally sends `x` (release) and prints a summary table sorted by
      settling time (smaller = better).

  4.  test2.py release
      Manual release if needed.

The motor is never released between tests — that keeps the firmware's
zeroOffsetCounts valid for the whole sweep. The sweep accepts that the safe
return doesn't reach exactly 0 (test 1B showed an ~8 deg steady-state offset
under the safe PID), so each test starts from "approximately zero" rather
than "exactly zero". Each test's metrics are still relative to its actual
starting point, so the comparison between rows is fair.

Plan file format (CSV with header):

  label,kp,ki,kd,mc,mt,easing_curve,easing_on
  2A_kp1.5M,1500000,30,40000000,1000,3000,in_out,1
  2B_kp5M ,5000000,30,40000000,1000,3000,in_out,1

Blank cells -> leave that parameter at whatever the motor currently has.
easing_curve accepts 0-3 or linear|in_out|ease_in|ease_out.
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
CSV_FILE = RESULTS_DIR / "test2_log.csv"

PYTHON = sys.executable

# Safe return parameters (validated as stable in test 1B).
SAFE_RETURN = {
    "kp": 1500000,
    "ki": 30,
    "kd": 40000000,
    "mc": 600,
    "mt": 3000,
    "easing_curve": 1,
    "easing_on": True,
}

EASING_NAMES = {"linear": 0, "in_out": 1, "ease_in": 2, "ease_out": 3,
                "0": 0, "1": 1, "2": 2, "3": 3}


# ---------- bridge wrappers ----------

def lab(*args) -> str:
    return subprocess.check_output([PYTHON, LAB, *args], text=True)


def lab_send(text: str) -> None:
    subprocess.check_call([PYTHON, LAB, "send", text])


def bridge_alive() -> bool:
    return "RUNNING" in lab("status")


# ---------- log parser (shared with test1.py logic) ----------

def parse_log(text: str):
    events, cmd_marks = [], []
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
        for k in ("st", "Tg", "sp", "p", "er", "a", "tr",
                  "tmp", "v", "mc", "kP", "kI", "kD"):
            if k in state:
                snap[k] = state[k]
        events.append(snap)
    return events, cmd_marks


def f(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def metrics_for_move(events, cmd_marks, target: float, goto_marker: str = "goto90"):
    """Find the move (st=M segment) and analyse it. cmd_marks are unused but
    kept for API compatibility — the state transition non-M -> M is more
    reliable than command markers, which may have no timestamp when the log
    was cleared right before the command."""
    t_goto = None
    prev_st = None
    for ev in events:
        st = ev.get("st")
        if st == "M" and prev_st != "M":
            t_goto = ev["t"]   # take last such transition (overwrites)
        prev_st = st
    if t_goto is None:
        return None

    # find end of move: first event after t_goto with tr=='-' and st=='H'
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

    # window: 2.5 s after end
    window_end = t_end + 2500
    overshoot = 0.0
    undershoot = 0.0
    final_p = p_at_end
    final_t = t_end
    settling_ms = None
    last_in_band_start = None
    zero_crossings = 0  # of er around zero — proxy for ringdown count
    last_er_sign = None
    peak_speed_post = 0.0
    peak_temp = 0

    for ev in events:
        if ev["t"] < t_end:
            continue
        if ev["t"] > window_end:
            break
        p = f(ev.get("p"))
        a = f(ev.get("a"))
        tmp = f(ev.get("tmp"))
        if a is not None and abs(a) > peak_speed_post:
            peak_speed_post = abs(a)
        if tmp is not None and tmp > peak_temp:
            peak_temp = tmp
        if p is None:
            continue
        final_p = p
        final_t = ev["t"]
        delta = p - target
        if delta > overshoot:
            overshoot = delta
        if delta < undershoot:
            undershoot = delta
        # zero-crossing of er around 0 (i.e. p crosses target)
        sign = 1 if delta > 0 else (-1 if delta < 0 else 0)
        if last_er_sign is not None and sign != 0 and last_er_sign != 0 and sign != last_er_sign:
            zero_crossings += 1
        if sign != 0:
            last_er_sign = sign
        # settling: |delta| <= 0.5 sustained 200 ms
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
        "peak_post_speed_rpm": round(peak_speed_post, 1),
        "peak_temp_C": peak_temp,
    }


# ---------- parameter application ----------

def apply_params(p: dict, label: str = "set"):
    """Apply a parameter dict to the motor via lab.py send."""
    if "kp" in p and p["kp"] is not None:
        lab_send(f"kp{int(p['kp'])}")
        time.sleep(0.05)
    if "ki" in p and p["ki"] is not None:
        lab_send(f"ki{int(p['ki'])}")
        time.sleep(0.05)
    if "kd" in p and p["kd"] is not None:
        lab_send(f"kd{int(p['kd'])}")
        time.sleep(0.05)
    if "mc" in p and p["mc"] is not None:
        lab_send(f"c{int(p['mc'])}")
        time.sleep(0.05)
    if "mt" in p and p["mt"] is not None:
        lab_send(f"mt{int(p['mt'])}")
        time.sleep(0.05)
    if "easing_curve" in p and p["easing_curve"] is not None:
        lab_send(f"e{int(p['easing_curve'])}")
        time.sleep(0.05)
    if "easing_on" in p and p["easing_on"] is not None:
        lab_send("eon" if p["easing_on"] else "eoff")
        time.sleep(0.05)


# ---------- subcommands ----------

def cmd_calibrate(_args):
    if not bridge_alive():
        print("ERROR: bridge not running.", file=sys.stderr)
        return 2
    lab_send("x")
    time.sleep(0.2)
    print()
    print("=== test2 calibrate ===")
    print("Motor RELEASED. Position the pointer at visual 0 by hand.")
    print("When ready, run:  test2.py go <plan.csv>")
    return 0


def cmd_release(_args):
    if not bridge_alive():
        print("ERROR: bridge not running.", file=sys.stderr)
        return 2
    lab_send("x")
    print("Released.")
    return 0


def cmd_reparse(args):
    """Re-run metrics on the per-run log files saved during a previous `go`.
    Reads the same plan to know labels and parameters, prints a summary table.
    No motor activity."""
    plan = parse_plan(args.plan)
    target = 90.0
    summary_rows = []
    for row in plan:
        label = row.get("label")
        if not label:
            continue
        log_path = RESULTS_DIR / f"test2_{label}.log"
        if not log_path.exists():
            print(f"  (missing log for {label})")
            continue
        text = log_path.read_text()
        events, cmd_marks = parse_log(text)
        m = metrics_for_move(events, cmd_marks, target)
        out = {"label": label,
               "kp": row.get("kp"), "ki": row.get("ki"), "kd": row.get("kd"),
               "mc": row.get("mc"), "mt": row.get("mt"),
               "easing_curve": row.get("easing_curve"),
               "easing_on": row.get("easing_on"),
               **(m or {})}
        summary_rows.append(out)

    print()
    print("=== reparsed summary (sorted by |final_error_deg| ascending) ===")
    def sort_key(r):
        fe = r.get("final_error_deg")
        return abs(fe) if fe is not None else 999
    summary_rows.sort(key=sort_key)
    print(f"{'label':24s}  {'kp':>10s}  {'mt':>5s}  {'mc':>5s}  "
          f"{'lag':>6s}  {'over':>6s}  {'fin_err':>8s}  {'settle':>8s}  "
          f"{'rings':>5s}  {'peak_rpm':>8s}")
    for r in summary_rows:
        print(f"{str(r.get('label')):24s}  "
              f"{str(r.get('kp')):>10s}  "
              f"{str(r.get('mt')):>5s}  "
              f"{str(r.get('mc')):>5s}  "
              f"{str(r.get('max_lag_during_move_deg')):>6s}  "
              f"{str(r.get('overshoot_deg')):>6s}  "
              f"{str(r.get('final_error_deg')):>8s}  "
              f"{str(r.get('settling_ms')):>8s}  "
              f"{str(r.get('ringdown_zero_crossings')):>5s}  "
              f"{str(r.get('peak_speed_during_move_rpm')):>8s}")
    return 0


def parse_plan(path: str):
    rows = []
    with open(path, "r", encoding="utf-8") as f:
        rdr = csv.DictReader(f)
        for raw in rdr:
            cleaned = {}
            for k, v in raw.items():
                if v is None:
                    cleaned[k] = None
                    continue
                v = v.strip()
                if v == "":
                    cleaned[k] = None
                    continue
                if k in ("kp", "ki", "kd", "mc", "mt"):
                    cleaned[k] = int(v)
                elif k == "easing_curve":
                    cleaned[k] = EASING_NAMES.get(v.lower())
                    if cleaned[k] is None:
                        raise ValueError(f"bad easing_curve in plan: {v}")
                elif k == "easing_on":
                    cleaned[k] = (v.lower() in ("1", "true", "yes", "on"))
                else:
                    cleaned[k] = v
            rows.append(cleaned)
    return rows


def cmd_go(args):
    if not bridge_alive():
        print("ERROR: bridge not running.", file=sys.stderr)
        return 2

    plan = parse_plan(args.plan)
    if not plan:
        print("plan is empty.", file=sys.stderr)
        return 2

    target = 90.0
    settle_after_test_sec = 4.0
    settle_after_return_sec = 5.0

    print()
    print(f"=== test2 go: {len(plan)} runs from {args.plan} ===")
    print("Setting zero from current pointer position...")
    lab("clear")
    lab_send("0")
    time.sleep(0.5)

    summary_rows = []

    for i, row in enumerate(plan, 1):
        label = row.get("label") or f"run{i}"
        print()
        print(f"--- run {i}/{len(plan)}: {label} ---")
        for k in ("kp", "ki", "kd", "mc", "mt", "easing_curve", "easing_on"):
            print(f"   {k:14s} = {row.get(k)}")

        # Apply test params
        apply_params(row, label=label)
        time.sleep(0.3)

        # Clear log so this test's data is isolated
        lab("clear")

        # Run the move
        mt = int(row.get("mt") or SAFE_RETURN["mt"])
        lab_send(f"goto{int(target)}")
        wait_total = (mt / 1000.0) + settle_after_test_sec
        print(f"   capturing for {wait_total:.1f} s ...")
        time.sleep(wait_total)

        log_text = lab("log")
        events, cmd_marks = parse_log(log_text)
        m = metrics_for_move(events, cmd_marks, target, goto_marker=f"goto{int(target)}")

        # Save raw log
        RESULTS_DIR.mkdir(parents=True, exist_ok=True)
        (RESULTS_DIR / f"test2_{label}.log").write_text(log_text)

        if m is None:
            print("   (parser found no goto in log!)")
            metrics = {}
        else:
            print("   metrics:")
            for k, v in m.items():
                print(f"      {k:28s} = {v}")
            metrics = m

        out = {"label": label,
               "kp": row.get("kp"), "ki": row.get("ki"), "kd": row.get("kd"),
               "mc": row.get("mc"), "mt": row.get("mt"),
               "easing_curve": row.get("easing_curve"),
               "easing_on": row.get("easing_on"),
               **metrics}
        summary_rows.append(out)

        # Restore safe return parameters and go back to zero
        print("   returning to zero with safe params...")
        apply_params(SAFE_RETURN, label="safe_return")
        time.sleep(0.3)
        lab_send(f"goto0")
        time.sleep((SAFE_RETURN["mt"] / 1000.0) + settle_after_return_sec)

    # End of plan: release
    print()
    print("--- plan complete: releasing motor ---")
    lab_send("x")

    # Append to combined CSV
    fields = ["ts", "label",
              "kp", "ki", "kd", "mc", "mt", "easing_curve", "easing_on",
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
        for row in summary_rows:
            w.writerow({"ts": time.strftime("%Y-%m-%d %H:%M:%S"), **row})

    # Print sorted summary
    print()
    print("=== summary (sorted by |final_error_deg| ascending) ===")
    def sort_key(r):
        fe = r.get("final_error_deg")
        return abs(fe) if fe is not None else 999
    summary_rows.sort(key=sort_key)
    print(f"{'label':18s}  {'kp':>10s}  {'mt':>5s}  {'mc':>5s}  "
          f"{'lag':>6s}  {'over':>6s}  {'fin_err':>8s}  {'settle':>8s}  "
          f"{'rings':>5s}  {'peak_rpm':>8s}")
    for r in summary_rows:
        print(f"{str(r.get('label')):18s}  "
              f"{str(r.get('kp')):>10s}  "
              f"{str(r.get('mt')):>5s}  "
              f"{str(r.get('mc')):>5s}  "
              f"{str(r.get('max_lag_during_move_deg')):>6s}  "
              f"{str(r.get('overshoot_deg')):>6s}  "
              f"{str(r.get('final_error_deg')):>8s}  "
              f"{str(r.get('settling_ms')):>8s}  "
              f"{str(r.get('ringdown_zero_crossings')):>5s}  "
              f"{str(r.get('peak_speed_during_move_rpm')):>8s}")
    print()
    print(f"Per-run logs: {RESULTS_DIR}/test2_<label>.log")
    print(f"CSV: {CSV_FILE}")
    return 0


# ---------- argparse ----------

def main(argv):
    p = argparse.ArgumentParser(prog="test2.py")
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("calibrate", help="release motor, prompt user to position at 0")

    s = sub.add_parser("go", help="run a plan file end-to-end")
    s.add_argument("plan", help="CSV plan file")

    sub.add_parser("release", help="send `x`")

    s = sub.add_parser("reparse",
                       help="re-run metrics on saved per-run logs (no motor activity)")
    s.add_argument("plan", help="same plan CSV used in the go run")

    args = p.parse_args(argv)
    if args.cmd == "calibrate":
        return cmd_calibrate(args)
    if args.cmd == "go":
        return cmd_go(args)
    if args.cmd == "release":
        return cmd_release(args)
    if args.cmd == "reparse":
        return cmd_reparse(args)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
