# 03_motion_test

Smooth-rotation lab for the wall-mounted pointer sculpture. Forked from
`03_motion_test` on **2026-04-28** while iterating with the slightly-unbalanced
pointer mounted on the wall. Identical control logic — the fork only adds a
machine-readable delta log and an extended `p` print.

---

## What this code does

A serial-driven bench where you set a target speed (RPM) and watch the motor
ramp into it under a configurable acceleration curve. There is **no positional
target, no closed-loop angle control** — this is purely a "make the pointer
turn beautifully" tuning rig.

### Hardware

- **MCU:** Seeed XIAO ESP32-S3
- **Motor:** M5Stack Unit-Roller485 Lite (BLDC, integrated FOC, magnetic
  encoder, OLED, RGB) — controlled here via I²C at `0x64` on `SDA=GPIO8`,
  `SCL=GPIO9`
- **Power:** 15 V via USB-C PD trigger to the motor's PWR-485 input
- **Mechanical:** Direct drive (no gear reduction). Slightly unbalanced pointer
  on a horizontal axis — gravity is the dominant disturbance per revolution.

### Control loop (100 Hz)

Every 10 ms the firmware:

1. Reads serial, parses commands.
2. If a transition is active, interpolates `currentSpeedRPM` between
   `startSpeedRPM` and `targetSpeedRPM` using the chosen easing curve.
3. Sends the result to the motor as either:
   - **SPEED mode** — write commanded RPM to register `0x40`, motor's internal
     speed PID closes the loop.
   - **POSITION mode** — accumulate `currentSpeedRPM × dt` into a float angle
     and write the angle to register `0x80` every tick. Motor's internal
     position PID chases the slow-moving setpoint. Smoother at low RPM because
     the speed PID's quantization disappears.

### Easing curves available

| Mode | Formula | Feel |
|------|---------|------|
| `LINEAR` (0) | `t` | Constant accel, abrupt at endpoints |
| `EASE_IN_OUT` (1) | `0.5 · (1 − cos(t·π))` | Cosine S-curve, zero accel at both ends |
| `EASE_IN` (2) | `t²` | Slow start, fast end |
| `EASE_OUT` (3) | `1 − (1−t)²` | Fast start, slow end |

### Serial commands

| Cmd | Effect |
|-----|--------|
| `s<rpm>` | Set target speed (negative = reverse) |
| `c<mA>` | Max current (100–2000 mA) |
| `t<ms>` | Transition duration (100–30000 ms) |
| `e<0–3>` | Easing curve |
| `m` | Toggle SPEED ↔ POSITION mode |
| `kp<v>`, `ki<v>`, `kd<v>` | Write speed-PID gains |
| `r` | Read PID back from motor |
| `p` | Print all settings + live Vin / encoder pos / temp |
| `x` | Emergency stop |

### Compressed delta log

Every 200 ms, one line per tick. Format: `t=ms,key=value,...` — **only
fields that changed since the last printed line are emitted.** Silence means
"nothing has changed." Per-field baselines update only when a field actually
prints, so sub-epsilon drift accumulates against the last logged value rather
than disappearing.

| Key | Meaning | Notes |
|-----|---------|-------|
| `t` | millis() since boot | Always present, anchors the line |
| `m` | `S` (speed) or `P` (position) | |
| `T` | Target RPM | Set by `s<rpm>` |
| `c` | Commanded RPM | What the firmware is currently writing |
| `a` | Actual RPM (speed mode) | Read from motor encoder |
| `p` | Encoder position (deg) | Reg `0x90` ÷ 100 |
| `tr` | Transition % | `-` when idle |
| `tmp` | Motor temp (°C) | **Only when motor is moving or trying to** |
| `v` | Vin (V) | Reg `0x34` ÷ 100 |
| `mc`, `tt`, `e`, `kP`, `kI`, `kD` | Settings | Print on change only |

If a tick produces no deltas, **nothing prints** — not even the timestamp.

---

## What it accomplishes

This was the rig that locked in the recipe for "the pointer rotates
beautifully." The values now baked in as defaults (and persisted in motor
flash) are:

| Setting | Value | Why |
|---------|-------|-----|
| `maxCurrentMA` | **1000 mA** | Headroom to break static friction at low RPM. Thermals stable at ~45 °C for intermittent operation; at the upper edge of the motor's continuous spec (500 mA) but well within burst (1000 mA), so fine for ritual cycles with stops. |
| `transitionTimeMS` | **2000 ms** | Slow enough to read as graceful; fast enough to not feel sluggish. |
| `easingMode` | **EASE_IN_OUT** | Cosine S-curve. Zero acceleration at both endpoints, smooth bell-shaped velocity profile. |
| Speed PID (in motor flash) | **P=1.5e6, I=1000, D=4.0e7** | Loose loop. Near-zero integral lets the motor not fight gravity. High D damps oscillation without punishing disturbance. |

The minimum RPM that reliably breaks free of static friction with this
specific pointer balance turned out to be **8 RPM** (7 RPM produces a small
audible kick then stalls). Below that, the motor commands the speed but the
pointer never moves.

---

## Findings (2026-04-28 session)

### Why the motion looks beautiful — the actual recipe

Four things compose to make this work:

1. **Cosine S-curve velocity profile** — no jolt at start or end of the
   transition. The acceleration crests in the middle and is exactly zero at
   the endpoints.
2. **2000 ms transition window** — the rate of change is below human "twitch"
   perception. The pointer doesn't appear to *change* speed, it appears to
   *be in a different speed*.
3. **Loose PID, especially `I≈0`** — the motor does **not** aggressively chase
   steady-state error. When gravity pulls the unbalanced pointer off the
   commanded speed, the motor lets it happen. High D damps oscillation but
   doesn't punish the disturbance.
4. **Direct drive** — no gearbox backlash, no stiction breakthroughs.

The motor is acting as a **soft spring guiding direction**, not as a stiff
servo. Gravity + the unbalanced pointer provide the dynamics; the motor
provides the slow forcing function. The result reads as a *living swing*
rather than a robotic crawl.

### What this means for precise positioning

This same recipe will **fight you** when you try to stop at a specific angle:

- With near-zero integral and gravity dominating, the rotor coasts past
  commanded "slow down at target" — overshoot/undershoot 5–30° depending on
  where in the gravity cycle the stop is requested.
- During a deceleration that aligns with gravity pulling the heavy side
  down, the actual RPM **climbs** while the commanded RPM drops (observed in
  the log: `c=7.9 → 2.0`, `a=6.8 → 15.9` peak).

The smooth-rotation regime (this code) and the precise-stop regime are
**different problems with different solutions**. Don't change this config to
add precision — layer the precision logic on top as a separate phase, or
fork into a position-control lab (next file).

### Thermal observations

- Vin: 15.09 V steady, never moved by 0.05 V over 85 s of mixed-RPM operation.
- Motor temp: 43–46 °C steady throughout, no upward trend. The ±5 °C spread
  between adjacent 200 ms samples is sensor noise, not real thermal change.
- Continuous-current rating per the M5Stack docs is 500 mA; we're running
  1000 mA. For the ritual use case (rotate seconds, stop, rotate again) this
  is comfortable. For 24/7 continuous rotation we would want longer-window
  data.

### Delta-log behavior at rest

After `x` (emergency stop), the log goes nearly silent — only position dither
(`p=8793.8 ↔ 8793.9`, ±1 encoder count). No `tmp=`, no `a=`, no `tr=`. That's
the desired "silence = stillness" behavior; useful for spotting the moment
something starts changing.

---

## Reference values (from `p` output, 2026-04-28)

```
Control Mode:    SPEED
Target Speed:    0.00 RPM
Max Current:     1000 mA
Transition Time: 2000 ms
Easing Mode:     EASE_IN_OUT
P (kp): 1500000
I (ki): 1000
D (kd): 40000000
Vin:             15.09 V
Enc Position:    4792.70 deg   (cumulative — encoder doesn't wrap)
Motor Temp:      45 C          (already warm from prior testing)
```

The PID values live in the motor's flash (`saveConfigToFlash` was called in a
prior session). On boot, `setup()` reads them back via `motorReadSpeedPID()`,
so the C-side defaults in this file (`speedPID_P = 150000`, etc.) are
overridden at runtime and exist only as fallback values if the motor flash is
ever wiped.

---

## Files

- `platformio.ini` — XIAO ESP32-S3 target
- `src/main.cpp` — full firmware
- `README.md` — this file
