# First Witness — Knowledge

What was learned building the firmware for the wall-mounted unbalanced
pointer sculpture. This document consolidates the design journey from
03 through 09 (the final unified firmware), the algorithms, the recipes,
and the lessons that aren't obvious from reading the code.

---

## TL;DR — what the final design does and why

On every power-on the sculpture autonomously:

1. **Calibrates** its absolute orientation using a sensorless gravity
   technique — runs 2 slow revolutions in each direction, detects the
   sinusoidal gravity signature in motor current, fits a sinusoid, and
   recovers the gravity-up direction with ±2° accuracy.
2. **Re-anchors** its working zero so that "user-frame 0" = visual
   vertical.
3. **Spins** 1080° (3 full revolutions) in position-mode rate-integrated
   control at 8 RPM.
4. **Brakes** by freezing the position setpoint at exactly the start
   angle while the rotor is still moving. The motor's stiff position
   PID arrests the residual kinetic energy in ≈ 1° of overshoot.
5. **Holds** indefinitely at visual zero.

The deterministic stop angle is the entire point. Earlier attempts that
used the more "alive" gravity-affected speed-mode rotation gave
unpredictable end angles (drift up to tens of degrees per spin). This
was incompatible with "the pointer always returns to visual zero."

---

## The journey, in compressed form

### `03_motion_test` — discovery of the swing aesthetic

A serial-driven manual lab. Edson tuned a deliberately *loose* speed PID
(P=1.5M, I=1k, D=40M) that lets gravity dominate motor velocity. With
the unbalanced pointer this produced a "beautiful" pendulum-swing
rotation: rotor accelerates downhill, slows uphill, the motion reads as
*alive* rather than mechanical.

This recipe lives in motor flash; later firmwares write it back
defensively in case the flash gets overwritten by tuning sessions.

### `04_position_lab` — production position PID

A different problem: hit a specific angle accurately. Stiff position PID
(kp=30M, ki=1k, kd=40M) at 1000 mA validated to ±0.5° from any starting
angle across full 0–360°. This is the "production recipe" — used now
for every move-to-target operation.

### `05_calibration` — sensorless gravity homing

Discovers visual vertical without any extra sensors or hand-positioning.
Two physics insights:

- The unbalanced pointer's mass center is offset from the rotation
  axis. Gravity exerts a position-dependent torque
  τ_gravity(θ) = -m·g·r·sin(θ_mass). The motor's current draw under
  steady rotation traces this sinusoid.
- Friction is direction-anti-symmetric (always opposes motion);
  gravity is direction-symmetric. A 2-direction sweep, summed,
  cancels friction and isolates gravity.

The fit is a 3-parameter linear least squares for
`cur(θ) = a·sin(θ) + b·cos(θ) + dc`. Phase `c = atan2(b, a)` gives
gravity-up; the signed angle from gravity-up to visual-up is the
**`mass_offset_deg`** constant — a fixed mechanical property of the
specific pointer. Calibrate once by hand, persist in ESP32 NVS.

Validated ±2° from any starting position across 3 power-cycle tests
on 2026-04-29.

### `06_mix` — autonomous random-target cycle

A first attempt at the artwork's eternal cycle. Picked random angles,
moved to them precisely (Mode 1 = production PID), then did smooth
360° rotations (Mode 2 = loose speed PID). Discovered two important
things during integration:

- **Power gate.** The XIAO ESP32-S3 boots fine on USB 5 V, but the
  motor on PWR-485 needs the full 15 V to break static friction with
  this pointer. At 11 V (PD trigger half-negotiated), the motor's
  commanded RPM is right but the actual RPM is ~0 — the motor lacks
  torque. The firmware now waits for `Vin >= 15 V` before engaging.
- **Loose speed PID + gravity = unpredictable angular travel.** What
  produces the beautiful swing in 03 produces wildly variable rotation
  amounts in a long sweep — gravity-assisted descents drove the rotor
  past 19 RPM during cruise commanded at 8 RPM, while uphill climbs
  stalled. End angle drifted by tens of degrees per spin.

### `07_smooth1080` — first deterministic 1080°

To land deterministically, abandoned speed-mode for cruise. Instead:
**position-mode rate-integrated control**. A software loop integrates
the eased velocity setpoint into a slowly-advancing position setpoint
at exactly 8 RPM. The motor's position PID tracks that setpoint tightly.
End angle is mathematically `start + 1080°`. Lands within ±0.5°.

Reserved as a reference (`07_smooth1080/FINDINGS.md`) — Edson
felt the smooth eased ramp-down was "ok but not great." Wanted a more
deliberate-looking stop.

### `08_brake1080` — brake instead of decelerate

Same position-mode rate-integrated cruise as 07, but the velocity
*doesn't* ramp down at the end. While the motor is still cruising at
full 8 RPM, the position setpoint suddenly *freezes* at `start + 1080°`.
The position PID's reverse current (with bumped 1500 mA cap for
authority) arrests the rotor's kinetic energy in ≈ 1° of travel.
Visually a definite stop, not a fade.

Validated 2026-04-30 across 10 random-angle tests: max 1.7° error,
mean 0.68°, 8/10 within ±1°. Temperature flat at 48–51 °C across a
7-minute run — the controlled cruise draws much less current than the
gravity-runaway speed-mode cruise (which would have thermal-limited).

### `09_witness` — calibrate + spin (this firmware)

Merger of 05's calibration with 08's spin. Boot ritual = 05's full
homing ending with a move to visual zero, then auto-trigger 08's
1080° spin which brakes at start (= visual zero).

Validated 2026-04-30: end-to-end ritual lands within 0.3° of visual
zero across multiple runs. Mass-offset re-calibrated to 21.74° after
re-mounting (was 10.83° in 05's session; the offset is mechanical and
shifts when the pointer geometry changes).

---

## The two control regimes — when to use which

| Regime | Use for | PID | Quality |
|---|---|---|---|
| **Position-mode, motor's eased trajectory** | Move-to-angle (homing's final move) | Production (kp=30M) | ±0.5° accurate, smooth |
| **Position-mode, software rate-integrated** | The 1080° spin's cruise | Production | Deterministic angular travel; stiff, mechanical-feeling motion |
| **Position-mode, snap-and-brake** | The spin's stop | Production (1500 mA brake cap) | ≤ 1° overshoot, decisive halt |
| **Speed-mode, looser PID + slower trajectory** | Gravity diagnostic sweep | Diagnostic (kp=1.5M) | Gravity lag legible in current; not for production |
| **Speed-mode, loose PID** | (rejected) gravity-affected free swing | Loose flash PID | "Alive" but unpredictable angular travel; reserved in 03 only |

The artwork uses the first three regimes. The last is for the
calibration sweep (5 RPM avg, slow enough to expose gravity in the
current signal). The speed-mode loose-PID free swing is a beautiful
behavior but fundamentally incompatible with "lands at a deterministic
end angle" — it's reserved as 03 for any future artwork that wants the
swing aesthetic without the precision constraint.

---

## Why position-mode rate-integration kills the gravity swing

The unbalanced pointer creates angle-dependent gravity torque. In
loose speed mode the motor lets the rotor accelerate downhill (peak
~19 RPM during 8 RPM cruise) and stall climbing back up. This is the
"beautiful" 03 swing.

In position-mode rate-integration the *setpoint* moves at exactly 8 RPM.
Whenever the rotor lags behind the setpoint, the position PID outputs
current proportional to lag × kp (= 30M × lag). With 1A of current
budget it can apply enough torque to drag the rotor along the setpoint
even uphill. The result: rotor velocity stays close to 8 RPM (we
measured 6.8–7.9 RPM at brake-fire across 10 random starts), gravity
ripple is bounded to a degree or two of tracking lag, and the motion
*looks smooth* but not gravity-affected.

Trade-off is intentional. Deterministic end angle won out.

---

## The mass_offset constant — why it must be re-calibrated after re-mounting

The mass offset is the angle between gravity-up (where the heavy mass
naturally settles when straight up against gravity, the *unstable*
equilibrium) and the visible pointer-tip-up direction. It's a fixed
property of the pointer's mass distribution.

But it depends on **how the pointer is mounted on the motor shaft**.
If the pointer is rotated by even a few degrees during re-installation
(or replaced with a slightly different geometry), the offset changes.

Symptoms when stale:
- After the boot ritual, pointer lands visually offset (e.g., +10°).
- The fit accuracy is fine — `phys-from-zero` reads near 0 — but
  "zero" no longer corresponds to visual vertical.

Recovery is the recalibration procedure documented in README.md:
release → hand-position at true visual vertical → hold → diag → setcal
the printed value → savecal. Takes 90 seconds.

History on this sculpture:
- 2026-04-29 (05's session): `mass_offset_deg = 10.83°` after pointer
  initially mounted.
- 2026-04-30 (09's session, after pointer re-mount): `mass_offset_deg = 21.74°`.
  Stored in NVS, used by every subsequent power-on.

---

## Recipes (the validated constants)

```
PRODUCTION POSITION PID (used for moves + spin cruise + brake)
  kp = 30,000,000      ki = 1,000        kd = 40,000,000
  mc = 1,000 mA (cruise) / 1,500 mA (brake — bumped for authority)
  easing = EASE_IN_OUT cosine S-curve
  mt = 33.33 ms per deg of move distance

DIAGNOSTIC POSITION PID (used during calibration sweep only)
  kp = 1,500,000       ki = 30           kd = 40,000,000
  mc = 1,000 mA
  easing = EASE_IN_OUT
  mt = 50 ms/deg (~5 RPM average)

SPIN PARAMETERS
  total_deg          = 1080.0  (3 full revolutions)
  cruise_rpm         =    8.0  (setpoint advance rate)
  ramp_ms            = 2000    (cosine ease 0 → 8 RPM)
  brake_grace_ms     =  300    (after brake fires)
  settle_ms          = 2000    (lock hold before reporting)
  brake_max_ma       = 1500    (bumped from 1000 for brake authority)

CALIBRATION
  diag_revs/dir      =  2.0
  diag_pause_ms      = 1500
  fit_skip_fraction  =  0.5    (skip first rev — friction breakthrough)
  mass_offset_deg    = 21.74   (current; persisted in NVS)

POWER GATE (validated in 06; firmware checks Vin)
  min_vin_v          = 15.0    (motor lacks torque below this)
```

---

## Validated performance (2026-04-30)

End-to-end boot ritual:

| Run | phys-from-zero | Notes |
|-----|---|---|
| Initial home + spin from random | 0.21° | First validation |
| 5 spins back-to-back (no re-home) | 0.01°, 0.21°, 0.21°, 0.11°, 0.21° | Mean abs 0.15° |
| Re-home after `setcal 21.74` | 0.26° | Full ritual after recalibration |

Repeatable to ≤ 0.3° across the conditions tested. Motor temp stable
46–47 °C during a 5-spin sequence (~3 minutes of motion).

---

## What lives in `documentation/motor-roller485-lite/`

The motor's authoritative reference — register map, electrical limits,
RS-485 / I²C protocols, library API, schematics. The firmware's
register addresses (`0x80` POSITION, `0xA0` POSITION_PID, `0xC0`
CURRENT_READ, etc.) come from there. When debugging a register-level
issue, that's the source of truth.

Critical limits (from there):
- Max input voltage 16 V — exceed → `E:1 Over Voltage`, motor disabled.
- Max continuous phase current 0.5 A — at 1 A is short-term burst,
  validated stable for ritual cycles. 1.5 A bursts during the brake
  phase are rare and brief enough to not thermal-limit (validated by
  the temperature data above).
- Encoder scaling: 36000 counts = 360°.
- Absolute mechanical alignment ±2° — this is the floor of the
  calibration's accuracy, not a software issue.

---

## Things to keep in mind

- The PD trigger / 15 V brick is a physical dependency. If anyone
  swaps cables and the motor reverts to 5 V, the homing sweep will
  not move and the firmware won't progress past the diag phase. The
  `Vin` field in the delta log is the canonical check.
- Stall protection is **disabled** at boot (`writeReg8(REG_STALL_PROT, 0)`)
  so the diag sweep doesn't trip a false "jam" alarm during the slow
  rotation under load. Don't re-enable it without re-validating.
- The `mass_offset_deg` default in `main.cpp` is now **21.74**. NVS
  takes precedence; the default only kicks in if NVS is wiped.
- The artwork is one autonomous boot sequence. There is no eternal
  loop — the sculpture does the ritual once on power-up and then
  holds. To repeat, power-cycle, or send `home` over serial.

---

## File map across the series

```
code/
├── 03_motion_test/      # gravity-swing aesthetic recipe
├── 04_position_lab/     # production position-PID recipe
├── 05_calibration/      # sensorless homing algorithm
├── 06_mix/              # eternal-cycle scaffold; voltage gate
├── 07_smooth1080/       # smooth eased 1080° (reserved, RESERVED)
├── 08_brake1080/        # brake-style 1080° (the chosen motion)
├── 09_witness/          # ★ FINAL ★ calibration + brake-1080° united
└── documentation/motor-roller485-lite/   # authoritative motor reference
```

Each predecessor's README/FINDINGS captures what was learned at that
step. 09's `KNOWLEDGE.md` (this file) is the consolidated synthesis.
