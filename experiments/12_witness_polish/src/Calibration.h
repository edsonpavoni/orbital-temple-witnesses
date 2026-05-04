// Calibration.h — NVS persistence of the per-pointer mass-offset constant.
// `mass_offset_deg` is the angle from gravity-up (where the heavy mass
// settles when straight up against gravity) to visual-up (where the
// pointer tip is straight up). Fixed property of how the pointer is
// mounted; calibrate once via the recalibration procedure (see README.md)
// and persist via `savecal`.
#pragma once

#include <Arduino.h>

class Calibration {
 public:
  static constexpr float DEFAULT_MASS_OFFSET_DEG = 21.74f;

  void   begin();                    // load from NVS at boot
  float  massOffsetDeg() const { return _massOffsetDeg; }
  void   setMassOffsetDeg(float v) { _massOffsetDeg = v; }   // RAM only
  void   save();                     // persist current value
  void   load();                     // reload from NVS

 private:
  float _massOffsetDeg = DEFAULT_MASS_OFFSET_DEG;
};
