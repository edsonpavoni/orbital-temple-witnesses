#include "Tracker.h"

#include "MotorIO.h"
#include "Recipes.h"

namespace {
// Update setpoint at most this often. The position PID interpolates
// between successive setpoints, so we don't need every-tick updates.
// The satellite's az changes by < 1° per second outside an overhead
// pass, so 200 ms is plenty.
constexpr uint32_t UPDATE_PERIOD_MS = 200;

// Pick the equivalent target angle nearest to the current setpoint,
// adjusted by ±360° as needed. Without this, after a few hundred
// degrees the rotor would unwind backwards every time az wraps.
float nearestUnwrap(float currentDeg, float candidateDeg) {
  while (candidateDeg - currentDeg > 180.0f)  candidateDeg -= 360.0f;
  while (candidateDeg - currentDeg < -180.0f) candidateDeg += 360.0f;
  return candidateDeg;
}
}  // namespace

void Tracker::enable() {
  if (!_store.hasData()) return;
  _m.state = ST_TRACKING;
  _lastUpdateMS = 0;     // force immediate update on next tick
  Serial.println(">> Tracker: enabled");
}

void Tracker::disable() {
  if (_m.state == ST_TRACKING) {
    _m.state = ST_HOLDING;
    Serial.println(">> Tracker: disabled, holding");
  }
}

void Tracker::tick(uint32_t nowUTC) {
  if (_m.state != ST_TRACKING) return;
  if (nowUTC == 0) return;             // need synced time
  if (millis() - _lastUpdateMS < UPDATE_PERIOD_MS) return;
  _lastUpdateMS = millis();

  float az = 0, el = 0;
  if (!_store.sampleAt(nowUTC, az, el)) return;
  _lastAz = az;
  _lastEl = el;

  // Map compass azimuth into user-frame degrees, picking the unwrap that's
  // closest to the current setpoint to avoid 360° lurches.
  float candidate = _negate ? -az : az;
  float target    = nearestUnwrap(_m.currentTargetDeg, candidate);

  _m.currentTargetDeg = target;
  _m.writeTargetDeg(target);
  _lastTargetDeg = target;
}
