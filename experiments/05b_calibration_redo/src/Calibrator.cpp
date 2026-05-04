#include "Calibrator.h"

void Calibrator::enterRelease() {
  MotorIO::setOutput(false);
  _m.state            = ST_RELEASED;
  _m.zeroSet          = false;
  _m.currentTargetDeg = 0.0f;
}

void Calibrator::enterHoldHere() {
  MotorIO::setOutput(false);
  delay(20);
  MotorIO::setMode(MotorIO::MODE_POSITION);
  delay(20);
  MotorIO::setPosMaxCurrentMA(Recipes::PROD_MC_MA);
  delay(20);

  _m.zeroOffsetCounts = MotorIO::encoderCounts();
  MotorIO::setPosTarget(_m.zeroOffsetCounts);
  delay(20);

  MotorIO::setOutput(true);

  _m.zeroSet          = true;
  _m.currentTargetDeg = 0.0f;
  _m.state            = ST_HOLDING;
}

void Calibrator::startHomeRitual() {
  enterHoldHere();
  delay(500);
  _applyHoming = true;
  Serial.println(">> Home ritual: latch -> 2-rev diag -> move to visual up -> 1080 spin.");
  startDiagSweep();
}

void Calibrator::startDiagOnly() {
  if (!_m.zeroSet) {
    Serial.println(">> ERROR: hold first (run `hold`) so we have a reference angle.");
    return;
  }
  Serial.println(">> Starting diagnostic two-direction sweep (no homing).");
  _applyHoming = false;
  startDiagSweep();
}

void Calibrator::startDiagSweep() {
  // Switch to the looser diag PID without jolting the rotor.
  int32_t cur_enc = MotorIO::safeSwitchToDiagPID();
  _m.usingDiagPid     = true;
  _m.currentTargetDeg = static_cast<float>(cur_enc - _m.zeroOffsetCounts) /
                        static_cast<float>(MotorIO::COUNTS_PER_DEG);

  _fit.reset();
  _diagStartDeg = _m.currentTargetDeg;

  // CW sweep: DIAG_REVS_PER_DIRECTION revs forward.
  float sweepDeg = 360.0f * Recipes::DIAG_REVS_PER_DIRECTION;
  _moveStartDeg = _diagStartDeg;
  _moveEndDeg   = _diagStartDeg + sweepDeg;
  _moveStartMS  = millis();
  _moveDurMS    = static_cast<uint32_t>(sweepDeg * Recipes::DIAG_MT_PER_DEG_MS);
  _m.state      = ST_DIAG_CW;
}

void Calibrator::tick() {
  if (_m.state == ST_DIAG_PAUSE) {
    _m.writeTargetDeg(_m.currentTargetDeg);
    if (millis() - _pauseStartMS >= _pauseDurMS) {
      // Begin CCW: sweep back to diagStartDeg through the same number of revs.
      float sweepDeg = 360.0f * Recipes::DIAG_REVS_PER_DIRECTION;
      _moveStartDeg = _m.currentTargetDeg;
      _moveEndDeg   = _diagStartDeg;
      _moveStartMS  = millis();
      _moveDurMS    = static_cast<uint32_t>(sweepDeg * Recipes::DIAG_MT_PER_DEG_MS);
      _m.state      = ST_DIAG_CCW;
    }
    return;
  }

  if (_m.state != ST_DIAG_CW && _m.state != ST_DIAG_CCW) return;

  // Sample (encoder, current) at 200 ms cadence to feed the sinusoid fit.
  // Skip the first DIAG_FIT_SKIP_FRACTION of each sweep — those samples
  // include the static-friction breakthrough and bias the fit.
  if (millis() - _lastSampleMS >= 200) {
    _lastSampleMS = millis();
    uint32_t elapsedSweep = millis() - _moveStartMS;
    float progress = (_moveDurMS > 0)
                       ? static_cast<float>(elapsedSweep) /
                         static_cast<float>(_moveDurMS)
                       : 1.0f;
    if (progress >= Recipes::DIAG_FIT_SKIP_FRACTION) {
      _fit.accumulate(_m.readEncoderDeg(), MotorIO::currentMA());
    }
  }

  // Trajectory interpolation (diagnostic PID is much looser, but the
  // setpoint trajectory is the same cosine ease).
  uint32_t elapsed = millis() - _moveStartMS;
  if (elapsed >= _moveDurMS) {
    _m.currentTargetDeg = _moveEndDeg;
    _m.writeTargetDeg(_m.currentTargetDeg);
    if (_m.state == ST_DIAG_CW) {
      _pauseStartMS = millis();
      _pauseDurMS   = Recipes::DIAG_PAUSE_MS;
      _m.state      = ST_DIAG_PAUSE;
    } else {
      onDiagComplete();
    }
    return;
  }

  float progress = static_cast<float>(elapsed) / static_cast<float>(_moveDurMS);
  float eased    = MotorIO::easeInOut(progress);
  float sp       = _moveStartDeg + (_moveEndDeg - _moveStartDeg) * eased;
  _m.writeTargetDeg(sp);
}

void Calibrator::onDiagComplete() {
  // Solve directly so we can print amplitude alongside gravity-up.
  // Amplitude = sqrt(a^2 + b^2) is the gravity-induced current swing in mA.
  // Higher amplitude = stronger gravity signal = more reliable fit. If you
  // see amplitude near zero, the pointer isn't loading the motor as expected
  // (mass distribution off, mechanical bind, or sweep too fast).
  float a = NAN, b = NAN, dc = NAN;
  bool ok = _fit.solve(a, b, dc);

  if (!ok) {
    Serial.println(">> Sinusoid fit FAILED (insufficient data).");
    MotorIO::safeSwitchToProductionPID();
    _m.usingDiagPid = false;
    _m.state        = ST_HOLDING;
    return;
  }

  float gravUserDeg = _fit.gravityUpDeg();
  float gravSigned  = gravUserDeg;
  if (gravSigned > 180.0f) gravSigned -= 360.0f;
  float amplitudeMA = sqrtf(a * a + b * b);

  Serial.print(">> fit: gravity-up at user-frame ");
  Serial.print(gravSigned, 2);
  Serial.print(" deg ; mass_offset_deg ");
  Serial.print(_cal.massOffsetDeg(), 2);
  Serial.print(" ; fit_N ");
  Serial.println(_fit.sampleCount());
  Serial.print(">> fit quality: amplitude=");
  Serial.print(amplitudeMA, 1);
  Serial.print(" mA  dc=");
  Serial.print(dc, 1);
  Serial.print(" mA  (a=");
  Serial.print(a, 2);
  Serial.print(" b=");
  Serial.print(b, 2);
  Serial.println(")");

  if (_applyHoming) {
    float visualUpDegOldFrame = gravSigned - _cal.massOffsetDeg();
    Serial.print(">> visual-up at old user-frame ");
    Serial.print(visualUpDegOldFrame, 2);
    Serial.println(" deg ; re-anchoring zero and moving there.");

    // Re-anchor: shift zeroOffsetCounts so old-frame visual-up becomes new user 0.
    _m.zeroOffsetCounts +=
        static_cast<int32_t>(visualUpDegOldFrame *
                             static_cast<float>(MotorIO::COUNTS_PER_DEG));

    // Where we are now in the NEW user frame:
    _m.currentTargetDeg = _m.readEncoderDeg();

    // Restore production PID (also re-pins target → no jolt).
    MotorIO::safeSwitchToProductionPID();
    _m.usingDiagPid = false;
    _applyHoming    = false;

    // Plan the move to user 0 (visual up). MoveOperator handles the cosine
    // ease + hold; we set the pendingSpin flag so the post-move handler
    // chains a 1080° spin.
    float distance = fabsf(_m.currentTargetDeg);
    if (distance > 180.0f) distance = 360.0f - distance;
    uint32_t mt = static_cast<uint32_t>(distance * Recipes::PROD_MT_PER_DEG_MS);
    if (mt < 200) mt = 200;
    Serial.print(">> moving to visual up (");
    Serial.print(_m.currentTargetDeg, 2);
    Serial.print(" -> 0 deg, mt=");
    Serial.print(mt);
    Serial.println(" ms)");
    _move.setPendingSpin(true);
    _move.moveTo(0.0f, mt);
  } else {
    // Diagnostic only — restore production PID and hold.
    MotorIO::safeSwitchToProductionPID();
    _m.usingDiagPid = false;
    _m.state        = ST_HOLDING;
    Serial.println(">> Diagnostic complete. Motor holding (no homing applied).");
  }
}
