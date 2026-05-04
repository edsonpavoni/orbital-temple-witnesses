# First Witness — Stage 3 Knowledge (IP geolocation)

What was learned adding auto-detection of the observer's lat/lon, so
the sculpture is plug-and-play wherever it gets a Wi-Fi connection
without requiring a per-installation `secrets.h` edit.

Stages 1 and 2 (Wi-Fi tracking + offline replay) are documented in
`STAGE1_KNOWLEDGE.md` and `STAGE2_KNOWLEDGE.md`. Calibration / spin
fundamentals live in `09_witness/KNOWLEDGE.md`.

---

## Behaviour

Three-step priority for the observer location used by the schedule
fetch:

```
1. NVS (set on first online boot via IP geolocation; persists forever)
2. IP geolocation via ipapi.co/json (one-time, on first boot with Wi-Fi)
3. Hardcoded fallback in secrets.h (only if both NVS and IP geo fail)
```

Once the location is in NVS the sculpture knows where it is forever —
even if Wi-Fi never returns. This matches the "schedule cache" model
from Stage 2: get the data once, persist, never depend on the network
again at runtime.

---

## API choice — ipapi.co

Free, HTTPS, returns JSON. Response is ~700 bytes; the only fields we
care about are `latitude` and `longitude`. ArduinoJson with a filter
keeps memory minimal.

```
GET https://ipapi.co/json/
→ { "ip":"...", "city":"Brooklyn",
    "latitude": 40.6799, "longitude": -74.0028,
    "timezone":"America/New_York", ... }
```

Validated 2026-04-30: the API returned `40.6799, -74.0028` for the
Brooklyn test network. Approx 3 km off the hardcoded `40.65, -73.95`
fallback — well within the < 1° azimuth-error budget at the satellite's
~510 km altitude. No degradation in tracking accuracy.

Alternative: `ip-api.com/json/` (HTTP only) or `ipinfo.io/json` (free
tier rate-limited). Stick with ipapi.co for the HTTPS + simplicity.

---

## Storage layout

NVS namespace `geo`, three keys:

```
"lat"     float    detected latitude  (-90..+90)
"lon"     float    detected longitude (-180..+180)
"fetched" uint32_t UTC seconds when the lookup happened
```

Total ~16 bytes including NVS overhead. Trivial.

`Geolocation::loadFromNVS()` validates the persisted values fall in
their plausible ranges before accepting them — guards against a
corrupted partition returning garbage.

---

## Files added in Stage 3

```
src/Geolocation.{h,cpp}    Fetch + persist + forget API.
                           (Full implementation, ~110 lines.)
```

`main.cpp` changes:
- Reads `g_observerLat / g_observerLon` from NVS at setup-time.
- After Wi-Fi connects, if NVS was empty, calls `fetchAndStore()` once.
- Schedule fetch uses the dynamic globals instead of hardcoded
  `OBSERVER_LAT/LON` from `secrets.h`.
- Two new serial commands: `geo` (print current location) and `regeo`
  (wipe NVS slot + re-fetch — for re-deployment to a new physical site).
- `report` now includes the observer location and tags it `(NVS)` or
  `(fallback default)`.

`secrets.h` keeps `OBSERVER_LAT / OBSERVER_LON` as the bottom-of-the-
priority-stack fallback for the case where a brand-new device is
flashed AND its first online attempt fails AND the cache is empty.

No motion / calibration / spin / network code was touched.

---

## Things deferred

- **End-user Wi-Fi onboarding** is now the only remaining blocker for
  shipping a sculpture to a buyer. See `14_witness_provision/`.

---

## Validation (2026-04-30)

Cold-boot from a freshly-erased flash:
```
>> Observer: using fallback lat=40.6500 lon=-73.9500 (will geolocate once Wi-Fi is up)
>> WiFi: connected, IP=..., RSSI=-52 dBm
>> NTP: synced, UTC=...
>> Geolocation: GET https://ipapi.co/json/
>> Geolocation: body bytes=787
>> Geolocation: persisted lat=40.6799 lon=-74.0028
>> Boot ritual: ... → Tracker: enabled
```

After-reboot (NVS populated):
```
>> Observer (NVS): lat=40.6799 lon=-74.0028
... no Geolocation: GET line, no API call ...
```

`geo` serial command prints `lat=40.6799 lon=-74.0028 (NVS)` confirming
persistence. Sculpture tracks the satellite using the auto-detected
location.
