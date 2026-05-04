// ScheduleStore.h — persistent storage of the daily satellite schedule.
//
// The /api/schedule endpoint returns ~1440 (t, az, el) samples at 1-min
// resolution covering 24 h from valid_from_utc. We compress to a header
// + samples blob, stash it in NVS, and interpolate between minute samples
// at lookup time.
//
// "Once a day, fetch this. If next day's fetch fails, keep using the
// cached version." The fetched_at_utc timestamp tells the fetch logic
// whether it's time to try again.
#pragma once

#include <Arduino.h>

class ScheduleStore {
 public:
  static constexpr uint16_t MAX_SAMPLES = 1440;
  // 0.1° resolution stored as int16. Float az/el is in [-180,180] / [-90,90],
  // both fit comfortably in [-32768, 32767] at deci-degree scale. Using 4 B
  // per sample (vs 8 B for float pair) lets us keep the full 1-min schedule
  // (1440 × 4 = 5760 B) in NVS alongside Wi-Fi creds, instead of decimating
  // to 5-min and losing accuracy across overhead passes.
  struct Sample {
    int16_t az_deci;
    int16_t el_deci;
    inline float az() const { return az_deci * 0.1f; }
    inline float el() const { return el_deci * 0.1f; }
  };

  // Mirrored in NVS as the header blob.
  struct Header {
    uint32_t valid_from_utc;
    uint32_t valid_until_utc;
    uint32_t fetched_at_utc;
    uint16_t samples_interval_sec;
    uint16_t samples_count;
  };

  bool begin();                                 // load from NVS at boot
  bool hasData() const { return _loaded; }

  // Replace the cached schedule. Persists immediately.
  bool replace(const Header& h, const Sample* samples, uint16_t n);

  // True if nowUTC falls within [valid_from, valid_until].
  bool isCurrent(uint32_t nowUTC) const;

  // True if fetched_at_utc is older than 24 h relative to nowUTC.
  bool fetchDue(uint32_t nowUTC) const;

  // Interpolated sample at nowUTC. Returns false if no data or out of range.
  bool sampleAt(uint32_t nowUTC, float& azDeg, float& elDeg) const;

  const Header& header() const { return _h; }

 private:
  bool   _loaded = false;
  Header _h{};
  Sample _samples[MAX_SAMPLES];

  bool loadFromNVS();
  bool saveToNVS();
};
