# Sensorless Gravity-Based Homing — Findings

Wall-mounted slightly-unbalanced pointer, M5Stack Unit-Roller485 Lite + XIAO
ESP32-S3 over I²C. Tuning + calibration session: **2026-04-29**.

This document captures the design journey: what we tried, what worked,
what didn't, and why the final architecture is the way it is. For
day-to-day use see `README.md`.

---

## 1. Result

**On every power-up, from any starting angle, the pointer rotates to true
visual vertical with ±2° accuracy.** No extra sensors. No hand-positioning.

Final validation, 3 power-cycle runs from random starts:

| Run | Final landing |
|-----|---------------|
| 1 | +2° from vertical |
| 2 | +1° from vertical |
| 3 | −2° from vertical |

Mean error 0.3°, range ±2°. Inside the motor's own ±2° mechanical-alignment
spec.

---

## 2. The physics we exploit

The pointer is mechanically unbalanced — its center of mass is not on the
rotation axis. As the rotor turns, gravity exerts a position-dependent
torque on the pointer:

```
τ_gravity(θ) = -m · g · r · sin(θ_mass)
```

where `θ_mass` is the mass's absolute angle measured from "up." Two key
positions have zero gravity torque:

- **Gravity rest** (mass straight down) — stable equilibrium.
- **Gravity up** (mass straight up) — unstable equilibrium.

If we drive the motor in steady-state rotation at constant speed, the
torque the motor must supply to maintain that rotation is dominated by
this sinusoidal gravity component (with friction as a smaller, constant
direction-anti-symmetric offset). The current draw therefore traces a
sinusoid in encoder angle, and the **phase** of that sinusoid pinpoints
the unstable equilibrium ("gravity up") relative to the encoder's working
zero.

Then the pointer's **mass offset** — the angular distance between the
gravity-up direction and the visible pointer-tip-up direction — is a
fixed mechanical property we measure once and store in NVS. With both
known, visual-up = gravity-up − mass_offset.

---

## 3. Two-direction sweep cancels friction

A single-direction rotation sees both gravity and friction in the current
signal. To isolate gravity, we use a clean physical separation:

- **Gravity** is direction-symmetric — at any given encoder angle, the
  motor must apply the same torque to oppose gravity regardless of which
  way it's spinning.
- **Friction** is direction-anti-symmetric — friction always opposes
  motion, so its contribution to motor current flips sign with direction.

```
I_CW(θ)  + I_CCW(θ)  =  2 · gravity_signal(θ)     // friction cancels
I_CW(θ)  − I_CCW(θ)  =  2 · friction_signal(θ)    // gravity cancels
```

The firmware spins one revolution CW, pauses, spins one revolution CCW,
and accumulates samples into a least-squares sinusoid fit. The sum
mathematically isolates gravity.

---

## 4. The journey — what we tried, in order

### Phase 0 — verify the gravity signal is real

Before designing any algorithm, we ran a diagnostic two-direction sweep
with a deliberately loose PID (so the gravity-induced lag becomes visible)
and looked at the data.

**Result:** clean sinusoidal pattern in both `er` (position error) and
`cur` (motor current). Pearson correlation between CW and CCW samples
≥ +0.80 in both signals. Peak-to-peak swing 25–37° in `er`, 350–540 mA in
`cur`. **Gravity signal is dominant; we can use it.**

`cur` was marginally cleaner than `er`, so the on-board fit uses `cur`.
Adding current-readback to the firmware (register `0xC0`) was a one-line
change.

### Phase 1 — off-board algorithm

`tools/homing_analysis.py` parses a captured log, bins samples by encoder
angle, computes `(I_CW + I_CCW) / 2` per bin (the gravity component),
and fits a 3-parameter sinusoid via linear least squares:

```
y(θ) = a · sin(θ) + b · cos(θ) + dc
phase c = atan2(b, a)
gravity-up at encoder angle = -c (mod 360°)
```

Three runs from the same hand-calibrated starting position gave
gravity-up estimates of +11.24°, +9.44°, +11.95° in user frame —
reproducible to ~2°. **Algorithm works.**

### Phase 2 — port the algorithm on-board

The 3×3 LS solver is ~30 lines of C++ (Gauss elimination on a 3×3
augmented matrix). The accumulators (8 running sums + count) are tiny.
Total memory cost: ~50 bytes.

The firmware now runs the diag, fits the sinusoid, computes visual-up
from gravity-up + offset, re-anchors the user-frame zero, and moves to
visual-up — all autonomously.

### Phase 3 — make calibration position-independent

The first round of testing revealed a problem: **the algorithm's output
depended on the starting position.** From a near-vertical start it
reported gravity-up at +10°; from an upside-down start it reported
gravity-up at −163° instead of the expected −170° (a 7° error).

Three theories were considered:

1. **Free-fall pre-conditioning** — disable the motor briefly so the
   pointer pendulum-swings to gravity rest, then start the diag from
   there. Same physical start every time.
2. **Discard endpoint data** — exclude samples taken near the start and
   end of each sweep, where motor stick-slip transients live.
3. **Multiple revolutions per direction** with the first revolution
   discarded — let the motor reach steady-state spin before sampling.
4. **Higher-harmonic fit** — fit `+ a₂·sin(2θ) + b₂·cos(2θ)` so the model
   can absorb 2nd-harmonic artifacts cleanly.

The user picked **(3) — discard the first revolution.** Implementation:
spin 720° per direction, only feed the second revolution into the fit.

### What didn't work: free-fall

We *also* tried free-fall pre-conditioning (theory 1). It was based on a
wrong physical assumption: that gravity would dominate friction enough
to swing the pointer as a damped pendulum.

Empirically, **gravity torque from this small mass imbalance is *weaker*
than the bearing/cogging friction.** When the motor is disabled, the
pointer barely swings — friction holds it nearly wherever the kick left
it. The user observed this directly: a 30° kick moved the pointer 25°,
and after release it drifted only ~5° back toward an unstable resting
position.

So free-fall didn't work as a settling mechanism on this hardware. We
removed it. The 2-revolution-with-first-discarded approach handles
position-dependence through *data* (averaging over a clean second revolution
that's reached steady state) rather than through *physics* (relying on
the pointer to actually swing freely).

### What worked: 2-rev fit + 14.83° → 10.83° offset trim

After implementing the 2-rev approach, four power-cycle tests from
deliberately diverse starting angles (0°, +11°, −10°, +180°) gave
landing errors in a tight band: **−4°, −5°, −3°, −4°**. Range 2°.

This is the signature of a successful 2-rev fix: the **error magnitude is
constant across starting positions**. Position-dependence is gone; what
remains is a constant bias that's absorbed by adjusting the offset
constant. We had been using `mass_offset_deg = 14.83°` from a
protractor-aided calibration; lowering it by the average bias (4°) gave
us **10.83°**.

Re-running the test from random starts after the trim gave +2°, +1°, −2°.
Mean 0.3°, range ±2°. Done.

---

## 5. Other things we learned

### Pre-anchoring the encoder when changing PID gains

Switching motor PID gains while the output is enabled can cause violent
oscillation: the previous gains' integral term carries over and gets
multiplied by the new (often much larger) gains, producing a huge
instantaneous output. Symptoms: motor slams, supply voltage droops 4 V,
high-frequency limit cycle.

The fix (`safeSwitchPid` helper):
1. Disable output (resets the motor's internal integrator).
2. Pin the position target to the current encoder reading (so on
   re-enable, the motor sees zero error and doesn't lunge).
3. Write new PID + current cap.
4. Re-enable output.
5. Update software state to match the actual encoder position.

This was a one-time pain point worth documenting because it's a
surprisingly common gotcha when reconfiguring PID gains live.

### The protractor reference itself was off by ~4°

The first hand-aligned calibration ("pointer at visual vertical, run
diag, save the measured offset") gave us 14.83°. The empirical mean
landing error from that offset was −4°. So the protractor + eyeball
reference was off by 4° from true vertical.

This is normal; bubble levels and hand-held protractors are easily ±2–3°.
The lesson: **don't trust the calibration reference more than the
algorithm.** Better to do a few power-cycle tests with the offset, observe
the mean landing error, and trim the offset by that amount.

### Algorithm has a 180° ambiguity that didn't bite us

The sinusoid `y(θ) = A·sin(θ + c)` has two zero-crossings per cycle:
gravity-up (positive slope) and gravity-rest (negative slope). The
algorithm picks gravity-up by virtue of `atan2`'s return convention, but
edge cases where (a, b) coefficients are near a sign flip could cause
the algorithm to report the *other* zero. We saw one such report
(+169.82° instead of +12°).

In practice this happened only once across all our testing, and the
final 2-rev architecture made the data robust enough that subsequent
runs never repeated it. A safety check (compare gravity-up estimate
against gravity-up + 180°, pick the one closer to the working frame's
likely starting state) could be added if it ever becomes a recurring
issue. For now, monitored.

---

## 6. Final architecture

### Boot ritual

```
┌─────────┐    ┌──────┐    ┌──────────┐    ┌────────────┐
│ Latch   │ -> │ Diag │ -> │ Fit +    │ -> │ Re-anchor  │
│ encoder │    │ sweep│    │ apply    │    │ + move to  │
│ (3 s    │    │      │    │ offset   │    │ visual-up  │
│ pre-    │    │ ~73 s│    │ < 100 ms │    │ ~3 s       │
│ delay)  │    │      │    │          │    │            │
└─────────┘    └──────┘    └──────────┘    └────────────┘
```

Total: ~80 s from power-on to ready.

### Diag sweep details

```
┌──────────┐    ┌──────┐    ┌──────────┐
│ CW spin  │ -> │ Pause│ -> │ CCW spin │
│ 720 deg  │    │ 1.5s │    │ 720 deg  │
│ (eased)  │    │      │    │ (eased)  │
└──────────┘    └──────┘    └──────────┘
   |                            |
   `------- skip rev 1 ---------'  (first half of each direction
                                    is not sampled — motor is still
                                    breaking static friction and
                                    accelerating into steady-state spin)
   |                            |
   `------- fit rev 2 ----------'  (motor is in steady spin;
                                    gravity dominates the torque budget;
                                    samples are clean)
```

Rev 2 of each direction = ~360° of clean samples, ~90 samples per direction
at the 5 Hz delta-log cadence.

### Two PID profiles

The firmware switches between them as needed:

- **Diagnostic PID** — used during the diag sweep. Loose enough that
  gravity-induced lag is large and clearly modulates the current
  signal.
- **Production PID** — used for the final move to visual-up and any
  subsequent moves. Tight enough to land within ±0.5° of any commanded
  angle (validated separately in `04_position_lab/test3`).

### NVS-persisted constant

`mass_offset_deg` lives in ESP32 NVS under namespace `"calib"`, key
`"mass_offset"`. Default 10.5° (compile-time fallback if NVS is wiped).
Current value: **10.83°** (calibrated 2026-04-29).

To recalibrate for a different pointer or after a mechanical change,
follow the procedure in `README.md` § 3.

---

## 7. Known limitations

1. **±2° accuracy** — limited by sample noise in the sinusoid fit and the
   1-bit-resolution encoder rounding. Could probably be improved to ±1°
   with longer integration (more revolutions) or higher-harmonic fits.
   Currently more than enough for an artwork.

2. **80-second ritual time** — the slow diagnostic sweep is the bottleneck.
   Faster sweeps would shorten this but degrade signal quality. For a
   sculpture that boots once and runs for hours, this is acceptable.

3. **Initial mass-offset calibration is manual** — needs one
   hand-positioned reference + a few power-cycle test iterations to
   trim. Once stored in NVS, persists indefinitely; only needs redoing
   if the pointer is mechanically modified.

4. **180° ambiguity is monitored, not solved** — see § 5. Hasn't
   triggered in the final architecture but could.

5. **Free-fall settling does NOT work on this hardware** — see § 4.
   Friction is too high relative to gravity torque from the small
   imbalance. If you swap pointers for a much heavier or more
   imbalanced one, free-fall pre-conditioning might become viable
   (and would be the cleanest path to true position-independence).

---

## 8. What's next

This firmware is purpose-built for calibration. The next iteration will
merge three orthogonal capabilities into one unified production firmware:

- **Calibration** (this folder) — boot-time auto-home to visual vertical.
- **Precise positioning** (`04_position_lab`) — move to any angle
  with ±0.5° accuracy.
- **Smooth rotation** (`03_motion_test`) — continuous gravity-aware
  rotation at low RPM with elegant velocity profiles.

The unified firmware will boot, calibrate, then expose precise-position
and smooth-rotation modes for the artwork's choreography.

---

## Appendix: timeline of key tests

```
2026-04-29 morning — Phase 0 diagnostic sweeps:
  Run 1: gravity-up at +10° in user frame, R²=0.85
  Run 2: gravity-up at +8.8°,             R²=0.87
  Run 3: gravity-up at +11.3°,            R²=0.88
  -> gravity signal is real and reproducible

2026-04-29 — first firmware build with on-board fit:
  hand-aligned protractor calibration: mass_offset_deg = 14.83°
  random-start tests:
    180° start: -3° error
    90° start:  -2° error

2026-04-29 — investigation of position-dependence:
  Tried "1-rev diag" — 7-10° errors from far-from-vertical starts.
  Tried "2-rev diag, discard rev 1" — errors collapse to ±2°.

2026-04-29 — empirical offset trim:
  4 power-cycle tests with mass_offset_deg = 14.83°:
    starts: 0°, +11°, -10°, +180°
    landings: -4°, -5°, -3°, -4°    (mean -4°)
  Trim: 14.83° - 4° = 10.83°. Save to NVS.

2026-04-29 — final validation, 3 power-cycle runs:
  Landings: +2°, +1°, -2°.    Mean 0.3°, range ±2°.    DONE.
```
