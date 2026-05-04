# 12_witness_polish — TODO

Items deferred from the current implementation. Each is non-blocking;
the firmware works end-to-end without them.

**Resolved in this stage (12):**
- [x] Offline replay mode — virtual UTC anchor + cache loop. Sculpture
      keeps moving even if Wi-Fi never comes back.
- [x] Hourly Wi-Fi reconnect (no excessive polling).
- [x] NVS persistence reliability (WiFi.persistent off, namespace
      clear before write).

**Deferred:**
- Skip calibration on software reset (Edson: "pass" — not now).

## End-user Wi-Fi onboarding

Right now the Wi-Fi SSID and password live in `src/secrets.h` and require
a re-flash to change. Unworkable for any deployment outside the
developer's bench. The sculpture needs a runtime way for an end user to
hand over their network credentials.

Standard ESP32 patterns:

- **WiFiManager / SmartConfig** — on first boot (or whenever stored
  creds fail to connect), the chip raises its own AP named e.g.
  "FirstWitness-XXXX". User joins it on phone, gets a captive portal
  with an SSID picker + password field, submits, ESP32 stores creds in
  NVS and reboots into the customer network.
- **BLE provisioning** — same idea over Bluetooth; requires a small
  companion app or web BLE.
- **QR + temporary AP** — sticker on the sculpture has a QR that opens
  the AP / portal directly.

Need to decide which UX fits the artwork. WiFiManager is the simplest
implementation; BLE + companion app is the most polished.

Also needs:
- A reset path (e.g., long-press the XIAO's USR button) that wipes the
  stored creds and re-enters provisioning mode.
- An indication on the sculpture that it's currently in provisioning vs
  connected vs offline-replay.

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
