# firmware-v1.0 — speed-brake 1800° choreography cycle

Production firmware for the First Witness sculpture series. v1.0, 2026-05-04.
Validated: 29 cycles / 1-hour endurance, mean error 1.27°, 8+ hours continuous.

Forked from `experiments/16_witness_choreography` on **2026-05-04**. All boot,
calibration, Wi-Fi, NTP, and schedule plumbing is identical to 16 (and 14
before it).

The change is in the choreography loop: replaces ChoreoSpin (07-style
speed-mode ramp-down, non-deterministic landing) with SpeedBrakeSpin (09-style
speed-mode cruise + position-mode brake, 1800°, deterministic landing). The
pre-spin drop (RELEASE_1) is removed. A post-spin hold (SPIN_HOLD) is added.

## Cycle

```
calibration -> enter sat (8 s) -> hold sat (30 s) -> smooth 1800° spin (~42 s)
-> hold at landing (30 s) -> repeat with new sat position
```

Detailed breakdown:

```
A. ENTER_SAT  ~8 s   — eased cosine ramp from current angle to live sat az (Tracker)
B. HOLD_SAT   30 s   — frozen at the captured az; tracker disabled at entry completion
C. SPIN       ~42 s  — SpeedBrakeSpin:
                         ramp up 0->8 RPM (2 s cosine)
                         cruise at 8 RPM until encoder reaches 1797° (BRAKE_TRIGGER)
                         fire position-mode brake: PID, mode, max-current, setpoint
                         settle 500 ms while PID converges to 1800° target
D. SPIN_HOLD  30 s   — position-mode hold at brake landing; Choreographer owns this
E. -> re-acquire satellite (sky has moved), loop to A
```

Total cycle: ~8 + 30 + 42 + 30 = **~110 s**.

## Key differences from 16

| 16 | 17 |
|----|----|
| ChoreoSpin: 07-style ramp-down, non-deterministic landing | SpeedBrakeSpin: 09-style brake, lands within ~1° of target |
| 1080° spin (3 revolutions) | 1800° spin (5 revolutions CW) |
| RELEASE_1: motor OFF 2 s, pointer drops before spin | No drop. Motor stays output-ON through mode switch |
| No post-spin hold | SPIN_HOLD: 30 s at brake landing |
| Cycle ~52 s | Cycle ~110 s |

## Design notes

**No drop.** The pre-spin drop (RELEASE_1) is removed. SpeedBrakeSpin::start()
switches from position mode to speed mode without toggling output, so there is
no visible drop before the spin begins. The mode switch is clean: write
REG_MODE=SPEED, load loose PID, set current, write speed=0, then capture
encoder origin.

**Brake register order (critical).** From 09's validated lessons:
PID first, then mode, then max-current, then setpoint. Each write separated
by ~5 ms. The position loop starts with correct gains on its very first
control cycle. No output toggle during the switch — rotor never coasts.

**Speed PID during cruise.** Uses SPIN07_KP/KI/KD (1.5e6 / 1000 / 4e7) —
the loose gravity-alive recipe from 07_smooth1080. The rotor accelerates
downhill and slows climbing back up. The brake then absorbs the accumulated
momentum and snaps to 1800°.

**Direction.** Always CW (+1800°) for now. Random direction to be added in a
later iteration once the deterministic profile is validated on hardware.

**Encoder capture.** spinStartEnc is captured AFTER the mode switch to speed
mode has settled (after setSpeedTarget(0) + 20 ms delay). This avoids
including any micro-jitter from the mode transition in the displacement
calculation.

**SPIN_HOLD.** After SpeedBrakeSpin signals isActive()==false, the motor is
already in position mode, output ON, at the brake target. Choreographer enters
SPIN_HOLD and simply waits 30 s. On exit, restoreProductionState() brings
PROD_MC_MA (1000 mA, vs BRAKE_MAX_MA=1500 mA) back and syncs MotorState.

**Schedule degradation.** If the sat schedule is not cached or the clock is
not synced, ENTER_SAT falls through and the pointer holds at its current angle
for the HOLD window. The cycle continues — the artwork is alive fully offline.

---

## How to compile

```bash
cd firmware-v1.0
~/.platformio/penv/bin/pio run
```

## How to flash

```bash
cd firmware-v1.0
~/.platformio/penv/bin/pio run -t upload
```

---

## Serial commands

Identical to 16. Two commands manage the choreography:

| Command  | Effect |
|----------|--------|
| `choreo` | Start the choreography loop (motor must be holding) |
| `chstop` | Halt the loop. Motor stays in whatever state it was in. |

Other motion commands (`release`, `hold`, `home`, `spin`, `track`, `move`)
implicitly call `choreo.stop()` so the loop doesn't fight manual operation.

The boot ritual auto-starts the choreographer once the boot 1080° lands at
visual zero. Set `BOOT_CHOREO_ENABLED = false` in `main.cpp` to disable.

---

## Files changed from 16

```
firmware-v1.0/
├── README.md                      # this file (updated for v1.0)
├── platformio.ini                 # updated project description
├── src/
│   ├── SpeedBrakeSpin.{h,cpp}     # NEW: 09-style speed+brake 1800° spin
│   ├── Choreographer.{h,cpp}      # UPDATED: new cycle (no RELEASE_1, adds SPIN_HOLD)
│   ├── Recipes.h                  # UPDATED: SB_* + BRAKE_* constants added
│   ├── main.cpp                   # UPDATED: SpeedBrakeSpin instantiation + banner
│   └── (ChoreoSpin removed — compile-clean leftover from 16, deleted in v1.0)
└── ... (everything else identical to 16)
```
