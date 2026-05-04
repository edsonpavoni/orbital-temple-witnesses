// ChoreoSpin.h — speed-mode 1080° spin in the 07_smooth1080 style.
//
// Replicates the rotation aesthetic that was tuned and validated in
// 07_smooth1080: speed-mode loose PID (P=1.5e6, I≈0, D=4e7), 8 RPM cruise,
// 2 s eased ramps. The unbalanced pointer accelerates with gravity on the
// descent and slows climbing back up — the "alive swing" texture that
// position-mode rate-integration kills.
//
// This is intentionally NON-DETERMINISTIC at the end angle (±5–30°). The
// choreographer's next phase re-finds the satellite from wherever the
// rotor lands, so precision is unnecessary here.
//
// Direction is randomized at start: half the time CW, half CCW. The
// caller (Choreographer) seeds the RNG once at boot.
//
// Pre/post motor state contract:
//   - start(): caller must have motor OFF. We switch to speed mode, write
//     loose PID + max current, target speed 0, then enable output.
//   - end (after SETTLE): motor stays in speed mode at 0 RPM, output ON.
//     Caller is responsible for setOutput(false) (release) or switching
//     back to position mode for the next phase.
#pragma once

#include <Arduino.h>

class ChoreoSpin {
 public:
  ChoreoSpin() = default;

  void startRandom();      // pick CW/CCW at random and kick off the spin
  void tick();             // call every main-loop iteration; no-op when idle
  bool isActive() const { return _phase != SP_IDLE; }
  float direction() const { return _direction; }   // +1.0f CW, -1.0f CCW

 private:
  enum Phase {
    SP_IDLE,
    SP_RAMP_UP,
    SP_CRUISE,
    SP_RAMP_DOWN,
    SP_SETTLE,
  };

  Phase    _phase           = SP_IDLE;
  float    _direction       = 1.0f;     // +1 CW, -1 CCW
  float    _startEncDeg     = 0.0f;     // raw cumulative encoder at spin start
  float    _curSpeedRPM     = 0.0f;
  float    _startSpeedRPM   = 0.0f;
  float    _targetSpeedRPM  = 0.0f;
  uint32_t _phaseStartMS    = 0;
};
