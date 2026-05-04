# Orbital Temple Witnesses — Architecture and Knowledge

Consolidated reference for the v1.0 firmware. This document is written for
a future developer (or a future Claude session) who needs to understand the
system quickly. For the operational commands and commissioning checklist, see
`firmware-v1.0/README.md`. For the detailed development journey of each
module, see `firmware-v1.0/KNOWLEDGE.md`.

---

## 1. System overview

Each sculpture is an XIAO ESP32-S3 communicating over I2C with an M5Stack
Unit-Roller485 Lite motor. The motor contains its own FOC driver, magnetic
encoder, and onboard PID loops. The firmware's job is to configure the right
PID mode and setpoints at the right time; the motor's own controllers do the
actual current regulation.

The pointer is deliberately unbalanced. Gravity acts on it continuously, which
is both the calibration mechanism and the visual aesthetic.

**Boot sequence (every power-on):**

```
Power gate (confirm Vin >= 14 V)
  -> Wi-Fi connect (4 attempts, max TX power)
  -> NTP sync (UTC)
  -> Schedule fetch from /api/schedule (cached in NVS on success)
  -> 3 s pause
  -> Gravity calibration (~80 s)
  -> Boot spin: 1080° position-mode, brake at visual zero
  -> Choreography loop begins
```

If Wi-Fi or schedule fetch fails, the sculpture falls through and continues
with whatever is cached in NVS. It never stops.

---

## 2. Module map

```
src/
  main.cpp              Boot sequence, serial command parser, loop()
  Recipes.h             All tuning constants in one place (edit here, not in modules)

  --- Motor I/O ---
  MotorIO.h             Header-only I2C primitives, register addresses, readbacks
  MotorState.h          Shared state struct (mode, encoder, state enum, phase code)

  --- Calibration ---
  Calibration.{h,cpp}   Sensorless gravity homing algorithm (sinusoid fit)
  Calibrator.{h,cpp}    High-level wrapper: runs the sweep, applies mass_offset_deg
  SinusoidFit.{h,cpp}   Least-squares sinusoid fitter used by Calibration

  --- Motion ---
  MoveOperator.{h,cpp}  Position-mode trajectory: eased moves, hold-in-place
  SpinOperator.{h,cpp}  Boot spin only: position-mode rate-integrated 1080°
  SpeedBrakeSpin.{h,cpp} Choreography spin: speed-mode cruise + position-mode brake

  --- Satellite tracking ---
  Tracker.{h,cpp}       Eased cosine ramp to live satellite azimuth
  ScheduleClient.{h,cpp} HTTPS fetch of /api/schedule
  ScheduleStore.{h,cpp}  NVS-backed schedule cache and az interpolation
  Geolocation.{h,cpp}   Observer lat/lon from IP geolocation

  --- Infrastructure ---
  Network.{h,cpp}       Wi-Fi connect/reconnect
  PowerGate.{h,cpp}     Vin readback and boot gate
  Provisioning.{h,cpp}  Captive portal Wi-Fi setup
  WifiCreds.{h,cpp}     NVS-backed credential store
  Logger.{h,cpp}        Compressed delta-log emitter

  --- Choreography ---
  Choreographer.{h,cpp} Cycle state machine (ENTER_SAT -> HOLD_SAT -> SPIN -> SPIN_HOLD)
```

---

## 3. The choreography cycle

After the boot spin lands at visual zero, the Choreographer takes over and
runs this cycle indefinitely:

```
State         Duration    What happens
-----------   ---------   -------------------------------------------------------
C_ENTER_SAT   ~8 s        Tracker runs an eased cosine ramp from current angle
                          to the live satellite azimuth. Tracker is disabled the
                          moment the ramp completes and the pointer locks.
C_HOLD_SAT    30 s        Pointer frozen at the captured azimuth. No tracking.
                          The satellite keeps moving; the pointer does not.
C_SPIN        ~42 s       SpeedBrakeSpin runs:
                            - ramp 0 -> 8 RPM over 2 s (cosine ease)
                            - speed-mode cruise at 8 RPM (loose PID, gravity-alive)
                            - brake when encoder displacement >= 1797°
                            - position-mode brake fires: PID set, mode switch,
                              max-current, setpoint (exactly spinStart + 1800°)
                            - 500 ms settle
C_SPIN_HOLD   30 s        Motor holds in position mode at brake landing.
                          Choreographer owns this phase; SpeedBrakeSpin is done.
                          On exit, restoreProductionState() brings current back
                          to PROD_MC_MA (1000 mA) and syncs MotorState.
Re-acquire    (immediate) New live satellite az is fetched, loop to C_ENTER_SAT.
```

Total cycle: ~110 s.

---

## 4. Two control modes and when each is used

The Roller485 has two PID modes: SPEED and POSITION. Understanding when each
applies is essential for reading the code.

**SPEED mode (speed-PID closed-loop):**
- The firmware writes a target RPM to REG_SPEED.
- The motor's internal speed PID closes the loop.
- The loose recipe (SPIN07_KP = 1.5e6, KI = 1000, KD = 4e7) intentionally
  does NOT fight gravity. The pointer swings with a pendulum-alive feel.
- Used for: the choreography spin cruise phase.

**POSITION mode (position-PID closed-loop):**
- The firmware writes a target angle (in encoder counts) to REG_POS.
- The motor's internal position PID closes the loop.
- The stiff recipe (PROD_KP = 30e6, KI = 1000, KD = 40e6) holds angle
  precisely against gravity disturbance.
- Used for: calibration sweeps (diagnostic), boot spin, hold phases,
  brake landing, satellite tracking.

**Switching between modes** requires writing registers in a specific order.
See section 6 for the brake register sequence. Switching wrong starts the
position loop with stale PID gains.

---

## 5. Critical integration bugs (read before touching choreography)

These two bugs were found during the 2026-05-04 validation session. Both were
subtle enough to produce misleading symptoms. Document them so the next
developer does not repeat the search.

### 5.1 Cruise REG_SPEED must be refreshed every tick

**Symptom:** During the speed-mode cruise phase, the motor visually looks
normal for ~10 s then becomes progressively laggy and stiff, as if stuck in
position mode. The serial log shows cruise phase ongoing but the pointer
resistance is wrong.

**Root cause:** The speed-mode PID has an integral term (KI = 1000). The
firmware was writing `setSpeedTarget(_curSpeedRPM)` once at the start of
cruise and then not updating it. With no refresh, the speed-PID's I-term
winds up over the ~37 s cruise as the motor fights gravity. By mid-cruise the
accumulated I-wind produces a restoring force indistinguishable from being in
position mode.

**Fix:** In `SpeedBrakeSpin::tick()`, the cruise branch writes
`MotorIO::setSpeedTarget(_curSpeedRPM)` on every tick (every 10 ms). The
I-term has no time to accumulate between refreshes.

**The lesson:** In speed mode, write the speed setpoint every tick even if
it hasn't changed. Silence is not neutral; it lets the I-term drift.

### 5.2 currentTargetDeg must be synced at the C_SPIN -> C_SPIN_HOLD transition

**Symptom:** At the end of the spin, the motor violently yanks the pointer
backward by ~1800°, snapping to the pre-spin satellite azimuth. This looks
like the position PID is fighting the choreography.

**Root cause:** `MoveOperator::tick()` runs every loop iteration. When the
motor is in ST_HOLDING state, it writes `_m.currentTargetDeg` to REG_POS
every tick to maintain hold. SpeedBrakeSpin writes REG_POS directly during
the brake phase (via `MotorIO::setPositionTarget()`), bypassing
`_m.currentTargetDeg`. After the spin, when Choreographer transitions to
C_SPIN_HOLD, the motor is physically sitting at spinStart + 1800°, but
`_m.currentTargetDeg` still holds the pre-spin satellite azimuth from
HOLD_SAT. On the next `MoveOperator::tick()`, it writes that stale azimuth
to REG_POS and the motor obeys, yanking 1800° backward.

**Fix:** In `Choreographer::tick()`, at the `C_SPIN -> C_SPIN_HOLD`
transition boundary:

```cpp
_m.currentTargetDeg = _m.readEncoderDeg();
```

This syncs the user-frame tracking variable to the physical reality before
`MoveOperator` gets a chance to write anything.

**The lesson:** Any time you bypass `MoveOperator::moveTo()` to write REG_POS
directly (as SpeedBrakeSpin does), you MUST sync `_m.currentTargetDeg`
afterward or MoveOperator will fight you on the very next tick.

---

## 6. Brake register write order (critical)

When transitioning from speed mode to position mode for the brake, the order
of register writes matters. Validated in 09_speed_brake:

```
1. Write position PID gains  (kp, ki, kd -> REG_POS_PID)
2. Write REG_MODE = POSITION
3. Write max current          (-> REG_POS_MAX_CURRENT)
4. Write position setpoint    (-> REG_POS)
```

Each write separated by ~5 ms. Do NOT toggle REG_OUTPUT during this sequence.

Why the order matters: if you write REG_MODE first, the position loop starts
with whatever PID is currently in motor flash before you've written the
correct gains. Starting with stale or zeroed PID produces a first control
cycle with wrong behavior, which can cause a visible jerk or failed landing.

Why no output toggle: toggling output OFF then ON adds ~200 ms of coast time
while the rotor has momentum at 8 RPM, reducing the brake's absorption window.
Writing registers inline with output always ON gives the position PID the full
deceleration to work with.

---

## 7. Hardware caveats

**Vin readback is unreliable as a power-state indicator.**
The Roller485's Vin register (`REG_VIN`) reads the motor rail voltage, but
it does not reliably indicate whether the 15 V power brick is connected vs.
the motor running on the XIAO's USB 5 V rail alone. In practice: confirm
motor power visually (the OLED display lights up when the 15 V brick is live).
Do not gate critical code on `readVin() >= 14.0`.

**15 V required.** At 11 V the motor cannot break static friction with the
unbalanced pointer. This was validated in 06_mix. Always use the 15 V PD brick
for the motor supply.

**I2C at 0x64 only.** The Roller485 Lite does not have a configurable address.
If you have multiple sculptures on one bus you need bus switching hardware.

---

## 8. Tuning knobs

All tuning constants live in `firmware-v1.0/src/Recipes.h`. Edit there, not
inside individual module files.

| Constant group | Keys | Effect |
|---|---|---|
| Production hold | PROD_KP / KI / KD, PROD_MC_MA | Position-mode hold stiffness and current |
| Choreography spin | SB_CRUISE_RPM | Speed during choreography spin cruise (default 8 RPM) |
| Choreography spin | SB_BRAKE_TRIGGER_DEG | Encoder displacement at which brake fires (default 1797°) |
| Choreography spin | SB_BRAKE_LEAD_DEG | Extra lead distance baked into trigger (default 3°) |
| Choreography spin | SB_BRAKE_MAX_MA | Current ceiling during brake (default 1500 mA) |
| Choreography spin | SB_RAMP_MS | Duration of 0->cruise ramp and cruise->0 ramp (default 2000 ms) |
| Choreography spin | SB_SETTLE_MS | Hold duration after brake before reporting done (default 500 ms) |
| Cycle timing | CHOREO_HOLD_MS | Duration of HOLD_SAT and SPIN_HOLD phases (default 30000 ms) |
| Calibration | SPIN07_KP / KI / KD | Speed-mode PID for calibration sweep (loose recipe) |
| Boot spin | SPIN_CRUISE_RPM | RPM for the boot 1080° spin (default 8 RPM) |

To change the spin from 5 revolutions to 3: set `SB_TOTAL_DEG = 1080` and
recalculate `SB_BRAKE_TRIGGER_DEG = SB_TOTAL_DEG - SB_BRAKE_LEAD_DEG`.

---

## 9. How to extend

**Add random spin direction (CW/CCW alternating):**
In `SpeedBrakeSpin::start()`, pick direction with `esp_random() % 2`. Write
`setSpeedTarget(_curSpeedRPM * direction)` where direction is +1 or -1.
Update the brake setpoint to `spinStartEnc + direction * SB_TOTAL_DEG`.
16_witness_choreography has a working implementation in ChoreoSpin for
reference.

**Change cruise RPM:**
Edit `SB_CRUISE_RPM` in Recipes.h. Also recalculate `SB_RAMP_DEG` (degrees
covered during a ramp at that RPM) to keep `SB_BRAKE_TRIGGER_DEG` accurate.
The formula: each 2 s cosine ramp covers `RPM * 6 * 2 * 0.5 = RPM * 6` degrees.

**Add a second sculpture (two motors, one ESP32):**
The firmware currently assumes one motor at I2C address 0x64. To support two,
wrap MotorIO with an address parameter, instantiate two sets of all modules
(Calibrator, SpinOperator, etc.), and interleave their choreography phases.
You will need I2C multiplexing hardware if both motors need the same address.

**Change hold duration:**
Edit `CHOREO_HOLD_MS` in Recipes.h. Both HOLD_SAT and SPIN_HOLD use this
constant, so they change together. To make them independent, split into
`CHOREO_HOLD_SAT_MS` and `CHOREO_SPIN_HOLD_MS`.

---

## 10. Endurance numbers (v1.0 validation)

Validated 2026-05-04, sculpture 1 of 12, Williamsburg Brooklyn studio:

| Metric | Value |
|---|---|
| 1-hour endurance test | 29 cycles completed |
| Mean pointing error | 1.27° |
| Error range | -3° to +2.6° |
| Anomalies | 0 |
| Extended continuous operation | 8+ hours |
| Extended operation result | No faults (visual confirmation) |

The 1.27° mean error is the azimuth residual: the difference between the live
satellite azimuth at the moment HOLD_SAT begins and where the pointer actually
points. It includes TLE staleness (largest contributor at slow az rates) and
brake landing spread (typically under 1°).

---

## 11. Schedule staleness and PRODUCTION_MODE

`PRODUCTION_MODE` in `main.cpp` controls the schedule fetch policy:

- `false` (development): `forceFetchScheduleNow()` every hour. One HTTPS
  request per hour, no staleness accumulation.
- `true` (production): `tryFetchScheduleIfDue()` checks a 24-hour cache
  window. Suitable for installed artwork; less network traffic.

**Recommended production setting:** flip to `true` AND tighten
`ScheduleStore::fetchDue()` to ~6 hours. This gives a good balance between
network frugality and TLE freshness.

**How to detect staleness during validation:** Open the visualization UI
(`tools/v3-dark.html`). At the moment ENTER_SAT completes (transition to
HOLD_SAT), the UI pointer and the sculpture pointer should agree within 1°.
If they diverge by more at that instant, the schedule was stale. Run
`forcefetch` over serial to resync immediately.

---

## 12. Development workflow (lab.py serial bridge)

`firmware-v1.0/tools/lab.py` is a background serial bridge that stays
connected to the XIAO while you edit, build, and upload. It owns the serial
port and lets a separate process (or a Claude session) inject commands and
read firmware output without restarting the monitor.

```bash
cd firmware-v1.0
~/.platformio/penv/bin/python tools/lab.py start       # background bridge
~/.platformio/penv/bin/python tools/lab.py events 20   # last 20 firmware events
~/.platformio/penv/bin/python tools/lab.py send "choreo"   # send a command
~/.platformio/penv/bin/python tools/lab.py stop        # shut down
```

The bridge writes a timestamped log to `/tmp/lab_pos.log`. `events N` filters
to command echoes and `>>` firmware messages only, skipping the dense delta-log
lines. Use `events` for routine debugging; use `session` for full delta data.
