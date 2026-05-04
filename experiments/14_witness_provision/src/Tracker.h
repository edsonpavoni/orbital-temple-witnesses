// Tracker.h — drives the pointer to "wherever the satellite is right now."
//
// When enabled (typically after the boot ritual completes), the tracker
// computes the current azimuth from the cached schedule + current UTC,
// maps it into the sculpture's user frame, and updates the position
// setpoint each loop iteration. The motor's position PID smooths the
// motion between updates.
//
// Sign convention: user-frame zero == visual vertical. Compass azimuth
// is 0=N, +east. The pointer's az → user-frame mapping is configurable
// (TRACKER_AZ_SIGN) so we can flip if the motor's positive direction is
// CCW.
#pragma once

#include <Arduino.h>

#include "MotorState.h"
#include "MoveOperator.h"
#include "ScheduleStore.h"

class Tracker {
 public:
  Tracker(MotorState& m, MoveOperator& moveOp, ScheduleStore& store)
      : _m(m), _move(moveOp), _store(store) {}

  void enable();        // enter ST_TRACKING; subsequent ticks update setpoint
  void disable();       // ST_TRACKING -> ST_HOLDING

  // Called every loop iteration. No-op outside ST_TRACKING.
  void tick(uint32_t nowUTC);

  bool isActive() const { return _m.state == ST_TRACKING; }
  float lastAz() const  { return _lastAz; }
  float lastEl() const  { return _lastEl; }

  // Sign of the az → user-frame mapping. false means user_target = +az
  // (default, validated 2026-04-30: positive user-frame rotates the rotor
  // CW from visual-up, which matches the visualization's CW rotation for
  // positive az). Set true if the motor wiring is reversed.
  void setSignFlip(bool negate) { _negate = negate; }

 private:
  MotorState&     _m;
  MoveOperator&   _move;
  ScheduleStore&  _store;
  bool            _negate         = false;    // default: target = +az (CW = +)
  uint32_t        _lastUpdateMS   = 0;
  float           _lastAz         = 0.0f;
  float           _lastEl         = 0.0f;
  float           _lastTargetDeg  = 0.0f;

  // Eased entry: on enable(), smoothly cosine-ramp from the current motor
  // target (typically the +1080° spin endpoint) toward the satellite az
  // over a few seconds, instead of letting the rate-limit snap-catch-up.
  bool            _entering       = false;
  uint32_t        _enterStartMS   = 0;
  float           _enterStartDeg  = 0.0f;
};
