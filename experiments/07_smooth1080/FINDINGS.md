# 07_smooth1080 — Findings (2026-04-30)

Goal: 1080° rotation with 03-style smoothness, ending at the same physical
angle it started. Reserved as a working but not-great reference.

## Strategies tried

### A) Speed-mode rotation + position-mode lock at end (initial design)

**Behavior:** ramp 0→8 RPM, cruise terminating when traveled ≥ 1000°,
ramp 8→0 RPM, then `switchToPositionMode()` and command exact target =
start + 1080°.

**Result:** unreliable. Spin 1 from a benign starting orientation landed
within 0.4°. Spin 2 from a different starting orientation **overshot the
ramp-down by 245°** because gravity assisted the cruise and the loose
speed-PID let the rotor accelerate well past the commanded 8 RPM. The
position PID at kp=30M then over-corrected, settling 84.8° short of target.

**Why it failed:** loose speed-PID + unbalanced gravity-loaded pointer +
angle-based termination = unpredictable angular travel. The rotor's
actual speed during cruise depends on instantaneous gravity orientation,
and the loose PID does not fight excursions. The error accumulates over
1080° to tens of degrees.

### B) Position-mode throughout, with eased velocity setpoint (final design)

**Behavior:** `switchToPositionMode()` at the start; updateMotion()
integrates the existing eased `currentSpeedRPM` (0 → 8 → 0 RPM) into
`floatPosition`; the motor's position PID tracks that setpoint
deterministically. End point is mathematically `start + 1080°`.

**Result:** repeatable within **±0.5°** when the motor enters the spin
fully settled at its starting position. Across consecutive spins:
err = 0.00°, 0.01°, 0.50°. The 0.5° band is the position-PID's tracking
floor, validated independently by 04_position_lab.

**Caveat — `j<deg>` jog snap:** snapping `floatPosition` 146° in one tick
to position the pointer makes the position PID slam to the new setpoint.
The motor overshoots and integrator-winds; if `g` is issued before that
settles (~4 s wasn't enough in test), the spin starts with residual
oscillation and lands ~3° off (saw 3.3° on first j180+g, 0.5° on second
when motor had already settled). Easing the jog or extending the settle
delay would fix this, but wasn't pursued.

**Why this works:** the motor is following a *setpoint* that we control
deterministically, not a velocity that gravity perturbs. The rotor lags
the setpoint by ≤ 1° (PID tracking error) but the lag is bounded and
relative to a known reference. Snap the final setpoint exactly to
`start + 1080°` and the motor pulls there.

## Why "ok but not great"

Position-mode tracking eliminates the gravity-affected acceleration
through the rotation — the visible motion is a clean cosine velocity
profile, not a pendulum swing. 03's "rotor accelerates with gravity on
the descent, slows climbing back up" texture is gone. The motion is
smooth in the kinematic sense but mechanical-feeling, not alive.

## Reserved files

- `src/main.cpp` — position-mode-throughout 1080° spin via `g` command,
  jog-to-physical-angle via `j<deg>` command, plus all of 03's existing
  serial interface.
- `src/main.cpp:171–199` — spin state machine (RAMP_UP → CRUISE →
  RAMP_DOWN → LOCK).
- `tools/lab.py` — same serial bridge as 06.

## What to try next

Brake-based stop instead of decelerated stop. Keep 03's loose-PID
speed-mode rotation for the gravity swing; engage an active brake
mechanism at the right angle to halt the rotor. See `08_brake1080/`
when it exists.
