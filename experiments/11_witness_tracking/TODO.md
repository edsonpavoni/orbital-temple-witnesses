# 11_witness_tracking — TODO

Items deferred from the current implementation. Each is non-blocking;
the firmware works end-to-end without them.

## Calibration only on real power cycle

Right now every boot (including dev reflashes) runs the full 110 s
calibration ritual. Should only run on a *cold power cycle*, not when
the firmware was just reflashed and the rotor is already at a known
calibrated position.

Approach: persist the post-homing state to NVS — last known
`zeroOffsetCounts`, plus a "calibrated" flag and a wallclock timestamp.
On boot, if NVS says we were calibrated and the rotor's current encoder
reading is within tolerance of where we left off, skip the diag sweep
and re-anchor zero from NVS. Otherwise (rotor moved while powered down,
or first boot ever), run the full ritual.

Detection of "real power cycle vs reflash" can use:
- ESP32's reset reason (`esp_reset_reason()`): `ESP_RST_POWERON` =
  cold power cycle, `ESP_RST_SW` = software reset (after flash).
- Or just always trust NVS state and check encoder consistency.

Either approach saves ~110 s per reflash during development and avoids
unnecessary motor wear in production.

## Auto-detect observer location

Currently `OBSERVER_LAT` / `OBSERVER_LON` are hardcoded in `secrets.h`
to Brooklyn (40.65, -73.95). Should auto-detect via IP geolocation on
first connect:

- After Wi-Fi up, hit a free geo-IP service (e.g.,
  `https://ipapi.co/json/`, `http://ip-api.com/json/`).
- Persist the discovered lat/lon to NVS.
- Use the persisted value on subsequent boots; fall back to the
  hardcoded default if NVS is empty AND the geo-IP lookup fails.

Sculpture is then plug-and-play wherever it gets a Wi-Fi connection —
no per-installation configuration step.

## Wall-orientation offset

The visualization and sculpture share a "pointer-up = 0°" convention,
so they agree visually regardless of where the wall is in real space
(per Edson's call: "our interface should flatten the axis"). If we
ever want the pointer to point at *true compass directions* (e.g., for
when the satellite is overhead and you want to know where to look in
the sky), we'll need a `wall_north_offset_deg` constant + serial
command + NVS slot.

Skipped for now — pointer-relative angle alignment is the artwork's
contract.

## Sign-flip exposed via serial

`Tracker::setSignFlip(bool)` exists but isn't reachable from the serial
console. If the motor ever gets re-wired, currently requires a code
edit + flash. Add a `tracksign +|−` (or `setsign 0|1`) serial command
that writes to NVS so it survives reboots.

## Schedule fetch resilience

The buffered `getString()` approach works but downloads ~50 KB into
`String`. If the device is under heap pressure (e.g., during a future
phase that adds more allocations), this could fail. Robustness path:

- Stream the JSON to LittleFS and parse from a file. Avoids holding
  the whole body in RAM at once.
- Or implement a custom JSON skim parser that walks the bytes and
  pulls just `samples[].az` and `samples[].el`.

Current path is fine until heap usage tells us otherwise.
