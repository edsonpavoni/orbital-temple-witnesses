// MoveOperator.h — production move-to-target with cosine S-curve easing.
// Used after homing to land at visual-up, and on demand via the `move`
// serial command. PID gains are the production set (kp=30M, validated by
// 04_position_lab for ±0.5° accuracy from any starting angle).
//
// The state machine for a move is just a duration-bounded interpolation:
//   t=0: state=ST_MOVING, currentTargetDeg starts moving from moveStartDeg
//   t=mt: target = moveEndDeg, state→ST_HOLDING
// At each tick we write the eased setpoint to the motor's position
// register; the motor's position PID does the actual tracking.
//
// Holds an optional "pendingSpin" flag the boot ritual sets so that
// move-to-zero auto-triggers the 1080° spin.
#pragma once

#include <Arduino.h>

#include "MotorIO.h"
#include "MotorState.h"

class MoveOperator {
 public:
  explicit MoveOperator(MotorState& m) : _m(m) {}

  void moveTo(float toDeg, uint32_t mt);
  void tick();

  bool isMoving() const { return _m.state == ST_MOVING; }
  bool consumePendingSpin();              // returns true ONCE if the flag was set

  void setPendingSpin(bool v) { _pendingSpin = v; }

 private:
  MotorState& _m;
  float    _moveStartDeg = 0.0f;
  float    _moveEndDeg   = 0.0f;
  uint32_t _moveStartMS  = 0;
  uint32_t _moveDurMS    = 0;
  bool     _pendingSpin  = false;
};
