#include "MoveOperator.h"

void MoveOperator::moveTo(float toDeg, uint32_t mt) {
  _moveStartDeg = _m.currentTargetDeg;
  _moveEndDeg   = toDeg;
  _moveStartMS  = millis();
  _moveDurMS    = (mt < 200) ? 200 : mt;
  _m.state      = ST_MOVING;
}

void MoveOperator::tick() {
  if (_m.state == ST_HOLDING) {
    // Hold cleanly: keep writing the held target each tick (cheap insurance
    // against a single-byte glitch on the position register).
    _m.writeTargetDeg(_m.currentTargetDeg);
    return;
  }
  if (_m.state != ST_MOVING) return;

  uint32_t elapsed = millis() - _moveStartMS;
  if (elapsed >= _moveDurMS) {
    _m.currentTargetDeg = _moveEndDeg;
    _m.writeTargetDeg(_m.currentTargetDeg);
    _m.state = ST_HOLDING;
    return;
  }
  float progress = static_cast<float>(elapsed) / static_cast<float>(_moveDurMS);
  float eased    = MotorIO::easeInOut(progress);
  float sp       = _moveStartDeg + (_moveEndDeg - _moveStartDeg) * eased;
  _m.writeTargetDeg(sp);
}

bool MoveOperator::consumePendingSpin() {
  if (!_pendingSpin) return false;
  _pendingSpin = false;
  return true;
}
