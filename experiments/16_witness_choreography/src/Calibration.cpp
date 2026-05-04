#include "Calibration.h"

#include <Preferences.h>

namespace {
const char* NVS_NS  = "calib";
const char* NVS_KEY = "mass_offset";
}

void Calibration::begin() {
  load();
}

void Calibration::load() {
  Preferences p;
  p.begin(NVS_NS, true);
  _massOffsetDeg = p.getFloat(NVS_KEY, DEFAULT_MASS_OFFSET_DEG);
  p.end();
}

void Calibration::save() {
  Preferences p;
  p.begin(NVS_NS, false);
  p.putFloat(NVS_KEY, _massOffsetDeg);
  p.end();
}
