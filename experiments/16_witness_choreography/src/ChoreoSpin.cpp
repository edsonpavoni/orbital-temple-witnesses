#include "ChoreoSpin.h"

#include <esp_random.h>

#include "MotorIO.h"
#include "Recipes.h"

void ChoreoSpin::startRandom() {
  // Random direction. esp_random() draws from the hardware RNG.
  _direction = ((esp_random() & 1u) == 0u) ? 1.0f : -1.0f;

  // Pre-condition: motor output is OFF (Choreographer just finished a
  // RELEASE phase). Switch to speed mode, write the 07 loose recipe,
  // commanded speed 0, then re-enable.
  MotorIO::setMode(MotorIO::MODE_SPEED);
  delay(20);
  MotorIO::setSpeedPID(Recipes::SPIN07_KP, Recipes::SPIN07_KI, Recipes::SPIN07_KD);
  delay(20);
  MotorIO::setSpeedMaxCurrentMA(Recipes::SPIN07_MC_MA);
  delay(20);
  MotorIO::setSpeedTarget(0.0f);
  delay(20);
  MotorIO::setOutput(true);
  delay(50);                              // let the motor latch the new mode

  _startEncDeg     = MotorIO::encoderRawDeg();
  _startSpeedRPM   = 0.0f;
  _curSpeedRPM     = 0.0f;
  _targetSpeedRPM  = Recipes::SPIN07_CRUISE_RPM * _direction;
  _phaseStartMS    = millis();
  _phase           = SP_RAMP_UP;

  Serial.print(">> ChoreoSpin start dir=");
  Serial.print(_direction > 0 ? "CW" : "CCW");
  Serial.print(" startEnc=");
  Serial.print(_startEncDeg, 2);
  Serial.println(" deg");
}

void ChoreoSpin::tick() {
  if (_phase == SP_IDLE) return;

  switch (_phase) {
    case SP_RAMP_UP: {
      uint32_t elapsed = millis() - _phaseStartMS;
      if (elapsed >= Recipes::SPIN07_RAMP_MS) {
        _curSpeedRPM = _targetSpeedRPM;
        MotorIO::setSpeedTarget(_curSpeedRPM);
        _phase        = SP_CRUISE;
        _phaseStartMS = millis();
        Serial.print(">> ChoreoSpin: cruise at ");
        Serial.print(_curSpeedRPM, 1);
        Serial.println(" RPM");
      } else {
        float prog  = static_cast<float>(elapsed) /
                      static_cast<float>(Recipes::SPIN07_RAMP_MS);
        float eased = MotorIO::easeInOut(prog);
        _curSpeedRPM = _startSpeedRPM +
                       (_targetSpeedRPM - _startSpeedRPM) * eased;
        MotorIO::setSpeedTarget(_curSpeedRPM);
      }
      break;
    }

    case SP_CRUISE: {
      // Encoder is cumulative — never wraps. traveled is the magnitude of
      // angular distance moved since spin start, regardless of direction.
      float curEnc   = MotorIO::encoderRawDeg();
      float traveled = (curEnc - _startEncDeg) * _direction;
      if (traveled >= Recipes::SPIN07_CRUISE_END_DEG) {
        _startSpeedRPM  = _curSpeedRPM;
        _targetSpeedRPM = 0.0f;
        _phase          = SP_RAMP_DOWN;
        _phaseStartMS   = millis();
        Serial.print(">> ChoreoSpin: ramp-down (traveled=");
        Serial.print(traveled, 1);
        Serial.println(" deg)");
      }
      break;
    }

    case SP_RAMP_DOWN: {
      uint32_t elapsed = millis() - _phaseStartMS;
      if (elapsed >= Recipes::SPIN07_RAMP_MS) {
        _curSpeedRPM = 0.0f;
        MotorIO::setSpeedTarget(0.0f);
        _phase        = SP_SETTLE;
        _phaseStartMS = millis();
      } else {
        float prog  = static_cast<float>(elapsed) /
                      static_cast<float>(Recipes::SPIN07_RAMP_MS);
        float eased = MotorIO::easeInOut(prog);
        _curSpeedRPM = _startSpeedRPM +
                       (_targetSpeedRPM - _startSpeedRPM) * eased;
        MotorIO::setSpeedTarget(_curSpeedRPM);
      }
      break;
    }

    case SP_SETTLE: {
      if (millis() - _phaseStartMS >= Recipes::SPIN07_SETTLE_MS) {
        float endEnc   = MotorIO::encoderRawDeg();
        float traveled = (endEnc - _startEncDeg) * _direction;
        Serial.print(">> ChoreoSpin done. traveled=");
        Serial.print(traveled, 1);
        Serial.print(" deg  endEnc=");
        Serial.println(endEnc, 2);
        _phase = SP_IDLE;
      }
      break;
    }

    case SP_IDLE:
      break;
  }
}
