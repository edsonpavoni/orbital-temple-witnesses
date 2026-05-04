// Choreographer.h — orchestrates the recurring ritual of the sculpture.
//
// After the boot ritual completes (calibration sweep + 1080° landing at
// visual zero), the choreographer takes over and runs forever:
//
//   ENTER_SAT    — Tracker's 8 s cosine ease from current angle to live
//                  satellite az. (Falls through immediately if there's no
//                  schedule or no clock — pointer just holds where it is.)
//   HOLD_SAT     — freeze at the captured az for 30 s. (Tracker is
//                  disabled the moment its eased entry completes — so the
//                  pointer locks at the live az at that exact moment, not
//                  a continuously-updating one.)
//   RELEASE_1    — REG_OUTPUT=0 for 2 s. The unbalanced pointer free-falls
//                  under gravity to its stable equilibrium ("gravity-down"),
//                  visible as a deliberate "drop" gesture.
//   SPIN         — ChoreoSpin: 07-style speed-mode 1080° rotation. Random
//                  CW/CCW direction. Loose PID, gravity-affected swing.
//                  ±5–30° landing — non-deterministic, deliberately.
//   (no second release — per Edson 2026-05-04: pre-spin drop is the
//    gesture; a second drop just before re-finding the sat is redundant.
//    After spin, re-engage directly in position mode and loop to ENTER_SAT.)
//
// Total cycle ≈ 52 s.
//
// Schedule degradation: if Tracker can't engage (no schedule cached, no
// clock), the ENTER_SAT phase is skipped and the pointer simply holds at
// whatever angle it landed after the previous spin. The ritual continues —
// the artwork is alive even fully offline.
#pragma once

#include <Arduino.h>

#include "ChoreoSpin.h"
#include "MotorState.h"
#include "MoveOperator.h"
#include "ScheduleStore.h"
#include "Tracker.h"

class Choreographer {
 public:
  Choreographer(MotorState& m, Tracker& tr, ChoreoSpin& sp,
                MoveOperator& mv, ScheduleStore& sched)
      : _m(m), _tracker(tr), _spin(sp), _move(mv), _schedule(sched) {}

  void start();             // begin the loop (called once after boot ritual)
  void stop();              // halt the loop (motor stays in its current state)
  bool isActive() const { return _phase != C_IDLE; }

  // Called every main-loop iteration. Drives ChoreoSpin internally; caller
  // does NOT need to tick the spin separately. Tracker is ticked by main.cpp
  // independently (via tracker.tick(nowUTC)) — this is intentional so the
  // tracker still updates the eased-entry setpoint at its own cadence.
  void tick(uint32_t nowUTC);

  const char* phaseName() const;

 private:
  enum Phase {
    C_IDLE,
    C_ENTER_SAT,
    C_HOLD_SAT,
    C_RELEASE_1,
    C_SPIN,
  };

  void transitionTo(Phase next, const char* reason);
  void engageAtCurrentEncoder();   // re-engage motor in position mode without snapping
  void tryEnterSat(uint32_t nowUTC);

  MotorState&     _m;
  Tracker&        _tracker;
  ChoreoSpin&     _spin;
  MoveOperator&   _move;
  ScheduleStore&  _schedule;

  Phase    _phase        = C_IDLE;
  uint32_t _phaseStartMS = 0;
};
