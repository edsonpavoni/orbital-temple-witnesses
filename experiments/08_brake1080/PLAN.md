# 08_brake1080 — Plan

**Goal.** Same 1080° smooth rotation that 03 does (loose-PID speed mode,
gravity-affected pendulum swing) but **halt at the starting physical
angle** by triggering an active brake at the right moment, instead of
07's "decelerate via velocity ramp + position lock" approach.

**Why brake-based.** 07 had to use position-mode-with-eased-velocity
throughout to land on target deterministically, which kills the gravity
swing aesthetic. The new theory: the swing comes from the *cruise* phase.
If we keep cruise in loose speed mode and only intervene at the very end
with a hard brake, the rotation looks like 03 but stops where we want it.

---

## Strategy

Three-mode trajectory:

1. **RAMP_UP** — speed mode, eased 0 → 8 RPM over 2 s (existing `startTransition`).
2. **CRUISE** — speed mode, target = 8 RPM, loose flash speed-PID
   (P=1.5M, I=1k, D=40M). Gravity perturbations show through. **No time
   bound, no fixed cruise length** — we monitor encoder and brake when
   we predict the rotor will land at start + 1080° if braked now.
3. **BRAKE** — switch to **POSITION mode** with target = `start + 1080°`
   and a temporarily-boosted max current. The position PID's restoring
   torque is the brake. PID gains are 04's validated production set
   (kp=30M, ki=1k, kd=40M).
4. **SETTLE** — hold the position for ~2 s while the rotor damps. Print
   final error.

The key parameter is the **brake-trigger lookahead** — how many degrees
before target we trip the brake to compensate for kinetic-energy carry.

## Predictive brake trigger

The rotor's kinetic energy is `½·I·ω²`, so brake distance scales with the
square of speed. With gravity ripple in cruise, instantaneous RPM varies
~1–13 RPM. A linear approximation will be fine for first iteration:

```
lookahead_deg = K_LOOKAHEAD * actual_RPM
```

Trip the brake when `(encoder − start) + lookahead_deg ≥ 1080`.

`K_LOOKAHEAD` is tuned empirically. Start at **3.0** (so at the nominal
8 RPM cruise, lookahead = 24°). Quadratic refinement
(`lookahead = K2·RPM²`) is a fallback if the linear version proves
insufficient.

## Brake authority

Two knobs to make the brake bite harder:

1. **Boost max current during brake.** Speed-mode max current stays at
   1000 mA per 03's recipe. Position-mode max current (`REG_POS_MAXCUR`,
   register `0x20`) is set to **1500–2000 mA** for the brake phase. This
   is in burst territory but the brake fires for ≤ 1 s per spin so duty
   cycle is low.
2. **Stiff position PID.** Use 04's production gains (kp=30M, ki=1k,
   kd=40M). Higher kp = more reverse torque per degree of overshoot.
   Kd=40M provides damping so we don't oscillate after the stop.

If those still aren't enough, fallback options:

- **CURRENT mode counter-torque pulse** at brake-trigger (mode 3, set
  current target to `−full_scale`), held for the predicted brake time,
  then switch to position mode for final lock. More aggressive but
  requires timing accuracy.
- **Higher kp** for brake (write 60M – 100M temporarily, restore after
  settle).

## State machine

```
IDLE
 │ (g command)
 ▼
RAMP_UP   ── currentSpeedRPM transitions 0 → 8 over 2 s
 │ (transition complete)
 ▼
CRUISE    ── speed mode, target 8 RPM, loose PID
 │ (encoder + lookahead ≥ start + 1080)
 ▼
BRAKE     ── switch to POSITION mode, target = start + 1080,
 │           max-current bumped to BRAKE_MAX_MA, prod PID active
 │ (settle timer expires)
 ▼
SETTLE    ── continue holding in POSITION mode
 │ (settle hold elapsed)
 ▼
IDLE      ── stay in POSITION mode at target so next 'g' starts settled
```

Note: at end of SETTLE we **stay in position mode**, holding at the
locked target. Next `g` will then call `switchToSpeedMode` (200 ms motor
disable + gravity drift) before starting the new cruise. That mode-flip
will move the start reference; that's fine because the spin is anchored
to the encoder reading taken *after* the mode flip is complete.

## Reference capture

Capture the start angle **after** any mode switch into speed mode is
complete and the rotor has settled, not before:

```cpp
switchToSpeedMode();         // 200 ms output-off + drift
delay(300);                  // let rotor settle
g_spinStartEnc = motorGetEncoderDeg();
g_spinTargetEnd = g_spinStartEnc + 1080;
```

This way the brake target is anchored to where the rotor *actually is*,
not where it was before the mode flip.

## Commands

Inherit 03's serial interface as-is. Add:

| Cmd | Effect |
|-----|--------|
| `g` | Start the smooth-cruise + brake 1080° spin |
| `j<deg>` | Jog to absolute physical angle (from 07) — used to position before each test |
| `b<N>` | Set `K_LOOKAHEAD` to N (live tuning) |
| `B<mA>` | Set BRAKE_MAX_MA (live tuning of brake current) |

Keep `s`, `c`, `t`, `e`, `m`, `kp/ki/kd`, `r`, `p`, `x` from 03
untouched so manual tuning still works.

## Tuning protocol

1. Flash, jog to a known angle (`j0`), trigger `g`.
2. Read final error. Possible outcomes:
   - **Overshoot positive** (rotor went past target): `K_LOOKAHEAD` too
     small → brake fires too late → increase. Or brake-current too low →
     increase `BRAKE_MAX_MA`.
   - **Undershoot** (rotor stopped short): `K_LOOKAHEAD` too large →
     brake fires too early → decrease.
   - **Oscillation** (rotor overshoots, swings back, overshoots again):
     position-PID kd insufficient → raise kd, or lower kp.
3. Iterate until 5 consecutive spins land within ±1° of start.
4. Test from multiple starting angles (0°, 90°, 180°, 270°). The brake
   should be angle-agnostic — gravity ripple is in the *cruise*, not the
   brake. If brake performance varies with starting angle, the brake
   isn't authoritative enough → bump `BRAKE_MAX_MA`.

## Risks

- **Jam protection trip** — abrupt brake current spike might trigger the
  motor's stall protection. Mitigation: keep stall protection off
  (already disabled in 03's `motorInit`), and bound the brake duration.
- **Mechanical clunk** — abrupt deceleration is audible / felt. Check if
  it's tolerable for the artwork. If not, soften the brake slightly
  (lower kp or BRAKE_MAX_MA) at the cost of accuracy.
- **Settle oscillation** — the loose-tuned position PID on the
  unbalanced pointer might ring after the brake. The 2 s settle window
  should absorb most of it; if not, raise kd.
- **Voltage drop on brake** — high transient current may pull Vin down
  briefly. Monitor in delta log. If Vin sags below 13 V the brake torque
  drops with V². Mitigate by adding bulk capacitance on the motor's
  power input (hardware change) or by accepting a softer brake.

## Files to create

```
08_brake1080/
├── platformio.ini      (copy from 03)
├── src/main.cpp        (fork 03 + spin state machine)
├── tools/lab.py        (copy from 06/07)
├── PLAN.md             (this file)
└── README.md           (drop in once tuned)
```

## Constants to define

```cpp
const float    SPIN_TOTAL_DEG       = 1080.0f;
const float    K_LOOKAHEAD          = 3.0f;     // deg of brake travel per RPM
const int32_t  BRAKE_MAX_MA         = 1500;     // boost during brake
const uint32_t SETTLE_MS            = 2000;     // hold after brake before reporting
const int32_t  BRAKE_PID_P          = 30000000; // production from 04
const int32_t  BRAKE_PID_I          = 1000;
const int32_t  BRAKE_PID_D          = 40000000;
```

## Implementation order

1. Copy 03 → 08 verbatim. Confirm builds clean.
2. Add `g` / state-machine scaffold with debug prints; build, flash,
   verify state transitions fire correctly (no motion logic yet).
3. Wire RAMP_UP → CRUISE using existing `startTransition`. Verify rotor
   ramps and cruises.
4. Add brake-trigger predicate (no actual brake yet — just print "would
   brake here at p=X" and let cruise continue). Tune `K_LOOKAHEAD`
   roughly.
5. Implement BRAKE phase: mode switch + position target + bumped
   max-current. Observe brake distance.
6. Add SETTLE phase + final-error print.
7. Tune `K_LOOKAHEAD` and `BRAKE_MAX_MA` from real measurements.

## What "success" looks like

5 consecutive spins from the same physical starting angle, each landing
within ±1° of the start, where the visible motion during cruise is
indistinguishable from 03's free-running speed-mode rotation. If the
brake is well-tuned the deceleration should look intentional — not a
crash, but a definite stop — and no perceptible oscillation in the
2 s after the brake fires.
