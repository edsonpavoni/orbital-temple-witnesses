// MotorState.h — shared state between the motion subsystems. Calibrator
// owns the zero offset (sets it during homing); MoveOperator and
// SpinOperator both read/write currentTargetDeg as they advance the
// position setpoint. The State enum is shared so logger + main.cpp can
// dispatch on the active phase.
//
// This is plain shared state — no behaviour. Behaviour lives in the
// operator classes that hold a reference to a MotorState.
#pragma once

#include <Arduino.h>

#include "MotorIO.h"

enum State {
  ST_RELEASED = 0,
  ST_HOLDING,
  ST_MOVING,
  ST_DIAG_CW,
  ST_DIAG_PAUSE,
  ST_DIAG_CCW,
  ST_SPIN_RAMP_UP,
  ST_SPIN_CRUISE,
  ST_SPIN_BRAKE,
  ST_SPIN_SETTLE,
  ST_TRACKING,           // follow-the-satellite mode (Tracker-driven)
};

inline char phaseCode(State s) {
  switch (s) {
    case ST_RELEASED:     return 'R';
    case ST_HOLDING:      return 'H';
    case ST_MOVING:       return 'M';
    case ST_DIAG_CW:      return 'C';
    case ST_DIAG_PAUSE:   return 'P';
    case ST_DIAG_CCW:     return 'A';
    case ST_SPIN_RAMP_UP: return 'u';
    case ST_SPIN_CRUISE:  return 'S';
    case ST_SPIN_BRAKE:   return 'B';
    case ST_SPIN_SETTLE:  return 'W';
    case ST_TRACKING:     return 'T';
  }
  return '?';
}

struct MotorState {
  State    state            = ST_RELEASED;
  bool     zeroSet          = false;
  int32_t  zeroOffsetCounts = 0;       // Encoder raw counts mapping to user 0 deg
  float    currentTargetDeg = 0.0f;    // Logical user-frame target / setpoint
  bool     usingDiagPid     = false;

  // Helpers — encoder ↔ user frame conversion using the current zero offset.
  inline void writeTargetDeg(float deg) const {
    int32_t enc = zeroOffsetCounts +
                  static_cast<int32_t>(deg * static_cast<float>(MotorIO::COUNTS_PER_DEG));
    MotorIO::setPosTarget(enc);
  }
  inline float readEncoderDeg() const {
    return static_cast<float>(MotorIO::encoderCounts() - zeroOffsetCounts) /
           static_cast<float>(MotorIO::COUNTS_PER_DEG);
  }
};
