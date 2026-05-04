#include "Choreographer.h"

#include "MotorIO.h"
#include "Network.h"
#include "Recipes.h"

const char* Choreographer::phaseName() const {
  switch (_phase) {
    case C_IDLE:       return "IDLE";
    case C_ENTER_SAT:  return "ENTER_SAT";
    case C_HOLD_SAT:   return "HOLD_SAT";
    case C_SPIN:       return "SPIN";
    case C_SPIN_HOLD:  return "SPIN_HOLD";
  }
  return "?";
}

void Choreographer::transitionTo(Phase next, const char* reason) {
  _phase        = next;
  _phaseStartMS = millis();
  Serial.print(">> Choreo: -> ");
  Serial.print(phaseName());
  Serial.print("  (");
  Serial.print(reason);
  Serial.println(")");
}

void Choreographer::start() {
  Serial.println(">> Choreo: starting cycle");
  // Pre-condition (from boot ritual): motor is in position mode, holding at
  // visual zero, output ON. We just need to pick the first phase.
  tryEnterSat(Network::nowUTC());
}

void Choreographer::stop() {
  _phase = C_IDLE;
  _tracker.disable();
  Serial.println(">> Choreo: stopped (motor left in current state)");
}

void Choreographer::tryEnterSat(uint32_t nowUTC) {
  if (_schedule.hasData() && nowUTC != 0) {
    _tracker.enable();          // Tracker takes over: 8 s cosine ramp to live az
    transitionTo(C_ENTER_SAT, "tracker engaged");
  } else {
    // No schedule / no clock — degrade gracefully. Hold at current angle
    // for the regular HOLD window, then continue the cycle.
    transitionTo(C_HOLD_SAT, "no schedule, holding in place");
  }
}

void Choreographer::restoreProductionState() {
  // Called after C_SPIN_HOLD exits.
  // SpeedBrakeSpin left the motor in position mode with BRAKE_MAX_MA (1500 mA).
  // Restore the production current cap so the rest of the cycle behaves
  // consistently, then sync MotorState so the tracking and move operators
  // have accurate frame-of-reference data.
  MotorIO::setPosMaxCurrentMA(Recipes::PROD_MC_MA);
  delay(10);
  _m.currentTargetDeg = _m.readEncoderDeg();
  _m.state            = ST_HOLDING;
}

void Choreographer::tick(uint32_t nowUTC) {
  switch (_phase) {
    case C_IDLE:
      return;

    case C_ENTER_SAT: {
      // Wait for Tracker's 8 s cosine ease to complete. The Tracker is
      // ticked elsewhere (main.cpp) and updates currentTargetDeg each tick.
      if (_tracker.isEntering()) return;
      // Eased entry done — freeze at the live az captured at this instant.
      _tracker.disable();
      transitionTo(C_HOLD_SAT, "eased entry complete");
      return;
    }

    case C_HOLD_SAT: {
      if (millis() - _phaseStartMS < Recipes::CHOREO_HOLD_MS) return;
      // 30 s hold complete. Fire the spin directly — no drop.
      // Pre-condition for SpeedBrakeSpin::start(): motor is in position mode,
      // output ON. That's exactly the state C_HOLD_SAT leaves us in.
      _spin.start();
      _m.state = ST_CHOREO_SPIN;   // distinct from ST_SPIN_* (boot SpinOperator
                                   // only fires on ST_SPIN_RAMP_UP etc.)
      transitionTo(C_SPIN, "hold done, begin speed-brake spin");
      return;
    }

    case C_SPIN: {
      _spin.tick();
      if (_spin.isActive()) return;
      // Spin done. Motor is in position mode, output ON, at brake target.
      // SYNC: SpeedBrakeSpin wrote REG_POS directly (raw encoder), bypassing
      // _m.currentTargetDeg. MoveOperator fires on ST_HOLDING and writes
      // currentTargetDeg every tick — without this sync, it yanks the rotor
      // ~1800° backward to the stale pre-spin satellite azimuth.
      _m.currentTargetDeg = _m.readEncoderDeg();
      _m.state            = ST_HOLDING;
      transitionTo(C_SPIN_HOLD, "spin complete, entering hold");
      return;
    }

    case C_SPIN_HOLD: {
      if (millis() - _phaseStartMS < Recipes::CHOREO_SPIN_HOLD_MS) return;
      // Post-spin hold done. Restore production current cap and sync state,
      // then re-acquire the satellite (sky has moved during the ~110 s cycle).
      restoreProductionState();
      tryEnterSat(nowUTC);
      return;
    }
  }
}
