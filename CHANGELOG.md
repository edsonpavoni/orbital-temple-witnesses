# Changelog

## v1.0 — 2026-05-04

Production firmware for the First Witness sculpture series. All twelve
sculptures run this build.

### What ships in v1.0

**Choreography loop** (the core artwork behavior):
- Recurring ~110 s cycle: find satellite azimuth, hold 30 s, 5-revolution
  smooth spin, hold 30 s at landing, repeat with fresh azimuth
- SpeedBrakeSpin module: speed-mode gravity-alive cruise (8 RPM, loose PID)
  + position-mode brake at exactly 1800° from spin start
- Deterministic landing: mean error 1.27°, range -3° to +2.6° across 29
  cycles in a 1-hour endurance test
- 8+ hours continuous operation without faults

**Boot ritual** (unchanged from 14_witness_provision):
- Power gate: confirms 15 V motor supply before any motion
- Wi-Fi + NTP + satellite schedule fetch (cached in NVS for offline use)
- Sensorless gravity calibration: 2-rev CW + 2-rev CCW sweep, sinusoid fit,
  finds visual-up within ±2° from any starting angle
- Boot spin: 1080° position-mode rate-integrated, brakes at visual zero
  within ±0.5°
- Graceful degradation: if Wi-Fi, NTP, or schedule fail, artwork continues
  on cached data or holds current angle; never stops

**Infrastructure** (accumulated across 09 through 14):
- Wi-Fi captive portal for credential provisioning (Provisioning.cpp)
- NVS-backed storage for Wi-Fi credentials, mass_offset_deg, schedule cache
- Geolocation from IP for automatic observer lat/lon
- Compressed delta-log serial output (silence = nothing changed)
- Full serial command set for manual operation and field debugging

### Integration bugs fixed during v1.0 development

**Cruise REG_SPEED refresh (2026-05-04):** Speed-mode cruise requires writing
`setSpeedTarget()` every tick, not just at cruise entry. Without the refresh,
the speed PID's I-term winds up over the ~37 s cruise, producing the same
resistive feel as position mode. Fixed in SpeedBrakeSpin::tick().

**currentTargetDeg sync at C_SPIN -> C_SPIN_HOLD (2026-05-04):** After
SpeedBrakeSpin writes REG_POS directly (bypassing MoveOperator), the user-
frame tracking variable `_m.currentTargetDeg` holds the pre-spin satellite
azimuth. Without syncing it to `readEncoderDeg()` at the transition boundary,
MoveOperator yanks the pointer 1800° backward on the first tick of SPIN_HOLD.
Fixed in Choreographer::tick().

### Dead code removed

`ChoreoSpin.{h,cpp}` were retained in 17 during development as a compile-clean
fallback from 16_witness_choreography. Removed in v1.0 cleanup. The module is
preserved in `experiments/16_witness_choreography/src/` if needed.

### Known limitations in v1.0

- Spin direction is always CW (+1800°). Random CW/CCW direction was not
  validated in time for v1.0; reserved for v1.1.
- `PRODUCTION_MODE = false` in main.cpp: schedule is force-fetched hourly
  during the validation period. Flip to `true` and tighten fetchDue to ~6 h
  before permanent installation.
- Vin readback unreliable: do not gate code on `readVin()` for power-state
  detection. Confirm motor power visually.

---

## Prior iterations (not versioned)

See `experiments/LINEAGE.md` for the full development history from
03_motion_test through 16_witness_choreography.
