#include "ScheduleStore.h"

#include <Preferences.h>

namespace {
const char* NS         = "sched";
const char* KEY_HEADER = "h";
const char* KEY_BLOB   = "blob";
constexpr uint32_t REFETCH_AGE_SEC = 24UL * 3600UL;
}

bool ScheduleStore::begin() {
  return loadFromNVS();
}

bool ScheduleStore::loadFromNVS() {
  Preferences p;
  if (!p.begin(NS, true)) return false;

  size_t hSize = p.getBytesLength(KEY_HEADER);
  if (hSize != sizeof(Header)) { p.end(); return false; }
  p.getBytes(KEY_HEADER, &_h, sizeof(Header));

  if (_h.samples_count == 0 || _h.samples_count > MAX_SAMPLES) {
    p.end();
    return false;
  }
  size_t bSize = p.getBytesLength(KEY_BLOB);
  size_t expected = sizeof(Sample) * _h.samples_count;
  if (bSize != expected) { p.end(); return false; }
  p.getBytes(KEY_BLOB, _samples, expected);
  p.end();

  _loaded = true;
  return true;
}

bool ScheduleStore::saveToNVS() {
  Preferences p;
  if (!p.begin(NS, false)) return false;
  p.putBytes(KEY_HEADER, &_h, sizeof(Header));
  size_t bSize = sizeof(Sample) * _h.samples_count;
  p.putBytes(KEY_BLOB, _samples, bSize);
  p.end();
  return true;
}

bool ScheduleStore::replace(const Header& h, const Sample* samples, uint16_t n) {
  if (n == 0 || n > MAX_SAMPLES) return false;
  _h = h;
  _h.samples_count = n;
  memcpy(_samples, samples, sizeof(Sample) * n);
  _loaded = true;
  return saveToNVS();
}

bool ScheduleStore::isCurrent(uint32_t nowUTC) const {
  if (!_loaded) return false;
  return nowUTC >= _h.valid_from_utc && nowUTC < _h.valid_until_utc;
}

bool ScheduleStore::fetchDue(uint32_t nowUTC) const {
  if (!_loaded) return true;
  // Refresh if fetched ≥24 h ago, OR if we're past valid_until.
  if (nowUTC >= _h.valid_until_utc) return true;
  if (nowUTC > _h.fetched_at_utc &&
      nowUTC - _h.fetched_at_utc >= REFETCH_AGE_SEC) return true;
  return false;
}

bool ScheduleStore::sampleAt(uint32_t nowUTC, float& azDeg, float& elDeg) const {
  if (!_loaded) return false;
  if (_h.samples_count == 0) return false;

  // Compute t = seconds since valid_from_utc, even outside [valid_from,
  // valid_until]. Caller can choose to refuse if isCurrent() is false.
  int64_t t = (int64_t)nowUTC - (int64_t)_h.valid_from_utc;
  if (t < 0) t = 0;

  uint32_t idxF = (uint32_t)(t / _h.samples_interval_sec);
  if (idxF >= (uint32_t)(_h.samples_count - 1)) {
    azDeg = _samples[_h.samples_count - 1].az;
    elDeg = _samples[_h.samples_count - 1].el;
    return true;
  }
  uint32_t idxC = idxF + 1;
  uint32_t baseT = idxF * _h.samples_interval_sec;
  float frac = (float)(t - baseT) / (float)_h.samples_interval_sec;
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;

  // Interpolate elevation linearly. Azimuth needs unwrapping at the 360°
  // wrap so a step from 359° → 1° doesn't lerp through 180°.
  float a0 = _samples[idxF].az;
  float a1 = _samples[idxC].az;
  float diff = a1 - a0;
  if (diff > 180.0f)       a1 -= 360.0f;
  else if (diff < -180.0f) a1 += 360.0f;
  float az = a0 + (a1 - a0) * frac;
  if (az < 0)        az += 360.0f;
  if (az >= 360.0f)  az -= 360.0f;

  azDeg = az;
  elDeg = _samples[idxF].el + (_samples[idxC].el - _samples[idxF].el) * frac;
  return true;
}
