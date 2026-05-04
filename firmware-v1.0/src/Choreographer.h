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
//   SPIN         — SpeedBrakeSpin: speed-mode cruise (gravity-alive, loose
//                  PID) then position-mode brake. 1800° CW, deterministic
//                  landing. ~42 s total. Motor stays output-ON throughout
//                  — no drop before the spin.
//   SPIN_HOLD    — 30 s position-mode hold at the brake landing angle.
//                  Choreographer owns this window; SpeedBrakeSpin is done.
//
//   After SPIN_HOLD, re-acquire next satellite from Tracker/ScheduleStore
//   (live, sky has moved) and loop back to ENTER_SAT.
//
// Total cycle: ENTER_SAT (~8 s) + HOLD_SAT (30 s) + SPIN (~42 s) +
//              SPIN_HOLD (30 s) = ~110 s.
//
// Schedule degradation: if Tracker can't engage (no schedule cached, no
// clock), the ENTER_SAT phase is skipped and the pointer holds at
// whatever angle it landed from the previous cycle. The ritual continues
// — the artwork is alive even fully offline.
#pragma once

#include <Arduino.h>

#include "MotorState.h"
#include "MoveOperator.h"
#include "ScheduleStore.h"
#include "SpeedBrakeSpin.h"
#include "Tracker.h"

class Choreographer {
 public:
  Choreographer(MotorState& m, Tracker& tr, SpeedBrakeSpin& sp,
                MoveOperator& mv, ScheduleStore& sched)
      : _m(m), _tracker(tr), _spin(sp), _move(mv), _schedule(sched) {}

  void start();              // begin the loop (called once after boot ritual)
  void stop();               // halt the loop (motor stays in current state)
  bool isActive() const { return _phase != C_IDLE; }

  // Called every main-loop iteration. Drives SpeedBrakeSpin internally;
  // caller does NOT need to tick the spin separately. Tracker is ticked by
  // main.cpp independently (via tracker.tick(nowUTC)).
  void tick(uint32_t nowUTC);

  const char* phaseName() const;

 private:
  enum Phase {
    C_IDLE,
    C_ENTER_SAT,
    C_HOLD_SAT,
    C_SPIN,
    C_SPIN_HOLD,
  };

  void transitionTo(Phase next, const char* reason);
  void tryEnterSat(uint32_t nowUTC);
  void restoreProductionState();  // restore PROD_MC_MA + sync MotorState after spin hold

  MotorState&     _m;
  Tracker&        _tracker;
  SpeedBrakeSpin& _spin;
  MoveOperator&   _move;
  ScheduleStore&  _schedule;

  Phase    _phase        = C_IDLE;
  uint32_t _phaseStartMS = 0;
};
