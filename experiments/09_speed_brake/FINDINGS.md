# 09_speed_brake — Findings

Validated 2026-05-04. Production-ready motion profile for the First Witness sculpture.

## What this code does

A 1800° (5-revolution) sculpture spin that:
- **Cruises in speed mode** so gravity contributes to the dynamics — the unbalanced pointer accelerates on the downswing and slows on the upswing. This is the "alive" visual feel borrowed from `03_motion_test`.
- **Brakes by switching to position mode at the target angle** — borrowed from `08_brake1080`. The position PID arrests the rotor and locks it on the start angle.
- Returns the rotor to **the exact angle where `g` was pressed** (within ~0.5° on average, regardless of starting position).

Three-phase sequence:
1. **SPIN_RAMP_UP** — speed mode, cosine ease 0 → 8 RPM over 2000 ms. Gravity contributes from the very first degree.
2. **SPIN_CRUISE** — speed mode, hold 8 RPM. Encoder polled every tick.
3. **SPIN_BRAKE** — at `(curEnc - startEnc) >= 1800° - 3°`, switch to position mode (PID first, then mode, then max-current, then setpoint). Position PID arrests the rotor on the target.
4. **SPIN_SETTLE** — 5 s position-mode hold. Final report printed.

## Why position mode would NOT work for the cruise

`07_smooth_1800` used the same cosine ease but in position mode throughout. Visually mechanical — the position PID stiffly cancels gravity. Same kinematic curve, dead feel. One line of code (`motorSetPosition` vs `motorSetSpeed`) determines whether the motor invites gravity in or fights it.

## Validation results

### Baseline runs (start from gravity rest)

| Run | Landing error |
|---|---|
| 1 | +0.80° |
| 2 | +1.00° |
| 3 | +0.70° |
| 4 (powered baseline) | -0.30° |

Mean magnitude: 0.70°.

### Multi-angle robustness (start from non-rest positions)

Tested with `j<deg>` jog command to position the pointer before `g`:

| Start offset from rest | Landing error |
|---|---|
| +45° | -0.10° |
| +90° | -0.51° |
| +135° | -0.10° |
| +180° | -0.20° |
| -90° | +0.20° |

**Mean magnitude across all 5: 0.22°. Range: -0.51° to +0.20°.**

The spin returns to wherever `g` was pressed, regardless of starting angle, even if:
- The rotor is in position-mode hold mid-correction when `g` fires
- Gravity drags the rotor *backward* during ramp-up (observed at j90: cruise marker showed `encMoved=-35.3°` before forward motion took over)
- Gravity *helps* the spin direction (observed at j-90: cruise marker `encMoved=+103.3°`)

The brake threshold fires off **accumulated encoder displacement**, not time, so it is angle-agnostic and immune to ramp-up dynamics.

## Known side-finding: jog hold is wobbly

The `j<deg>` command uses the same PID as the brake (kp=30M, ki=1k, kd=40M). These gains are tuned for arresting a moving rotor (transient), not for steady-state hold against gravity at lifted angles. Result: the rotor oscillates around the jog target instead of locking, and the JOG_SETTLE state typically times out at non-rest angles.

**This does not affect spin precision** — `g` captures the encoder reading at the moment it fires and uses that as the start. But if ever a stable hold at a specific angle is needed (e.g., for the choreography sequence), a separate hold-PID profile may be needed.

## Hardware caveat

The Roller485's `Vin` readback was observed to read 14.95 V both with and without the 15 V mains supply connected. The sensor appears unreliable as a power-state indicator. Confirm power state by visual inspection (LED, supply LED, motor torque feel during a spin), not by reading `Vin`.

Tested under proper 15 V mains. Behavior under USB-only 5 V power was much weaker — spins did not complete reliably and motion looked sluggish.

## Constants (current values)

```cpp
SPIN_TOTAL_DEG    = 1800.0  // 5 full revolutions
SPIN_CRUISE_RPM   = 8.0
SPIN_RAMP_MS      = 2000
BRAKE_LEAD_DEG    = 3.0     // brake fires at SPIN_TOTAL_DEG - this
BRAKE_MAX_MA      = 1500
BRAKE_PID_P       = 30000000
BRAKE_PID_I       = 1000
BRAKE_PID_D       = 40000000
SETTLE_MS         = 5000
JOG_SEEK_TOL_DEG       = 1.0
JOG_SETTLE_TOL_DEG     = 0.3
JOG_SETTLE_WIN_MS      = 1500
JOG_SEEK_TIMEOUT_MS    = 5000
JOG_SETTLE_TIMEOUT_MS  = 8000
```

## Serial commands

```
f         Release motor (free-spin). Pointer falls to gravity rest.
j<deg>    Jog by relative offset, hold via position mode.
            (Settle is wobbly at non-rest angles — see side-finding above.)
g         Speed-mode 1800° spin, position-mode brake at target.
x         Emergency stop
p         Print settings
```

## Lineage

- `03_motion_test` — original speed-mode cruise (gravity-alive, no stop)
- `07_smooth_1800` — position-mode 1800° spin (precise stop, mechanical feel)
- `08_brake1080` — position-mode 1080° spin with setpoint-freeze brake (validated stopping mechanism)
- **`09_speed_brake`** — hybrid: speed-mode cruise + position-mode brake at target. Combines the alive feel of 03 with the precise stop of 08.

Next iteration target: `17_witness_choreography_smooth` — integrate this motion profile into the 16_witness_choreography satellite-finding cycle.
