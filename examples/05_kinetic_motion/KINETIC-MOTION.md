# Kinetic Motion · task group

**Purpose:** find the motion language for the Witness sculptures. Everything
about how the arm moves — deadband, direction, ritual choreography, the gravity-
rest calibration — lives here. No WiFi, no HTTP, no schedule. Pure motor + serial.

**Hardware:** same XIAO ESP32-S3 + M5 RollerCAN BLDC + pointer arm as the rest
of the project. Uploading this firmware temporarily replaces `04_idea3_hybrid`
on the Witness. Revert by re-flashing `04_idea3_hybrid` when done.

**How we work here:**
1. Upload this firmware once.
2. Open a serial monitor at 115200.
3. Type commands. Each one runs a small routine we can observe, tune, and log.
4. Write down what we learn at the bottom of this file (**Results log**).

---

## Open questions to answer

1. **Deadband.** What is the minimum RPM at which the motor reliably moves the
   arm? Below that threshold commanded speeds become "just not moving." We
   suspect this is why slow BREATH looks still.
2. **Gravity-rest zero.** Does the sculpture's mass imbalance produce a stable
   rest angle when the motor is released? If yes, we can auto-calibrate with
   no sensor — spin hard, release, wait for settle, mark zero.
3. **Spin-down behaviour.** After release from high RPM, how long does it take
   to settle? Does it oscillate before settling, or damp smoothly?
4. **Direction convention.** Is positive RPM clockwise or counter-clockwise as
   seen from the front of the sculpture? Must match the SVG + the physics.
5. **The right ritual.** What motion earns the word "ritual"? Not just "spin
   fast then slow" — something that reads as intentional, ceremonial. We
   discover this by trying variations and choosing.

---

## Routines to build, in order of priority

### 1 · `deadband` sweep *(diagnose the slow-motion problem)*
Start at 0.5 RPM. Step up by 0.5 RPM every 2 s. Print the commanded RPM and
watch for the first RPM at which the arm visibly moves. Stop on Enter.

### 2 · `calibrate` *(gravity-rest auto-zero)*
   a. Spin at a high RPM (say 80 RPM) for 4 full revolutions.
   b. Set speed 0 (but motor still active) for 0.5 s — removes commanded drive.
   c. Release motor output (REG_OUTPUT=0) — arm now free.
   d. Wait 15 s for the arm to settle at its gravity-rest angle.
   e. Re-enable motor. Read encoder. That's zero. Store in RAM.
   f. Print: "Zero is at X steps (Y°). All future commands reference this."

### 3 · `go <deg>` *(absolute position move)*
With zero calibrated, drive the arm to an absolute angle using speed-mode
plus a short feed-forward ramp. (We're avoiding RollerCAN position mode
per tonight's decision.)

### 4 · `breath <amp_deg> <period_s>` *(continuous sweep from current position)*
Same math as 04_idea3_hybrid but parameterised. Lets us find the smallest
amplitude that still looks like motion.

### 5 · `ritual <revs> <duration_s>` *(the big gesture)*
Sine-velocity integral. Ends exactly back at start. Tune duration and peak
until it looks right.

### 6 · `settle` *(end-of-performance release)*
Stop drive, release motor, let it settle. Report the final rest angle. This
is also a sanity check — if calibration is good, settle angle ≈ 0°.

---

## Serial command surface

All commands are one line, newline-terminated. Case-insensitive. Unknown
commands print `?`.

```
help                         list commands
info                         state, angle, last-commanded RPM, uptime
stop                         speed = 0, motor still active
release                      motor output disabled (free swing)
enable                       motor output enabled
rpm <n>                      set speed directly, n can be negative
spin <revs> <seconds>        smooth sine-velocity spin, returns to start
deadband                     sweep from 0.5 RPM up, find minimum movable
calibrate                    gravity-rest auto-zero routine
go <deg>                     move to absolute angle (requires calibrate first)
breath <amp_deg> <period_s>  start continuous sweep, type 'stop' to end
ritual <revs> <seconds>      perform one ritual gesture
settle                       release and wait, report final angle
```

Typing **any key** during a long routine aborts it (stops motor, waits).

---

## Results log

### 2026-04-15 · Deadband test (Witness 01)

Command: `deadband 5 1 17`. One revolution's worth of time per RPM step,
release 2 s, measure actual travel.

```
  rpm   expect_deg  actual_deg  ratio   verdict
  ----  ----------  ----------  ------  -------
   5.0       360.0         4.7    0.01  DEAD
   6.0       360.0        -7.4   -0.02  DEAD
   7.0       360.0         2.5    0.01  DEAD
   8.0       360.0        -9.3   -0.03  DEAD
   9.0       360.0         2.7    0.01  DEAD
  10.0       360.0       -21.9   -0.06  weak
  11.0       360.0       374.6    1.04  OK
  12.0       360.0       369.1    1.03  OK
  13.0       360.0       353.0    0.98  OK
  14.0       360.0       345.7    0.96  OK
  15.0       360.0       364.8    1.01  OK
  16.0       360.0       351.7    0.98  OK
  17.0       360.0       361.7    1.00  OK
```

**Key finding:** the motor is **bimodal**. Between 10 and 11 RPM it flips
from stuck to fully operational. Below 11, commands register but the
rotor can't overcome cogging + arm moment + stiction. At 11 and above,
ratio settles around 1.0 ±5% — the motor does what it's told.

**Consequence for v1 BREATH (±15°, 7 s period):**
peak angular velocity = 2π·15°/7s = 13.5°/s = **2.25 RPM** — far below floor.
v1 BREATH could not have worked at any commanded speed that small.

**Design implication:** any "slow" motion has to either
  (a) use peak RPMs above ~11, OR
  (b) use short pulses above 11 RPM separated by motor-off pauses
      (gravity-released phases read as stillness), OR
  (c) command ritual-style bursts with damped release instead of sustained
      slow drive.

---

### 2026-04-15 · Fine-grained deadband (Witness 01)

Command: `deadband 10 0.2 12`.

```
  rpm   expect_deg  actual_deg  ratio   verdict
  ----  ----------  ----------  ------  -------
  10.0       360.0       441.8    1.23  overshoot
  10.2       360.0       246.4    0.68  partial
  10.4       360.0       402.2    1.12  OK
  10.6       360.0       326.2    0.91  OK
  10.8       360.0       375.6    1.04  OK
```

**Reinterpretation of the bimodal finding:** the threshold isn't sharp. At 10
RPM the first sweep saw −21.9° (stuck); this sweep saw +441.8° (overshoot).
The difference is **starting arm position**. Between runs the motor was
released, so the arm settled somewhere — which either helped or hindered
the next spin-up. The real regime map:

- **< 10 RPM**: deterministically dead.
- **10–11 RPM**: unstable, outcome depends on starting angle relative to
  cogging detents + gravity assist. Don't design motion here.
- **≥ 11 RPM**: deterministic, ratio ≈ 1.0 ± 5%.

**Practical design floor: 12 RPM.** Above this, motion is predictable.

**Indirect confirmation of gravity-rest idea:** the fact that starting
position materially affects whether 10 RPM succeeds means the arm has a
real, gravity-driven preferred orientation. The calibration routine
should work.

---

### 2026-04-15 · Calibration trials (Witness 01)

Method: 4 revs at 80 RPM → release → settle 15 s → mark zero.

| Run | Settled angle | Notes |
|---|---|---|
| 1 | 13.40° | damped by 7s |
| 2 | 20.60° | damped by 7s |
| 3 | 17.80° | damped by 7s |

**Mean 17.27°, spread 7.2° (max pair).** Repeatable within a few degrees.
Physics confirmed: the arm is mass-asymmetric, pendulum-oscillates for
3–5 s after release, then damps to a gravity-rest angle.

**Rules learned from the settling traces:**
- First few readings are mid-oscillation — ignore them.
- 7 seconds of settle is enough; 15 s is overkill. Set to **10 s**.
- Read the encoder BEFORE re-enabling motor output; the re-enable causes
  a ~1° kick (observable as `settled 13.40° → recorded 14.60°`). Fixed.

**This is the sculpture's zero.** Every Witness will have its own physical
rest angle (depends on exact mass distribution), but each has exactly ONE.
That's all the calibration needs to be deterministic.

### 2026-04-15 · Calibration at 40 RPM — FAILED reliability

Same procedure, halved spin energy (40 RPM, 4 revs, 6 s spin):

| Run | Settled angle |
|---|---|
| 1 | 15.40° |
| 2 | 33.40° |
| 3 | 29.00° |
| 4 | 38.00° |

**Mean 29.0°, spread 22.6°.** Significantly worse than 80 RPM.

The mean also shifted by ~12° from the 80 RPM trials. If both were
sampling the same true gravity-rest, means would converge. They don't —
which means 40 RPM releases are landing in false minima (magnetic cogging
detents), not the true rest.

Physics: kinetic energy scales with RPM². Halving RPM → ¼ energy.
Insufficient to sweep past cogging detents after release.

**Rule:** calibration spin must be ≥ 80 RPM to guarantee the arm finds
true gravity-rest. Below that, detents trap it.

### 2026-04-15 · `go` deadband bug

Observed: after `calibrate 80`, `go 90` lands the arm back at its gravity
rest, not at 90°. Root cause: the original `go` formula scaled duration
generously with distance, so a 90° move produced peak ~12.5 RPM (sine
average ~6 RPM) — right in the deadband. The motor twitched and quit.

Fix: constrain peak to ≥ 30 RPM always. Duration now = |d| / 90 seconds,
with a 0.3 s floor for very small moves. At 90° this means 1 s travel
time, peak 30 RPM, average 15 RPM — comfortably above deadband.

**Physical rest position** (Witness 01, photographed 2026-04-15):
heavy aluminum tip points ~5 o'clock (down-right), brass/copper end
toward ~11 o'clock. Disc mounted vertically on wall.

### 2026-04-15 · Motor-drift-after-calibrate bug

Observed: after calibrate completed, the arm sat at its true gravity
rest for a moment, then drifted 30–60° clockwise within a couple
seconds and held there. The drifted position was NOT the true rest.

Root cause: at end of calibrate the firmware re-enabled motor output
with speed=0. The RollerCAN FOC speed loop applies hold torque
whenever output is enabled. That hold torque has its own small
electromagnetic preference (nearest stator-pole alignment) which is not
the mechanical gravity rest. The arm got pulled off gravity rest
toward the electromagnetic preferred position.

Fix: **leave motor RELEASED after calibration.** The arm physically
sits at gravity rest with no hold torque. Motion commands (go/spin/
breath/ritual) re-enable the motor right when they need it. When they
finish, they can optionally release again.

### Zero-vs-rest conceptual fix

The gravity-rest position is NOT artwork-frame 0°. It's wherever the
heavy end happens to hang. In Witness 01's current wall-mount that
position visually corresponds to roughly azimuth 170° (pointing ~5
o'clock). So after calibration, we need a second step:

    calibrate 80            # find gravity rest, define it as raw 0°
    setzero 170             # declare: this rest position = 170° in artwork frame

From then on, `go 0` rotates the arm to what we consider artwork-zero
(north / 12 o'clock), `go 90` to east (3 o'clock), etc. The two-step
split keeps calibration physical (always the same routine) and framing
declarative (one-time per sculpture mount).

### 2026-04-15 · Motor direction asymmetry

Test: `rpm 20` then `rpm -20`.

- **Positive RPM**: smooth, full torque, immediate response.
- **Negative RPM**: starts with a struggle, lots of force, doesn't move
  for a moment, then begins crawling — slower than positive even after
  it gets going.

Cause is internal to the RollerCAN's FOC commutation — the FOC current
loop for one direction is configured/tuned differently than the other.
We can't fix it from the host side.

**Design rule:** **always command positive RPM.** When a target requires
moving "the short way" via negative arc, we plan the longer positive arc
instead. `go` updated to never plan negative motion — it always wraps to
[0, 360) positive degrees.

This means: a 170° → 0° move is actually executed as +190°, not −170°.
The arm goes the long way around but arrives smoothly. Acceptable
trade for the artwork — it just means motion always rotates one
direction, never reverses, which is actually a nice feature.

### 2026-04-15 · Sine profile insufficient for gravity-loaded moves

`go 0` from rest (170°) tried to move +190° at 30 RPM peak (sine).
Result: arm rose ~50°, ran out of torque, fell back, settled at 190°.
The sine profile spends too much of its trip near 0 RPM (deadband zone).
At low cruising RPM the motor can't lift the heavy end through the top
of the arc.

Why calibration works: it commands **constant 80 RPM** for the whole
spin — high steady torque the whole way around.

**Fix: trapezoidal velocity profile.** Quick ramp up to a strong cruise
RPM, hold cruise through the bulk of the move, quick ramp down at the
end. Cruise = 50 RPM (well above deadband, sustainable torque). Ramps
0.25 s each. For a 190° move, total time ≈ 1.0 s with ~0.5 s cruise.

### 2026-04-15 · POSITION MODE DOES NOT WORK (rediscovered)

We tried switching the RollerCAN to POSITION mode (REG_MODE=0) and
commanding target steps via 0x80. **The motor did not respond.**
Encoder reported a stable +5.9° regardless of target (+90°, +180°, +270°,
+360°, +450°). It just sat there.

This is a **known hardware limitation, already documented by Edson on
2026-01-24** in `examples/03_first_witness/TEST-LOG.md`:

> What Doesn't Work
> - Position mode — Motor doesn't respond, LED blinks blue (error)
>
> Mode Comparison Test (2026-01-24 evening)
> Position mode (motor internal): No difference
> Conclusion: No perceivable difference between modes. Stick with Speed
> mode + sine curve.

**Rule:** the M5 RollerCAN in this configuration cannot do internal
position mode. **Only SPEED mode works.** Holding an arbitrary angle
against gravity is therefore not possible with the motor's own
controller. We have two paths:

1. **Software position controller** — read encoder, compute error,
   command corrective speed via SPEED mode. Limited by the 11 RPM
   deadband (small corrections fall in the deadzone) and the direction
   asymmetry (positive strong, negative weak). Probably insufficient
   for true hold but worth trying.

2. **Embrace the gravity-stable constraint** — design choreography that
   only ever lands at the gravity-rest position when stopped. Spin,
   release, settle. Spin, release, settle. Every "still" moment is the
   same physical pose. This is what TEST SYNC mode already does, and
   it works.

**Path 2 is where the artwork lives.** This is not a workaround — it's
a feature. The Witness only ever rests in one place: its true gravity
home. The collector sees that pose and recognizes it.

---

### Still to test

- **Fine-grained floor**: `deadband 10 0.2 12` to pinpoint exact threshold.
- **Reverse direction**: `deadband -5 -1 -17` to check CCW has same floor.
- **Gravity-rest calibration**: `calibrate` — does arm settle at same angle
  across 3 trials?
- **Settle time**: `settle` after high spin.
- **Direction convention**: positive RPM = CW or CCW from front?
- **BREATH redesign**: try `breath 45 3` (peak ~15.7 RPM) — should actually move.
