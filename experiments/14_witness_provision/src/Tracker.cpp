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
  _enterStartDeg = _m.currentTargetDeg;
  _enterStartMS  = millis();
  _entering      = true;
  _m.state       = ST_TRACKING;
  _lastUpdateMS  = 0;     // force immediate update on next tick
  Serial.println(">> Tracker: enabled (8s eased entry)");
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

  if (_entering) {
    // Cosine ease-in-out from the entry start (typically +1080° spin
    // endpoint) to the live satellite az over ENTRY_MS. The end target
    // is itself a moving target, so we re-evaluate it each tick — this
    // gives a smooth start (zero velocity), gradual acceleration, and a
    // smooth handoff into the steady-state tracker (which then takes
    // over with sub-degree per-tick deltas).
    constexpr uint32_t ENTRY_MS = 8000;
    uint32_t elapsed = millis() - _enterStartMS;
    if (elapsed < ENTRY_MS) {
      float prog  = (float)elapsed / (float)ENTRY_MS;
      float eased = 0.5f * (1.0f - cosf(prog * PI));   // 0..1, smooth
      // Unwrap the satellite az near the entry-start frame so the lerp
      // doesn't loop the long way around.
      float wantedNearStart = nearestUnwrap(_enterStartDeg, candidate);
      float target = _enterStartDeg +
                     (wantedNearStart - _enterStartDeg) * eased;
      _m.currentTargetDeg = target;
      _m.writeTargetDeg(target);
      _lastTargetDeg = target;
      return;
    }
    _entering = false;
    Serial.println(">> Tracker: eased entry complete, normal tracking");
  }

  float wanted = nearestUnwrap(_m.currentTargetDeg, candidate);

  // Rate-limit the setpoint advance. The motor's production PID is tuned
  // for tight small-delta tracking; a large step input (e.g. a 360° wrap
  // mid-track, or recovery from a stale schedule) saturates the current
  // cap, peaks at ~50 RPM, and oscillates ±60° about the target. Capping
  // the per-tick delta smears any catch-up across several ticks.
  constexpr float DEG_PER_RPM_PER_SEC = 6.0f;
  const float maxStepDeg = Recipes::TRACK_MAX_RPM * DEG_PER_RPM_PER_SEC *
                           (UPDATE_PERIOD_MS / 1000.0f);
  float delta = wanted - _m.currentTargetDeg;
  if (delta >  maxStepDeg) delta =  maxStepDeg;
  if (delta < -maxStepDeg) delta = -maxStepDeg;
  float target = _m.currentTargetDeg + delta;

  _m.currentTargetDeg = target;
  _m.writeTargetDeg(target);
  _lastTargetDeg = target;
}
