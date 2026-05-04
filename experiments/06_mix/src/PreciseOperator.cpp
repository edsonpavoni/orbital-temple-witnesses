#include "PreciseOperator.h"

using namespace MotorIO;

void PreciseOperator::engageHoldHere() {
  applyPositionModeSafe();
  // Latch wherever the rotor currently is as user-frame zero.
  _zeroOffsetCounts = encoderRaw();
  writeReg32(REG_POS, _zeroOffsetCounts);
  delay(20);
  writeReg8(REG_OUTPUT, 1);
  _engaged          = true;
  _state            = State::IDLE;
  _currentTargetDeg = 0.0f;
}

void PreciseOperator::release() {
  writeReg8(REG_OUTPUT, 0);
  _engaged          = false;
  _state            = State::IDLE;
  _currentTargetDeg = 0.0f;
}

void PreciseOperator::moveTo(float toDeg) {
  if (!_engaged) return;
  _moveStartDeg = _currentTargetDeg;
  _moveEndDeg   = toDeg;
  _moveStartMS  = millis();
  float distance = fabsf(toDeg - _currentTargetDeg);
  _moveDurMS    = static_cast<uint32_t>(distance * MT_PER_DEG_MS);
  if (_moveDurMS < MIN_MT_MS) _moveDurMS = MIN_MT_MS;
  _state = State::MOVING;
}

void PreciseOperator::tick() {
  // No-op while disengaged (e.g., during SmoothOperator's speed-mode rotation).
  if (!_engaged) return;

  if (_state == State::MOVING) {
    uint32_t elapsed = millis() - _moveStartMS;
    if (elapsed >= _moveDurMS) {
      _currentTargetDeg = _moveEndDeg;
      writeTargetDeg(_currentTargetDeg);
      _state = State::IDLE;
    } else {
      float progress = static_cast<float>(elapsed) / static_cast<float>(_moveDurMS);
      float eased    = easeInOut(progress);
      float sp       = _moveStartDeg + (_moveEndDeg - _moveStartDeg) * eased;
      writeTargetDeg(sp);
    }
  } else {
    // Idle / holding: rewrite the held target every tick (cheap insurance
    // against single-byte glitches).
    writeTargetDeg(_currentTargetDeg);
  }
}

void PreciseOperator::reEngageAfterSpeedMode() {
  // Motor is currently in SPEED mode (output disabled or about to be).
  // Switch it back to POSITION mode at whatever encoder reading it has now,
  // KEEPING the original zero offset so currentTargetDeg in user-frame
  // continues to mean the same physical angle.
  applyPositionModeSafe();
  int32_t cur = encoderRaw();
  writeReg32(REG_POS, cur);
  delay(20);
  writeReg8(REG_OUTPUT, 1);
  _engaged          = true;
  _state            = State::IDLE;
  _currentTargetDeg =
      static_cast<float>(cur - _zeroOffsetCounts) / static_cast<float>(COUNTS_PER_DEG);
}

float PreciseOperator::currentSetpointDeg() const {
  if (_state != State::MOVING) return _currentTargetDeg;
  uint32_t elapsed = millis() - _moveStartMS;
  if (_moveDurMS == 0 || elapsed >= _moveDurMS) return _moveEndDeg;
  float progress = static_cast<float>(elapsed) / static_cast<float>(_moveDurMS);
  return _moveStartDeg + (_moveEndDeg - _moveStartDeg) * easeInOut(progress);
}

int PreciseOperator::progressPct() const {
  if (_state != State::MOVING || _moveDurMS == 0) return -1;
  uint32_t elapsed = millis() - _moveStartMS;
  if (elapsed >= _moveDurMS) return 100;
  return static_cast<int>(100.0f * elapsed / _moveDurMS);
}

void PreciseOperator::writeTargetDeg(float deg) {
  int32_t enc = _zeroOffsetCounts +
                static_cast<int32_t>(deg * static_cast<float>(COUNTS_PER_DEG));
  writeReg32(REG_POS, enc);
}

void PreciseOperator::applyPositionModeSafe() {
  // Disable output (resets motor's internal integrator)
  writeReg8(REG_OUTPUT, 0);
  delay(20);
  writeReg8(REG_MODE, MODE_POSITION);
  delay(20);
  // Max current
  writeReg32(REG_POS_MAXCUR, MC_MA * 100);
  delay(20);
  // Production PID
  writeReg96(REG_POS_PID, KP, KI, KD);
  delay(20);
  // Caller is responsible for pinning the target and re-enabling output.
}
