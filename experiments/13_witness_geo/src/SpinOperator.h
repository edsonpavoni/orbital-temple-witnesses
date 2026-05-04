// SpinOperator.h — 1080° rate-integrated spin with brake-style stop.
//
// Position-mode rate-integration: each 100 Hz tick we advance currentTargetDeg
// by currentSpeedRPM × 6 deg/s × 0.01 s. Motor's production position PID
// tracks that slowly-moving setpoint at a controlled 8 RPM average. At end
// of cruise we freeze the setpoint at exactly start + 1080°; PID's reverse
// current arrests the rotor's residual kinetic energy in ~1°.
//
// Validated as 08_brake1080. See that README for design rationale.
#pragma once

#include <Arduino.h>

#include "MotorIO.h"
#include "MotorState.h"
#include "Recipes.h"

class SpinOperator {
 public:
  explicit SpinOperator(MotorState& m) : _m(m) {}

  void start();
  void tick();
  bool isActive() const {
    return _m.state == ST_SPIN_RAMP_UP ||
           _m.state == ST_SPIN_CRUISE  ||
           _m.state == ST_SPIN_BRAKE   ||
           _m.state == ST_SPIN_SETTLE;
  }

 private:
  MotorState& _m;
  float    _startTargetDeg     = 0.0f;
  float    _targetEndDeg       = 0.0f;
  float    _currentSpeedRPM    = 0.0f;
  float    _startSpeedRPM      = 0.0f;
  float    _targetSpeedRPM     = 0.0f;
  bool     _inTransition       = false;
  uint32_t _transitionStartMS  = 0;
  uint32_t _phaseStartMS       = 0;
  float    _brakeEnc           = 0.0f;
  float    _brakeRPM           = 0.0f;
};
