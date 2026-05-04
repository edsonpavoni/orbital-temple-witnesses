# Position-Control Recipe & Findings

Wall-mounted slightly-unbalanced pointer, M5Stack Unit-Roller485 Lite + XIAO ESP32-S3 over I²C, 15 V supply.
Tuning sessions: **2026-04-28 → 2026-04-29.**

---

## 1. The recipe (locked in)

For commanding any absolute angle from 0–360° with sub-degree precision, smooth motion, and low motor stress:

```
kP                = 30,000,000     // proportional gain
kI                = 1,000           // integral gain
kD                = 40,000,000      // derivative gain
mc (current cap)  = 1000 mA         // position-mode max current
easing            = EASE_IN_OUT     // cosine S-curve velocity profile
mt scaling        = 33.33 ms / deg of move distance
                  // 45° move  -> mt = 1500 ms
                  // 90° move  -> mt = 3000 ms
                  // 180° move -> mt = 6000 ms
                  // 360° move -> mt = 12 000 ms
```

### Validated performance envelope (two back-to-back test 3 runs, 32 moves total)

| Quantity | Value |
|---|---|
| Worst-case final error | **0.5°** |
| Mean \|final error\| | **0.21°** |
| Peak speed during move | 7.7 – 10.4 RPM |
| Peak motor temperature | 40 – 47 °C |
| Settling within ±0.5° after move-end | < 1 s in every case |
| Max overshoot ever observed | 1.1° |
| Max ringdown amplitude | 1.3° |
| Run-to-run reproducibility | within 0.2° |

This is **well inside the motor's own ±2° mechanical-alignment spec.** No reason to push harder.

---

## 2. The journey (how we got here)

### Test 1 — single-move discovery (interactive, hand-positioned between iterations)

Goal: drive a smooth 0 → 90° move that stops at 90°.

| Run | Change | Result |
|---|---|---|
| 1A | `mc=1000, mt=1500` from motor-flash PID | **Catastrophic.** 27° tracking lag during move, 28° overshoot, 44° undershoot in post-move pendulum oscillation |
| 1B | `mt=3000` (slowed trajectory) | Smooth move, no overshoot, **but motor stopped 8.2° short of 90°** and held steadily |
| 1C | + `kI=1000` | **No effect.** Steady-state error 8.0° |
| 1D | + `mc=1000` (instead of kI) | **No effect.** SS error 8.6° |

Diagnosis: at err = 8° with kP = 1.5M, the proportional output mapped to a current command (~200 mA) that was simply too small to overcome static friction + gravity at that angle. Neither raising the cap nor adding integral could help, because the actuator was *under-driven*, not saturated. **kP needed to rise.**

### Test 2 — autonomous parameter sweeps

Each sweep: calibrate zero by hand once, then run a plan file end-to-end with the safe-return parameters bringing the pointer back near zero between tests.

**kP sweep** at `mc=1000, mt=3000, kI=30, kD=40M`:

| Test | kP | final err | settled | rings | peak RPM |
|---|---:|---:|---:|---:|---:|
| 2A | 1.5M | −8.4° | no | 0 | 9.2 |
| 2B | 5M | −3.1° | no | 2 | 10.5 |
| 2C | 15M | −1.1° | no | 0 | 9.1 |
| **2D** | **30M** | **−0.5°** | **1608 ms** | **0** | **8.7** |

**kP refinement around 2D:**

| Test | what changed from 2D | final err | settled | rings |
|---|---|---:|---:|---:|
| 2E | mt=2000 (faster) | −0.5° | 1005 ms | 2 |
| 2F | mc=600 (less current) | −0.5° | 2211 ms | 6 |
| 2G | kP=60M | −0.6° | **never** | 9 |
| **2H** | **kI=1000** | **−0.4°** | **402 ms** | **0** |

Test 2H beat 2D by **4× on settling time** and zeroed the ringdown. This is the recipe.

### Test 3 — full-amplitude validation

Out-and-back move pairs at every 45° step from 45° → 360°, with `mt` scaled linearly with distance. Two independent runs (2026-04-28 23:55 + 2026-04-29 00:03).

All **32 moves landed within ±0.5°** of target. The asymmetric residual pattern (small-amp out-moves land short, large-amp out-moves land long, back-moves consistently land at +0.1°) is reproducible across runs and is the signature of the unbalanced pointer's gravity bias relative to the calibrated zero.

---

## 3. Insights (what we learned about this motor + pointer)

1. **Higher kP is *less* motor stress, not more.** Counter-intuitive but the data is clear. With low kP the motor lags through the trajectory then sprints to catch up — those sprints hit higher peak RPM and produce more abrupt torque demand than a tight-tracking high-kP controller. Test 2A (kP=1.5M) peaked at 9.2 RPM during catch-up; test 2D (kP=30M) tracked smoothly and peaked at only 8.7 RPM despite landing 16× more accurately.

2. **Increasing the current cap doesn't help if the controller isn't asking for the current.** Test 1C confirmed: at kP=1.5M, raising `mc` from 600 → 1000 mA changed *nothing* about the steady-state offset. The PID was commanding ~200 mA — the cap was never the limit. Once kP was tuned to actually demand high current, mc=1000 became necessary (test 2F showed mc=600 throttled performance at kP=30M).

3. **Integral action only matters once kP is in range.** With under-tuned kP (test 1C), kI=1000 produced *no improvement* — because the actuator was already too weak to overcome residual friction. With well-tuned kP (test 2H), kI=1000 produced a 4× improvement in settling time. Order of operations: get kP right, then add kI for the final fraction of a degree.

4. **There is an optimum kP, not a "more is better" gradient.** Test 2G at kP=60M was unstable (oscillating, did not settle). The pointer_v3 value (15M) was good; this sculpture wanted 30M.

5. **The gravity bias is structural, not noise.** The pattern of residual errors (small-amp out-moves slightly short, large-amp out-moves slightly long, all back-moves consistently +0.1°) is reproducible to within 0.2° across runs. It encodes where the unbalanced pointer's gravity rest sits relative to the user-defined zero. **Worth keeping in mind for the sensorless calibration: this signal is real and consistent.**

6. **`mt` scales linearly with distance to hold peak speed constant.** At 33.33 ms/deg, peak measured speed across all amplitudes stayed in the 7.7–10.4 RPM band, well within thermal/torque comfort.

7. **The 180° amplitude has the most ringdown** (~10 zero-crossings, ~1.3° amplitude, decays in 1 s). Makes sense: the pointer at 180° is at the gravitational antipode of the calibrated zero, so arrives with the most stored kinetic energy.

---

## 4. How to use the lab

### The bridge

`tools/lab.py` owns the serial port to the motor. It runs as one process, streams the firmware's compressed delta-log to `/tmp/lab_pos.log`, and forwards commands written to `/tmp/lab_pos.fifo`. Subcommands: `start | send | tail | log | clear | status | stop | run`. See `tools/README.md` for full reference.

### Three test rigs

| Tool | Purpose | Workflow |
|------|---------|----------|
| `test1.py` | Single-move bench, interactive | `setup` (apply params, release motor) → user positions pointer at 0 → `run` (zero, hold X s, goto90, capture, leave held at 90) |
| `test2.py` | Autonomous parameter sweeps | `calibrate` (release, user positions) → `go <plan.csv>` (loops through every row in the plan, returning to zero between tests via known-stable safe-return params) |
| `test3.py` | Full-amplitude validation | `calibrate` → `go` (out-and-back moves at every step from 45° to max, mt auto-scaled with distance) |

All three save raw per-move logs to `tools/results/` and append to a combined CSV.

### Required state of the firmware

The firmware in this folder (`src/main.cpp`) is `04_position_lab` — see top-level `README.md` for the command reference. It exposes every relevant tunable over serial. The bridge talks to it.

The motor-side flash retains PID and current-cap values across reboots, so anything written via `kp/ki/kd/c` commands persists until overwritten. **The recipe values above are not currently in flash** — apply them at the start of each session via the test scripts or by hand.

---

## 5. Files in this folder

```
04_position_lab/
├── platformio.ini
├── src/
│   └── main.cpp                  // firmware (position-mode lab)
├── README.md                     // firmware command + log-format reference
├── RECIPE-AND-FINDINGS.md        // this document
└── tools/
    ├── README.md                 // bridge + tooling reference
    ├── lab.py                    // serial bridge (FIFO + log)
    ├── test1.py                  // single-move interactive bench
    ├── test2.py                  // autonomous parameter-sweep loop
    ├── test3.py                  // full-amplitude validation
    ├── plans/
    │   ├── sweep_kp.csv
    │   ├── sweep_kp_refine.csv
    │   └── step_response.txt
    └── results/
        ├── test1_<label>.log     // raw per-move serial logs
        ├── test1_log.csv         // combined metrics
        ├── test2_<label>.log
        ├── test2_log.csv
        ├── test3_amp<deg>_<dir>.log
        └── test3_log.csv
```

---

## 6. Next adventure

Sensorless gravity-based homing: on power-up, use the unbalanced pointer's interaction with gravity (during a programmed rotation in both directions) to determine the absolute angular position automatically — no hand-calibration required. Discussed in the plan-mode session of 2026-04-29.
