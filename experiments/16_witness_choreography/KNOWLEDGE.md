# 16_witness_choreography — Knowledge

Forked from `14_witness_provision` on **2026-05-04**. The boot ritual,
calibration, Wi-Fi/NTP, schedule fetch, captive portal, NVS layout,
power gate, and motor I²C are **identical** to 14. For all of that
foundational material, see [`../14_witness_provision/KNOWLEDGE.md`](../14_witness_provision/KNOWLEDGE.md).

This file documents only what's new in 16.

---

## 1. What 16 does

After the 14 boot ritual lands the pointer at visual zero, control hands
to the Choreographer, which runs a recurring 52-second cycle forever:

```
A. ENTER_SAT  8 s   — Tracker eased cosine ramp from current angle to live sat az
B. HOLD_SAT  30 s   — frozen at the captured az; tracker disabled the moment entry completes
C. RELEASE_1  2 s   — REG_OUTPUT=0; unbalanced pointer free-falls to gravity-down (the gesture)
D. SPIN     ~12 s   — ChoreoSpin: 07-style speed-mode 1080° at 10 RPM, random CW/CCW
E. → loop back to A from the spin endpoint (no second release)
```

Total ≈ 52 s. Validated 2026-05-04 on OW1/12, 5+ consecutive cycles
clean, gap to UI < 1° after `forcefetch`.

---

## 2. Design decisions (with rationale)

| Decision | Rationale |
|---|---|
| **Spin uses 07's loose-PID speed mode**, not the validated position-mode lander from SpinOperator | "Alive" gravity-affected swing is the aesthetic. Landing is non-deterministic by ±5–30° — that's fine because the next phase re-finds the satellite. The boot ritual still uses position-mode for ±0.5° landing at visual zero. |
| **Release between phases is REG_OUTPUT=0** (not soft-stop or speed-mode-zero) | The unbalanced pointer falls visibly to gravity-down in ~1–2 s — read as a deliberate "drop" gesture. Edson explicitly chose this over (b) speed-mode-at-zero or (c) position-hold-release. |
| **Random spin direction** (esp_random) | Variation across cycles. Validated: CW first cycle, CCW second, etc. |
| **Hold = freeze, not live-track** | Edson: "during these 30 s we don't have to adjust the position, we can just hold." Tracker is disabled the moment the eased entry completes — pointer locks at the live az **at that instant**. |
| **30 s hold, 10 RPM spin** (bumped from 10 s / 8 RPM) | Tuned interactively 2026-05-04. 8 RPM is the minimum that breaks static friction with the unbalanced pointer; 10 RPM is comfortable and reads as more deliberate. 30 s hold gives the sat-pointing gesture time to register. |
| **No RELEASE_2** (cut after one validation pass with two releases) | The pre-spin drop is the gesture; a second drop just before re-finding the sat was redundant — it added drift but no choreographic value. |
| **`PRODUCTION_MODE = false` ⇒ hourly forcefetch** | TLE staleness causes UI/sculpture divergence (saw 17° on OW1/12). Hourly forcefetch is the dev workaround until the fetchDue policy is tightened in production. See §5. |
| **Schedule degradation: keep cycling** | If schedule is missing or NTP unsynced, ENTER_SAT falls through and the pointer holds at its current angle for the regular 30 s window. Artwork keeps cycling even fully offline. |

---

## 3. New module map

```
src/
├── ChoreoSpin.{h,cpp}        — 07-style speed-mode 1080° spin (random direction)
├── Choreographer.{h,cpp}     — cycle state machine (ENTER_SAT → HOLD_SAT → RELEASE_1 → SPIN → loop)
├── MotorIO.h    (extended)   — speed-mode primitives (setSpeedTarget, setSpeedMaxCurrentMA, setSpeedPID)
├── MotorState.h (extended)   — added ST_CHOREO_SPIN state + 'X' phase code
├── Recipes.h    (extended)   — SPIN07_* + CHOREO_* constants
├── Tracker.h    (extended)   — added isEntering() accessor
└── main.cpp     (extended)   — boot-to-choreo trigger, choreo/chstop serial commands, PRODUCTION_MODE flag
```

Everything else (Calibration, Calibrator, Geolocation, Logger, MoveOperator,
Network, PowerGate, Provisioning, ScheduleClient, ScheduleStore,
SinusoidFit, SpinOperator, WifiCreds) is unchanged from 14.

---

## 4. Validated recipes (constants in Recipes.h)

```
CHOREO 07-STYLE SPIN  (the recurring spin; loose-PID gravity swing)
  SPIN07_KP / KI / KD     = 1.5e6 / 1000 / 4e7      (07_smooth1080's recipe)
  SPIN07_MC_MA            = 1000 mA
  SPIN07_CRUISE_RPM       = 10.0   (bumped from 8.0 on 2026-05-04)
  SPIN07_TOTAL_DEG        = 1080
  SPIN07_CRUISE_END_DEG   = 1020   (ramp-down adds ~60° of coast at 10 RPM)
  SPIN07_RAMP_MS          = 2000   (eased ramp 0↔10 RPM)
  SPIN07_SETTLE_MS        =  500

CYCLE TIMING
  CHOREO_HOLD_MS          = 30000  (bumped from 10000 on 2026-05-04)
  CHOREO_RELEASE_MS       =  2000  (the visible drop window)
```

**Measured spin distance (2026-05-04 hardware run):** 1038–1075° per
spin (vs nominal 1080°). The discrepancy is the loose-PID's gravity-
affected coast envelope — non-deterministic, deliberately. Across
cycles the encoder cumulates monotonically (CW spins +1080-ish, CCW
spins −1080-ish); the absolute encoder reading after N cycles is
unpredictable but the position-mode re-engage at the spin endpoint
always lands cleanly because we read the **current** encoder when
re-engaging.

---

## 5. Schedule staleness — and PRODUCTION_MODE

### The problem

OW1/12 ran with a schedule cached for several hours. UI showed az=187°,
sculpture pointed at 170° — **17° gap.** Diagnosis: the cached TLE on
the sculpture was old enough that its predicted az diverged from the
live ground truth. Sending `forcefetch` (bypasses the 24 h fetchDue
policy) closed the gap to under 1°.

### The dev fix (in code now)

`main.cpp:55` `PRODUCTION_MODE = false` switches the periodic schedule
fetch loop in `loop()` from `tryFetchScheduleIfDue()` (24 h policy) to
`forceFetchScheduleNow()` (forcefetch every hour). One HTTPS request
per hour, trivial. Drift can never accumulate beyond an hour.

### The production fix (when shipping)

Flip `PRODUCTION_MODE = true` AND tighten `ScheduleStore::fetchDue()`
to ~6 hours. Cleaner architecture (no manual workaround) and same
alignment guarantee. KNOWLEDGE.md §5 of 14 already flagged this as a
recommended production tweak.

### How to spot a staleness gap during dev

Watch the UI vs sculpture **at the moment ENTER_SAT completes** (just
before HOLD_SAT begins). If the gap is non-zero AT THAT INSTANT,
schedule was stale. If the gap grows from ≈0° to ≈sat_az_rate×30s
during the hold, that's the deliberate freeze drift (working as spec'd).

---

## 6. Bug history (this session)

### 6.1 ST_CHOREO_SPIN was needed (2026-05-04)

**Symptom:** First flash had `motor.state = ST_SPIN_CRUISE` set during
the choreography spin (intended for logger visibility). The boot
SpinOperator's `tick()` checks `motor.state == ST_SPIN_CRUISE` to know
"I'm cruising"; with stale `_startTargetDeg=0` from the boot spin,
`currentTargetDeg − _startTargetDeg ≥ 1080` evaluated true on the first
tick of the choreo spin, and SpinOperator slammed its position-mode
brake mid-choreography. Log showed `>> SPIN brake` interleaved with
`>> ChoreoSpin: cruise`.

**Fix:** New enum value `ST_CHOREO_SPIN` (phase code 'X'). The boot
SpinOperator's `isActive()` check uses `ST_SPIN_*` only, so it stays
inert during the choreo spin.

**Lesson:** Don't reuse states across operators that have lifecycle
state. If two operators want to "be in cruise," they need two distinct
state values.

### 6.2 RELEASE_2 was unnecessary (2026-05-04)

Initial design had two release phases per cycle (one before spin, one
after, before re-finding the satellite). Hardware run showed the second
release added drift (the pointer drops ~30° during 2 s of OFF) without
adding choreographic value — the eased entry to the next sat position
was happening anyway. **Cut.** Only RELEASE_1 (pre-spin drop) remains.

---

## 7. Pointer fall behavior under release

During the 2 s `RELEASE_1` window, motor output is OFF. The unbalanced
pointer free-falls under gravity. Measured behavior:
- The pointer settles ~30° from the spin endpoint to gravity-down
  (~stable equilibrium) within ~1–2 s.
- After release, the spin starts in speed-mode at 0 RPM with the loose
  PID. The PID absorbs any residual oscillation as the rotor accelerates.
- Cumulative encoder reflects the fall: between spin endpoints, the
  encoder shifts by 30° or so corresponding to the gravity drop.

**The 2 s window is enough.** Earlier advisor flagged this might be too
short, but the loose-PID speed mode absorbs residual oscillation
gracefully. Spin distances were consistent across cycles (1038–1075°),
indicating no destabilizing entry.

---

## 8. Re-engage after spin (no release)

Cut from the cycle, but the mechanism is documented because it lives
in `Choreographer::engageAtCurrentEncoder()`:

After the spin's SETTLE phase ends, motor is in speed-mode at 0 RPM,
output ON. To resume position-mode tracking:

```
1. setOutput(false)              ~20 ms output blip
2. setMode(MODE_POSITION)
3. encoderCounts() → setPosTarget(...)   pin target to current encoder (no snap)
4. setPosPID(PROD_KP/KI/KD)
5. setPosMaxCurrentMA(PROD_MC_MA)
6. setOutput(true)               re-engage in position mode
7. delay(50)
8. motor.currentTargetDeg = readEncoderDeg()  user-frame sync
9. motor.state = ST_HOLDING
```

The ~70 ms total transition is invisible — much shorter than the
deliberate RELEASE drops (2 s). After this, `Tracker::enable()` runs
the 8 s eased ramp from current angle to the live sat az.

---

## 9. Field commissioning checklist (delta from 14)

All of 14's pre-flight checklist applies. Plus:

13. **First cycle observation.** Confirm:
    - `>> ChoreoSpin start dir=...` line appears (random CW/CCW).
    - `>> ChoreoSpin done. traveled=...` reports 1000–1100° per spin.
    - `>> Choreo: -> ENTER_SAT` immediately follows the spin (no
      RELEASE_2 between).
    - HOLD_SAT phase lasts ≈30 s (verify via timestamps in delta log).

14. **Schedule fetch healthy.** With `PRODUCTION_MODE=false`, expect a
    `>> forcefetch: OK.` line every hour in the serial log. If you see
    `>> forcefetch: FAILED` or no fetch lines, network is unhealthy —
    sculpture will keep cycling on stale data and UI/sculpture will
    eventually drift apart.

15. **UI alignment check.** Open `v3-dark.html` (it propagates the
    same TLE locally via satellite.js). Watch the sculpture pointer
    and the on-screen pointer at the moment HOLD_SAT begins — they
    should agree within 1° if the schedule is fresh. Drift during the
    30 s hold is expected (sculpture is frozen, viz tracks live).

---

## 10. Slow az isn't a bug

Sun-sync polar LEO satellites (like SUPERVIEW NEO-2 05, current
default) have ~95 min orbits with 4–6 visible passes per day from any
given observer. **Most of the day the satellite is below the horizon**
(`el < 0`). During those long stretches az drifts at sub-1°/min.
During visible passes az can spike to 3°/s near zenith.

Slow az on the viz during a quiet stretch is not a bug — it's the
satellite's actual celestial geometry. Use `]` on the viz to time-warp
forward and confirm az speeds up during pass windows.

---

## 11. Serial commands (delta from 14)

| Command  | Effect |
|----------|--------|
| `choreo` | Manually start the choreography loop (motor must be holding) |
| `chstop` | Halt the loop. Motor stays in whatever state it was in. |

All of 14's commands work identically. Motion-related commands
(`release`, `hold`, `home`, `spin`, `track`, `move`) implicitly call
`choreo.stop()` first so the loop doesn't fight the manual command.

---

## 12. State machine summary

```
                 ┌─────────────┐
                 │  C_IDLE     │
                 └──────┬──────┘
                        │  start()
                        ▼
                 ┌─────────────┐    if !schedule || !nowUTC
   ┌──────────── │ C_ENTER_SAT │ ◄───── (skip directly to HOLD_SAT)
   │             └──────┬──────┘
   │                    │  Tracker eased entry done (~8 s)
   │                    ▼
   │             ┌─────────────┐
   │             │ C_HOLD_SAT  │  30 s
   │             └──────┬──────┘
   │                    │
   │                    ▼
   │             ┌─────────────┐
   │             │ C_RELEASE_1 │  motor OFF, 2 s drop
   │             └──────┬──────┘
   │                    │
   │                    ▼
   │             ┌─────────────┐
   │             │   C_SPIN    │  ChoreoSpin random dir, 1080°
   │             └──────┬──────┘
   │                    │  spin done → engageAtCurrentEncoder + tryEnterSat
   └────────────────────┘
```

`Choreographer::stop()` returns to `C_IDLE` from any phase. Motor is
left in whatever state the current phase had it in (released, holding,
or spinning) — caller responsible for moving it to a desired state.

---

## 13. Files

```
16_witness_choreography/
├── README.md                  # operational summary + flash instructions
├── KNOWLEDGE.md               # this file
├── platformio.ini
├── partitions_witness.csv
├── src/
│   ├── ChoreoSpin.{h,cpp}             NEW
│   ├── Choreographer.{h,cpp}          NEW
│   ├── MotorIO.h                      extended (speed-mode primitives)
│   ├── MotorState.h                   extended (ST_CHOREO_SPIN)
│   ├── Recipes.h                      extended (SPIN07_*, CHOREO_*)
│   ├── Tracker.h                      extended (isEntering())
│   ├── main.cpp                       extended (boot trigger, commands, PRODUCTION_MODE)
│   └── (everything else identical to 14)
└── tools/                             unchanged from 14 (lab.py, etc.)
```
