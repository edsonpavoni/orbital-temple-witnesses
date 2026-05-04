# First Witness — Stage 1 Knowledge (Wi-Fi + Tracking)

What was learned moving from a self-contained calibrate-and-spin sculpture
to one that follows the satellite live, fed by a webservice.

This file is the journey doc for the tracking stage; the operating
reference lives in `README.md`, the open work in `TODO.md`. The
calibration / spin algorithms inherited from earlier stages are
documented in `09_witness/KNOWLEDGE.md`.

---

## What works end-to-end

On every power-on the firmware runs:

```
power-gate (Vin >= 15 V)
  → Wi-Fi connect (4 attempts, max TX power, RSSI logged)
  → NTP sync (UTC)
  → schedule fetch from /api/schedule (cached in NVS)
  → 3 s pause
  → sensorless gravity homing (re-anchor zero = visual vertical)
  → 1080 deg spin (brake at start angle)
  → tracking: every 200 ms, look up current az from cached schedule,
              drive position setpoint = az.
```

Validated 2026-04-30 with the live SUPERVIEW NEO-2 OBJECT A TLE from
celestrak. Sculpture pointer matched the visualization (`v3-dark.html`)
to within PID-tracking lag (a degree or two during fast az changes,
sub-degree during slow tracking).

---

## Architecture

The webservice owns the math. Each sculpture is a thin client.

```
[ celestrak ]
     ↓ TLE (live)
[ orbital-temple.web.app/api/schedule ]
     ↓ 24 h × 1-min samples (az, el) for any lat/lon
[ Witness sculpture ]            [ Visualization (v3-dark.html) ]
   • caches in NVS                  • parses same TLE in browser
   • interpolates samples              via satellite.js
   • drives the rotor                • drives the on-screen pointer
```

Both clients fetch the same endpoint. Both end up using SGP4 propagation
of the same TLE — sculpture via the server-pre-computed samples,
visualization via in-browser satellite.js. They cannot drift relative
to each other by anything more than the schedule's 1-min interpolation
floor (sub-degree at typical az rates).

---

## Why the schedule API is the right interface

We considered three options:

| Option | Pros | Cons |
|---|---|---|
| Sculpture computes orbit itself (SGP4 in C++) | Standalone | Need TLE updates over Wi-Fi anyway; SGP4 in C is real work |
| Sculpture fetches az each minute | Simple | Network chatter; offline = no motion |
| **Sculpture fetches a 24 h schedule once a day, plays it locally against NTP** | Network only daily; works offline if cached; one source of truth | Slightly more code (NVS BLOB + interpolation) |

We picked the third. The webservice doc-comment had already described
this design before any client code existed:

> "Every Witness sculpture and every companion app fetches this once a
>  day and plays it locally against NTP time. No polling, no network
>  chatter during playback."

Each client trusts the schedule blob and runs autonomously between
fetches. If the next fetch fails, it keeps using the previous one
(per-spec: "If in the next day, they try to get the data and can't,
for any connecting reason, they will just repeat itself with the
current data it has in storage").

---

## Storage layout

`ScheduleStore` persists in ESP32 NVS under namespace `sched`:

```
key  "h"    → Header struct (24 bytes)
              { valid_from_utc, valid_until_utc, fetched_at_utc,
                samples_interval_sec, samples_count }
key  "blob" → samples array, samples_count × {float az, float el}
              (≈ 11.5 KB for a full 1440-sample day)
```

Total ≈ 11.5 KB written once per day. NVS wear is negligible at this rate.

`fetchDue(nowUTC)` returns true when:
- there's no cached data, OR
- `nowUTC >= valid_until_utc` (expired), OR
- `nowUTC - fetched_at_utc >= 24 h` ("first opportunity after 24 h")

The main loop polls `fetchDue` once an hour. The boot loop also calls
it once at startup.

---

## Sculpture ↔ visualization alignment, the "flatten the axis" call

The pointer's rotation is described in two coordinate systems:

- **Compass az**: 0° = North, +east. What the schedule API returns.
- **User-frame deg**: 0° = visual vertical (the wall's pointer-up
  position, as established by the sensorless homing + mass_offset).
  +deg = rotor turns CW (validated 2026-04-30 by `move 30` test).

The visualization uses three.js with `clockGroup.rotation.z = -az`,
which (looking from +Z toward the model) renders as: positive az ⇒ CW
rotation of the pointer from its up direction. The sculpture's mapping
is **`user_target = +az`** so positive az ⇒ CW rotation of the rotor
from its up direction. Both ⇒ visually identical.

This deliberately ignores where actual compass North is on the wall
("Does it matter? Our interface should flatten the axis." — Edson).
The artwork's contract is *the visualization's pointer and the
sculpture's pointer move together*, not *the sculpture points at true
compass North*. If a future deployment needs the latter (e.g., during
an overhead pass), see TODO.md "Wall-orientation offset".

---

## Things we hit and fixed during integration

### Schedule fetch IncompleteInput parse error

ArduinoJson's streaming parser couldn't keep up with the TLS chunked
response on this Wi-Fi — the byte stream stuttered and the parser
returned `IncompleteInput`. Switched to `HTTPClient::getString()`
(buffer the entire body, then parse) and the issue went away. The body
is ~51 KB, well within the ESP32-S3's heap.

### Wi-Fi connect timeout in weak-signal location

The first connect attempt sometimes times out before the AP is found
(scan window is small). Patched `Network::connect()` to:

- Retry up to 4 times, full disconnect+reconnect between attempts.
- Set `WiFi.setTxPower(WIFI_POWER_19_5dBm)` (max for ESP32-S3) and
  `WiFi.setSleep(false)` so the radio is at full strength.
- On total failure, scan visible networks and dump SSID/RSSI/channel
  to the log so we can see what the chip sees.

After this, connect succeeded on the first attempt at the same
location where the original code had been timing out. RSSI of -52 dBm
is comfortable; -65+ would benefit from a closer router.

### Voltage gate hysteresis

Boot ritual originally tripped the voltage gate during the spin's
brake-current bump (Vin sagged 15.03 → 14.93 V for ~200 ms — just below
the 15 V threshold), causing a full re-home. Fixed by introducing
asymmetric thresholds + debounce:

- **Engage** at Vin ≥ 15.0 V (only allow ritual to start with full supply).
- **Drop** at Vin < 13.5 V *for 3 consecutive 500 ms checks* (~1.5 s of
  real low voltage). Transient sags during current spikes are absorbed.

Validated post-fix: full ritual completed without a spurious voltage
trip. End error 0.14° from visual zero.

### Sign of az → user-frame mapping

Initial implementation used `target = -az` (mirrored the visualization's
`rotation.z = -az`). But the sculpture's encoder direction is opposite
to three.js's positive-rotation convention. After the `move 30` test
confirmed the rotor turns CW for positive user-frame deg, flipped to
**`target = +az`**. After that the sculpture's pointer agreed with the
visualization's pointer to within tracking lag.

---

## Files added in Stage 1

```
src/
├── Network.{h,cpp}        Wi-Fi connect + NTP sync (boot + retry)
├── ScheduleStore.{h,cpp}  NVS persistence + interpolated lookup
├── ScheduleClient.{h,cpp} HTTP GET + ArduinoJson parse
├── Tracker.{h,cpp}        Periodic az → user-frame setpoint
├── PowerGate.{h,cpp}      Refactored from main.cpp (was inline in 10)
├── secrets.h.example      Template (committed)
└── secrets.h              Wi-Fi creds + observer lat/lon (gitignored)
```

`MotorIO`, `MotorState`, `Recipes`, `SinusoidFit`, `Calibration`,
`Calibrator`, `MoveOperator`, `SpinOperator`, `Logger` are inherited
unchanged from `10_witness_modular`. Stage 1 didn't touch any of the
validated motion code.

`MotorState::ST_TRACKING` was added (one new value in the State enum).
`Logger`'s `phaseCode()` got the corresponding `'T'` letter.

---

## Constants worth knowing

```
SCHEDULE_URL_BASE    https://orbital-temple.web.app/api/schedule
OBSERVER_LAT/LON     40.65 / -73.95   (Brooklyn — TODO: auto-detect via IP)

ENGAGE_VIN_V         15.0   (boot ritual will not start below this)
DROP_VIN_V           13.5   (mid-cycle release threshold)
DROP_DEBOUNCE_N         3   (× 500 ms ≈ 1.5 s of low-V before tripping)

REFETCH_AGE_SEC      24 × 3600   (one day)
TRACKER UPDATE_PERIOD_MS    200     (≈ 5 Hz setpoint advance)
TRACKER STILL_THRESH_DEG    0.4
TRACKER negate              false   (target = +az, validated 2026-04-30)
```

---

## What's deferred — see TODO.md

- Calibration only on real power cycle (skip the 110 s ritual when the
  reset is software-only, e.g., a reflash).
- Auto-detect observer location via IP geolocation.
- Wall-orientation offset (parked — flatten-the-axis call).
- Sign-flip exposed via serial command + NVS.
- Schedule-fetch resilience under heap pressure (LittleFS streaming).

---

## Status

Stage 1 complete and validated. Sculpture and visualization track in
lockstep against the live TLE. Next firmware (`12_witness_polish`)
clones from this one and works the TODO list.
