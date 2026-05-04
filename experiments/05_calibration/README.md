# 05_calibration

Sensorless gravity-based homing for the wall-mounted unbalanced pointer
sculpture. On every power-up, the sculpture autonomously discovers its
absolute orientation and rotates to true visual vertical — no extra
sensors, no hand-positioning required.

**Validated 2026-04-29.** Power-cycle test from 4 random starts (0°, +11°,
−10°, +180°) gave landing errors of −4°, −5°, −3°, −4°. Three subsequent
power-cycle tests after the final offset adjustment landed at +2°, +1°, −2°
— **mean error ~0.3°, accuracy ±2° from any starting position**.

For the journey + design rationale see `FINDINGS.md`. This file is the
operating reference.

---

## What the firmware does

On every power-on:

1. **3 s pre-delay** so the user can step back from the sculpture.
2. **Latch encoder** — `enterHoldHere` records whatever angle the pointer
   happens to be at as user-frame zero. Doesn't matter what the angle is.
3. **Diag sweep** — 2 full revolutions CW, 1.5 s pause, 2 full revolutions
   CCW. The motor uses a deliberately loose PID profile so gravity-induced
   lag becomes visible in the current-draw signal. **Only the second
   revolution of each direction feeds the on-board sinusoid fit** — the
   first revolution is the motor breaking static friction and accelerating
   into steady-state spin (artifact-prone).
4. **Sinusoid fit** — solves a 3×3 linear least-squares for `cur(θ) =
   a·sin(θ) + b·cos(θ) + dc`. Phase `c = atan2(b, a)` gives gravity-up
   angle. The CW + CCW sums cancel friction and isolate the gravity
   component.
5. **Apply offset** — visual-up = gravity-up − `mass_offset_deg` (a
   per-pointer constant calibrated once, persisted in ESP32 NVS).
6. **Re-anchor zero + move** — set the new user-frame zero so it equals
   visual-up. Switch back to the production PID. Move the pointer to user 0.

Total ritual time ≈ 80 s. State after completion: motor holding at visual-up
within ±2°.

---

## Recipes

### Production (move-to-target after homing)

```
kp = 30,000,000     ki = 1,000      kd = 40,000,000
mc = 1000 mA        easing = EASE_IN_OUT
mt = 33.33 ms per deg of move distance
```

Discovered/validated in `04_position_lab` (test 2H + test 3 across
the full 0–360° range). Lands within ±0.5° of target with no overshoot.

### Diagnostic (calibration sweeps only)

```
kp = 1,500,000      ki = 30         kd = 40,000,000
mc = 1000 mA        easing = EASE_IN_OUT
mt = 50 ms per deg  (≈ 5 RPM avg trajectory speed)
```

Looser PID + slower sweep amplifies the gravity-induced lag so the
sinusoid fit has clean signal to work with.

### `mass_offset_deg` (this pointer's calibration constant)

```
10.83 deg     // persisted in ESP32 NVS
```

Recovered empirically: with a previous offset of 14.83°, four power-cycle
runs from random starts landed at a mean error of −4°. New offset =
14.83° − (−4°) = 10.83°.

To recalibrate from scratch (e.g., for a different pointer):
1. Hand-position pointer at true visual vertical (use a level/protractor).
2. `hold` → `diag` (research mode, doesn't apply offset).
3. The firmware prints `>> fit: gravity-up at user-frame X.XX deg`.
4. `setcal X.XX` then `savecal`.

---

## Commands (serial, 115200 8N1)

| Command | Effect |
|---------|--------|
| `release` | Disable motor output (free-coast) |
| `hold` | Latch current encoder as zero, hold in position mode |
| `diag` | Run 2-rev sweep without applying homing — research only |
| `home` | Latch + diag + apply offset + move to visual-up |
| `move <deg>` | Move to a user-frame angle (production recipe) |
| `report` | Live readbacks + current state |
| `cal` | Print current `mass_offset_deg` |
| `setcal <deg>` | Set `mass_offset_deg` in RAM only |
| `savecal` | Persist current `mass_offset_deg` to NVS |
| `loadcal` | Reload `mass_offset_deg` from NVS |

---

## Compressed delta log

Same scheme as `04_position_lab`, with two additions for this firmware:

| Key | Meaning |
|-----|---------|
| `t` | millis() — always present (anchors the line) |
| `ph` | Phase: `R` released / `H` holding / `M` moving / `C` diag-CW / `P` diag-pause / `A` diag-CCW |
| `Tg` | Logical target deg (user frame) |
| `sp` | Trajectory setpoint deg (user frame) |
| `p` | Encoder readback (user frame) |
| `er` | `sp − p` |
| `a` | Actual RPM |
| `cur` | Motor current draw (mA) — gated on output enabled |
| `tmp` | Motor temp (°C) — gated on output enabled |
| `v` | Vin (V) |

Lines emitted only when at least one field changes. Silence == "still the same."

---

## Tools

```
tools/
├── lab.py              # serial bridge (background process owning the port)
├── phase0_plot.py      # off-board diagnostic plot (Phase 0 dev tool)
├── homing_analysis.py  # off-board sinusoid-fit analyser (Phase 1 dev tool)
└── results/            # captured logs from validation runs
```

`lab.py` is the only tool needed in production. `phase0_plot.py` and
`homing_analysis.py` are kept for documentation of how the algorithm was
developed; they are not used at runtime.

### Running the bridge

```
python tools/lab.py start             # background bridge
python tools/lab.py events 15         # last 15 firmware messages + cmds
python tools/lab.py session           # current-session log only
python tools/lab.py send "<command>"  # forward a command
python tools/lab.py stop              # shut down cleanly
```

`events` and `session` are token-efficient: they only show the *current*
bridge session, not the whole log file. Use these instead of `log` for
routine inspection.

---

## Files

```
05_calibration/
├── platformio.ini
├── src/
│   └── main.cpp           # firmware
├── README.md              # this file
├── FINDINGS.md            # journey + design rationale
└── tools/
    ├── lab.py
    ├── phase0_plot.py
    ├── homing_analysis.py
    └── results/           # captured logs
```

## Hardware

- Seeed XIAO ESP32-S3
- M5Stack Unit-Roller485 Lite (BLDC + integrated FOC + magnetic encoder)
- I²C at `0x64` on `SDA = GPIO8`, `SCL = GPIO9`
- 15 V via USB-C PD trigger to the motor's PWR-485 input
- Wall-mounted slightly-unbalanced pointer, direct drive

## Next

To be merged with the production recipe and the smooth-rotation recipe
(from `03_motion_test`) into the final unified firmware that handles
calibration on boot, smooth rotations during the artwork, and precise
positioning — see the next iteration.
