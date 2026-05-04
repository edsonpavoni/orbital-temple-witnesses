#include "SpeedBrakeSpin.h"

#include "MotorIO.h"
#include "Recipes.h"

// ─── start() ──────────────────────────────────────────────────────────────
//
// Pre-condition: motor is in position mode, output ON (coming from C_HOLD_SAT).
//
// Mode switch sequence:
//   1. Write REG_MODE = SPEED first — no output toggle, rotor stays energised.
//   2. Load loose speed PID (gravity-alive cruise, same as 07-style).
//   3. Set speed max-current.
//   4. Write speed target = 0 (motor holds still for a moment at cruise PID).
//   5. Capture encoder AFTER mode is set and settled — this is the clean origin.
//   6. Begin cosine ramp.

void SpeedBrakeSpin::start() {
  // 1. Pre-zero REG_SPEED while still in position mode (no-op for the active
  //    loop but clears any stale value from the previous spin's cruise phase).
  //    Without this, REG_SPEED holds 8 RPM from cycle N's cruise and the motor
  //    lurches immediately on the mode switch in cycle N+1.
  MotorIO::setSpeedTarget(0.0f);
  delay(2);

  // 2. Switch to speed mode without dropping output.
  MotorIO::setMode(MotorIO::MODE_SPEED);
  delay(5);

  // 3. Load loose speed PID (gravity-alive swing aesthetic).
  MotorIO::setSpeedPID(Recipes::SPIN07_KP, Recipes::SPIN07_KI, Recipes::SPIN07_KD);
  delay(5);

  // 4. Speed max-current (cruise value; BRAKE_MAX_MA applied when braking).
  MotorIO::setSpeedMaxCurrentMA(Recipes::SPIN07_MC_MA);
  delay(5);

  // 5. Confirm zero speed target after mode switch (belt + suspenders).
  MotorIO::setSpeedTarget(0.0f);
  delay(20);

  // 5. Capture encoder origin after mode switch has settled.
  _spinStartEnc  = MotorIO::encoderRawDeg();
  _spinTargetEnc = _spinStartEnc + Recipes::SB_TOTAL_DEG;

  // 6. Kick the ramp.
  _curSpeedRPM  = 0.0f;
  _phaseStartMS = millis();
  _phase        = SP_RAMP_UP;

  Serial.print(">> SpeedBrakeSpin start enc=");
  Serial.print(_spinStartEnc, 2);
  Serial.print(" deg  target=");
  Serial.print(_spinTargetEnc, 2);
  Serial.println(" deg  dir=CW");
}

// ─── fireBrake() ──────────────────────────────────────────────────────────
//
// One-shot position-mode brake sequence from 09_speed_brake.
// Register order is critical (see 09 PLAN.md for rationale):
//   1. PID first — loop starts with correct gains on the very first control cycle.
//   2. Mode switch to POSITION — uses the PID just written.
//   3. Max-current for braking (higher than cruise).
//   4. Frozen setpoint = spinStartEnc + SB_TOTAL_DEG.
// No output toggle — rotor is continuously energised through the switch.

void SpeedBrakeSpin::fireBrake() {
  MotorIO::setPosPID(Recipes::BRAKE_PID_P, Recipes::BRAKE_PID_I, Recipes::BRAKE_PID_D);
  delay(5);
  MotorIO::setMode(MotorIO::MODE_POSITION);
  delay(5);
  MotorIO::setPosMaxCurrentMA(Recipes::BRAKE_MAX_MA);
  delay(5);
  MotorIO::setPosTarget(static_cast<int32_t>(_spinTargetEnc * 100.0f));

  Serial.print(">> SpeedBrakeSpin brake fire -> lock setpoint=");
  Serial.print(_spinTargetEnc, 2);
  Serial.println(" deg");
}

// ─── tick() ───────────────────────────────────────────────────────────────

void SpeedBrakeSpin::tick() {
  if (_phase == SP_IDLE) return;

  switch (_phase) {

    case SP_RAMP_UP: {
      uint32_t elapsed = millis() - _phaseStartMS;
      if (elapsed >= Recipes::SB_RAMP_MS) {
        // Ramp complete — lock to cruise RPM.
        _curSpeedRPM = Recipes::SB_CRUISE_RPM;
        MotorIO::setSpeedTarget(_curSpeedRPM);
        _phaseStartMS = millis();
        _phase        = SP_CRUISE;
        Serial.print(">> SpeedBrakeSpin cruise at ");
        Serial.print(_curSpeedRPM, 1);
        Serial.println(" RPM");
      } else {
        float prog   = static_cast<float>(elapsed) /
                       static_cast<float>(Recipes::SB_RAMP_MS);
        _curSpeedRPM = Recipes::SB_CRUISE_RPM * MotorIO::easeInOut(prog);
        MotorIO::setSpeedTarget(_curSpeedRPM);
      }
      break;
    }

    case SP_CRUISE: {
      // Re-write REG_SPEED every tick. Without this, the motor's speed loop
      // I-term accumulates over the ~37 s cruise and the response to gravity
      // perturbations goes laggy/stiff (visually indistinguishable from
      // position mode). 09 did this in updateMotion() unconditionally.
      MotorIO::setSpeedTarget(_curSpeedRPM);

      // Encoder is cumulative (never wraps). Displacement is always positive
      // (CW only for now), so plain subtraction works.
      float curEnc   = MotorIO::encoderRawDeg();
      float traveled = curEnc - _spinStartEnc;

      if (traveled >= (Recipes::SB_TOTAL_DEG - Recipes::BRAKE_LEAD_DEG)) {
        Serial.print(">> SpeedBrakeSpin brake trigger (traveled=");
        Serial.print(traveled, 2);
        Serial.print(", lead=");
        Serial.print(Recipes::BRAKE_LEAD_DEG, 2);
        Serial.println(")");
        fireBrake();
        _phase        = SP_BRAKE;
        _phaseStartMS = millis();
      }
      break;
    }

    case SP_BRAKE: {
      // BRAKE is a one-shot register write (already done in fireBrake()).
      // Immediately enter SETTLE so the position PID has time to converge.
      _phase        = SP_SETTLE;
      _phaseStartMS = millis();
      break;
    }

    case SP_SETTLE: {
      if (millis() - _phaseStartMS >= Recipes::SB_SETTLE_MS) {
        float finalEnc = MotorIO::encoderRawDeg();
        float err      = finalEnc - _spinTargetEnc;
        Serial.print(">> SpeedBrakeSpin done. final=");
        Serial.print(finalEnc, 2);
        Serial.print("  err=");
        Serial.print(err, 2);
        Serial.println(" deg");
        _phase = SP_IDLE;
      }
      break;
    }

    case SP_IDLE:
      break;
  }
}
