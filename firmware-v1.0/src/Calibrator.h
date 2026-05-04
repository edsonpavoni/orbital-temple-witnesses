// Calibrator.h — sensorless gravity-based homing state machine.
//
// On `startHomeRitual()` the calibrator:
//   1. Latches whatever the encoder reads now as user-frame zero.
//   2. Switches to the loose diagnostic PID so gravity-induced lag is
//      legible in current draw.
//   3. Spins 2 full revs CW, pauses 1.5 s, spins 2 revs CCW back to start.
//      Only the second rev of each direction feeds the on-board sinusoid
//      fit (rev 1 is the motor breaking static friction).
//   4. Solves a 3×3 LS sinusoid fit on the (encoder, current) samples,
//      recovers gravity-up angle.
//   5. Applies the persisted mass_offset to compute visual-up.
//   6. Re-anchors the zero offset so user-frame 0 = visual-up.
//   7. Restores the production PID and asks MoveOperator to move there.
//
// On `startDiagOnly()` (research mode) it does steps 2-4, prints the
// gravity-up angle, and stops — used during recalibration.
//
// Validated as 05_calibration. ±2° accuracy from any starting angle.
#pragma once

#include <Arduino.h>

#include "Calibration.h"
#include "MotorIO.h"
#include "MotorState.h"
#include "MoveOperator.h"
#include "SinusoidFit.h"

class Calibrator {
 public:
  Calibrator(MotorState& m, Calibration& cal, MoveOperator& moveOp)
      : _m(m), _cal(cal), _move(moveOp) {}

  // One-shot ritual entry points.
  void enterRelease();                 // disable motor output
  void enterHoldHere();                // latch current encoder as user 0
  void startDiagOnly();                // research: sweep + fit + print, no homing
  void startHomeRitual();              // full homing + queue spin-after-home

  // State machine driver — called every loop iteration.
  void tick();

  bool isDiagPhase() const {
    return _m.state == ST_DIAG_CW ||
           _m.state == ST_DIAG_PAUSE ||
           _m.state == ST_DIAG_CCW;
  }

 private:
  void  startDiagSweep();
  void  onDiagComplete();              // called when CCW finishes

  MotorState&    _m;
  Calibration&   _cal;
  MoveOperator&  _move;
  SinusoidFit    _fit;

  uint32_t _moveStartMS    = 0;
  uint32_t _moveDurMS      = 0;
  float    _moveStartDeg   = 0.0f;
  float    _moveEndDeg     = 0.0f;

  uint32_t _pauseStartMS   = 0;
  uint32_t _pauseDurMS     = 0;
  float    _diagStartDeg   = 0.0f;     // user-frame angle at start of diag
  bool     _applyHoming    = false;    // end-of-CCW: apply offset + move-to-zero?
  uint32_t _lastSampleMS   = 0;        // for the 200 ms fit-sample cadence
};
