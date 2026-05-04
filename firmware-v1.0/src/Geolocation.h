// Geolocation.h — auto-detect the observer's lat/lon via IP geolocation,
// persist to NVS so it survives across reboots and never needs network
// again. The schedule fetch (and therefore tracking) keys off this
// location; getting it once means the sculpture is plug-and-play.
//
// Source: ipapi.co/json (free, HTTPS, ~3 req/sec rate limit which is
// way more than we'll ever need — we fetch once per device, ever).
//
// Persistence: NVS namespace "geo", float keys "lat" and "lon" plus a
// uint32 "fetched_at" timestamp.
#pragma once

#include <Arduino.h>

namespace Geolocation {

// Read persisted lat/lon from NVS into the out-params. Returns true if
// a previously-stored location was found, false otherwise. The
// out-params are untouched on false.
bool loadFromNVS(float& lat, float& lon);

// HTTP GET ipapi.co/json, parse, persist to NVS, write into out-params.
// Returns true on success. Requires Wi-Fi to be connected.
bool fetchAndStore(float& lat, float& lon, uint32_t nowUTC);

// Wipe persisted location (for re-deployment to a new physical site).
void forget();

}  // namespace Geolocation
