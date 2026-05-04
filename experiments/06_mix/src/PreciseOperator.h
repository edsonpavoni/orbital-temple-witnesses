// PreciseOperator.h — Mode 1: closed-loop precision moves.
//
// Recipe from 04_position_lab (test 2H + test 3, ±0.5° accuracy
// from any starting position):
//
//   POSITION mode
//   kp = 30,000,000  ki = 1,000  kd = 40,000,000
//   max current = 1000 mA
//   easing = cosine S-curve
//   mt = 33.33 ms per deg of move distance
//
// Lifecycle:
//   engageHoldHere() -> latch current encoder as user-frame zero, hold
//   moveTo(deg)      -> begin an eased move from currentTargetDeg to deg
//   tick()           -> drive trajectory + write target each loop
//   isMoving()       -> true until the move completes
//   reEngageAfterSpeedMode() -> SmoothOperator calls this after a rotation
//                               to put the motor back under our control;
//                               keeps zero offset, syncs target to current
//                               encoder reading.
//   release()        -> disable motor output, clear engaged flag
#pragma once

#include "MotorIO.h"
#include <Arduino.h>

class PreciseOperator {
 public:
  enum class State { IDLE, MOVING };

  // Recipe constants (production PID validated by 04_position_lab).
  static constexpr int32_t  KP            = 30000000;
  static constexpr int32_t  KI            = 1000;
  static constexpr int32_t  KD            = 40000000;
  static constexpr int32_t  MC_MA         = 1000;
  static constexpr float    MT_PER_DEG_MS = 33.33f;
  static constexpr uint32_t MIN_MT_MS     = 200;

  void engageHoldHere();
  void release();
  void moveTo(float toDeg);
  void tick();
  void reEngageAfterSpeedMode();

  bool  isEngaged() const { return _engaged; }
  bool  isMoving()  const { return _state == State::MOVING; }
  float currentTargetDeg() const { return _currentTargetDeg; }
  float currentSetpointDeg() const;
  int   progressPct() const;
  int32_t zeroOffsetCounts() const { return _zeroOffsetCounts; }

 private:
  bool     _engaged          = false;
  State    _state            = State::IDLE;
  int32_t  _zeroOffsetCounts = 0;
  float    _currentTargetDeg = 0.0f;
  float    _moveStartDeg     = 0.0f;
  float    _moveEndDeg       = 0.0f;
  uint32_t _moveStartMS      = 0;
  uint32_t _moveDurMS        = 0;

  void writeTargetDeg(float deg);
  // Disable output → set POSITION mode → write production PID + max current
  // → pin target = current encoder → re-enable output. Avoids integrator-
  // windup jolt when changing modes.
  void applyPositionModeSafe();
};
