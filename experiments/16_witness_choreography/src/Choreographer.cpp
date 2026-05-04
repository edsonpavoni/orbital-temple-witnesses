#include "Choreographer.h"

#include "MotorIO.h"
#include "Network.h"
#include "Recipes.h"

const char* Choreographer::phaseName() const {
  switch (_phase) {
    case C_IDLE:       return "IDLE";
    case C_ENTER_SAT:  return "ENTER_SAT";
    case C_HOLD_SAT:   return "HOLD_SAT";
    case C_RELEASE_1:  return "RELEASE_1";
    case C_SPIN:       return "SPIN";
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

void Choreographer::engageAtCurrentEncoder() {
  // Pre-condition: motor output is OFF (just exited a RELEASE phase, the
  // pointer has fallen under gravity to wherever).
  // Pin the position target to the current encoder reading so the motor
  // doesn't snap when we re-enable. Apply production PID. Re-enable.
  MotorIO::setMode(MotorIO::MODE_POSITION);
  delay(20);
  int32_t enc = MotorIO::encoderCounts();
  MotorIO::setPosTarget(enc);
  delay(10);
  MotorIO::setPosPID(Recipes::PROD_KP, Recipes::PROD_KI, Recipes::PROD_KD);
  MotorIO::setPosMaxCurrentMA(Recipes::PROD_MC_MA);
  delay(20);
  MotorIO::setOutput(true);
  delay(50);

  // Sync MotorState to the encoder's current position (in user frame).
  // The zero offset persisted from boot calibration is still valid.
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
      // Release the motor — pointer drops under gravity.
      MotorIO::setOutput(false);
      _m.state = ST_RELEASED;
      transitionTo(C_RELEASE_1, "release motor (drop)");
      return;
    }

    case C_RELEASE_1: {
      if (millis() - _phaseStartMS < Recipes::CHOREO_RELEASE_MS) return;
      // Start the 07-style spin (random CW/CCW). ChoreoSpin handles the
      // OFF→speed-mode→loose-PID→enable transition internally.
      _spin.startRandom();
      _m.state = ST_CHOREO_SPIN;     // distinct from ST_SPIN_CRUISE so the
                                     // boot SpinOperator stays inert (its
                                     // tick() only fires for ST_SPIN_*).
      transitionTo(C_SPIN, "begin spin");
      return;
    }

    case C_SPIN: {
      _spin.tick();
      if (_spin.isActive()) return;
      // Spin done. Per Edson 2026-05-04: skip RELEASE_2. The pre-spin drop
      // is the choreographic gesture; a second drop just before re-finding
      // the sat is redundant. Re-engage directly in position mode at the
      // spin endpoint and ask the tracker to ease over to the live az.
      engageAtCurrentEncoder();
      tryEnterSat(nowUTC);
      return;
    }
  }
}
