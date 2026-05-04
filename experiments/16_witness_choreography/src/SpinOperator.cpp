#include "SpinOperator.h"

void SpinOperator::start() {
  // Bump the position-mode current cap so the brake at the end has authority.
  // Restored to the production cap when the spin completes.
  MotorIO::setPosMaxCurrentMA(Recipes::SPIN_BRAKE_MAX_MA);
  delay(10);

  _startTargetDeg     = _m.currentTargetDeg;
  _targetEndDeg       = _m.currentTargetDeg + Recipes::SPIN_TOTAL_DEG;
  _startSpeedRPM      = 0.0f;
  _currentSpeedRPM    = 0.0f;
  _targetSpeedRPM     = Recipes::SPIN_CRUISE_RPM;
  _transitionStartMS  = millis();
  _inTransition       = true;
  _phaseStartMS       = millis();
  _m.state            = ST_SPIN_RAMP_UP;

  Serial.print(">> SPIN1080 start=");
  Serial.print(_startTargetDeg, 2);
  Serial.print(" deg  target=");
  Serial.print(_targetEndDeg, 2);
  Serial.println(" deg");
}

void SpinOperator::tick() {
  if (!isActive()) return;

  // Velocity ramp (cosine ease 0 → 8 → freeze).
  if (_inTransition) {
    uint32_t elapsed_t = millis() - _transitionStartMS;
    if (elapsed_t >= Recipes::SPIN_RAMP_MS) {
      _currentSpeedRPM = _targetSpeedRPM;
      _inTransition    = false;
    } else {
      float prog  = static_cast<float>(elapsed_t) /
                    static_cast<float>(Recipes::SPIN_RAMP_MS);
      float eased = MotorIO::easeInOut(prog);
      _currentSpeedRPM = _startSpeedRPM +
                         (_targetSpeedRPM - _startSpeedRPM) * eased;
    }
  }

  // Setpoint advance during ramp_up + cruise (RPM × 6 deg/s × 0.01 s).
  if (_m.state == ST_SPIN_RAMP_UP || _m.state == ST_SPIN_CRUISE) {
    _m.currentTargetDeg += _currentSpeedRPM * 0.06f;
  }
  _m.writeTargetDeg(_m.currentTargetDeg);

  // Phase transitions.
  if (_m.state == ST_SPIN_RAMP_UP && !_inTransition) {
    _m.state       = ST_SPIN_CRUISE;
    _phaseStartMS  = millis();
    Serial.print(">> SPIN cruise  enc=");
    Serial.println(_m.readEncoderDeg(), 2);
  }

  if (_m.state == ST_SPIN_CRUISE) {
    if (_m.currentTargetDeg - _startTargetDeg >= Recipes::SPIN_TOTAL_DEG) {
      // BRAKE: snap setpoint to exact target, zero velocity.
      _m.currentTargetDeg = _targetEndDeg;
      _m.writeTargetDeg(_m.currentTargetDeg);
      _currentSpeedRPM = 0.0f;
      _targetSpeedRPM  = 0.0f;
      _inTransition    = false;
      _brakeEnc        = _m.readEncoderDeg();
      _brakeRPM        = MotorIO::actualRPM();
      _m.state         = ST_SPIN_BRAKE;
      _phaseStartMS    = millis();
      Serial.print(">> SPIN brake   enc=");
      Serial.println(_brakeEnc, 2);
    }
  }

  if (_m.state == ST_SPIN_BRAKE) {
    if (millis() - _phaseStartMS >= Recipes::SPIN_BRAKE_GRACE_MS) {
      _m.state      = ST_SPIN_SETTLE;
      _phaseStartMS = millis();
    }
  }

  if (_m.state == ST_SPIN_SETTLE) {
    if (millis() - _phaseStartMS >= Recipes::SPIN_SETTLE_MS) {
      float finalEnc = _m.readEncoderDeg();
      float err      = finalEnc - _targetEndDeg;
      float physDeg  = fmodf(finalEnc, 360.0f);
      if (physDeg < 0) physDeg += 360.0f;
      Serial.print(">> SPIN done.   final=");
      Serial.print(finalEnc, 2);
      Serial.print(" deg  err=");
      Serial.print(err, 2);
      Serial.print(" deg  phys-from-zero=");
      Serial.println(physDeg, 2);
      // Restore production current cap for the steady hold.
      MotorIO::setPosMaxCurrentMA(Recipes::PROD_MC_MA);
      _m.state = ST_HOLDING;
    }
  }
}
