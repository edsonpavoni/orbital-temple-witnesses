# First Witness — Stage 4 Knowledge (captive-portal onboarding + tracker polish)

What was learned making the sculpture plug-and-play for an end user — no
re-flash needed to hand over their Wi-Fi credentials — and tightening the
tracker so the rotor lands cleanly on the satellite without overshoot or
audible whip.

Stages 1–3 (Wi-Fi tracking, offline replay, IP geolocation) are
documented in `STAGE1_KNOWLEDGE.md`, `STAGE2_KNOWLEDGE.md`,
`STAGE3_KNOWLEDGE.md`. Calibration / spin fundamentals live in
`09_witness/KNOWLEDGE.md`.

---

## What this stage adds

1. **Captive-portal Wi-Fi onboarding.** Sculpture boots, raises a
   SoftAP named after itself (`Orbital Witness 1/12`), serves a setup
   page on `192.168.4.1` that auto-pops via OS captive-portal probes,
   user picks their network + types the password, sculpture stores in
   NVS and reboots into station mode. No re-flash, no companion app.
2. **NVS schedule format compaction.** Sample is now `int16` deci-deg
   (4 B/sample) instead of `float` (8 B). All 1440 1-min samples fit
   in 5760 B, leaving room for Wi-Fi creds + geo + calib in the
   default 20 KB NVS partition.
3. **Tracker rate-limit + eased entry.** Tracker no longer slams a
   100°-step setpoint into the position PID at the spin → tracking
   handoff. An 8-second cosine ease-in carries the rotor from the
   `+1080°` spin endpoint to the live satellite az; per-tick
   `TRACK_MAX_RPM` cap protects against any future step-inputs.
4. **Per-sculpture identity.** `SCULPTURE_NAME` constant in
   `secrets.h` propagates to SoftAP SSID, captive-portal page, and
   boot banner. Each unit's firmware build carries its own name.

---

## Captive-portal architecture

`Provisioning::runPortal()` is a `[[noreturn]]` blocking call invoked
from `setup()` when both NVS and the `secrets.h` fallback fail to
yield Wi-Fi credentials. Flow:

1. `WiFi.persistent(false)` — never let the IDF stash creds in its own
   NVS namespace; we own persistence via `WifiCreds`.
2. Bring up SoftAP at `192.168.4.1` on channel 1, max TX power
   (19.5 dBm, re-asserted after `softAP()` since some boards reset
   the value internally).
3. `dnsServer.start(53, "*", AP_IP)` — wildcard DNS hijack: every
   hostname resolves to us. Client OS sees this as a captive portal.
4. Pre-scan the visible Wi-Fi networks once before serving any HTTP,
   sorted by RSSI, capped at 32. Subsequent page-loads use the cache;
   a `↻ Re-scan` link hits `/rescan` to refresh.
5. HTTP server on port 80 serves form HTML directly on every captive
   probe URL (Apple `/hotspot-detect.html`, Android `/generate_204`,
   Windows `/ncsi.txt`, plus a wildcard `onNotFound`). **No 302
   redirect** — returning the form as 200 makes the OS popup show
   content immediately instead of chaining a redirect.

### The deferred-save pattern

The HTTP request handler **cannot tear down its own server**. If
`handleSave()` calls `httpServer.stop()` mid-flight, the chip crashes
inside the WebServer's own callback dispatch. Same for `WiFi.softAPdisconnect()`
called inside the handler.

Solution: handler stashes the creds + sets a flag, returns the
"Saved" page, then the main `for(;;)` loop notices the flag, gives
the response a beat to finish leaving the wire (~800 ms), and then
performs the NVS write + `ESP.restart()`.

```cpp
// Inside handleSave():
httpServer.send(200, "text/html", DONE_PAGE);
pendingSSID = ssid;
pendingPW   = pw;
saveAtMS    = millis();
savePending = true;
// returns; handler unwinds cleanly.

// In runPortal()'s main loop:
if (savePending && (millis() - saveAtMS > 800)) {
  savePending = false;
  WifiCreds::save(pendingSSID, pendingPW);
  ESP.restart();
}
```

### Why the raw nvs_* API instead of Preferences

`Preferences::putString()` silently returned 0 (zero bytes written)
in this stage's namespace until I switched to the raw `nvs_*` API —
which immediately reported the actual error (`ESP_ERR_NVS_NOT_ENOUGH_SPACE`).
`Preferences` swallows the underlying `esp_err_t`, returning size_t
zero with no diagnostic. **For diagnosing NVS save failures, always
use `nvs_open` / `nvs_set_str` / `nvs_commit` directly.** The raw API
returns a typed error code and `esp_err_to_name()` decodes it.

The "fwcreds" namespace name was chosen specifically to avoid
collision — `wifi` and `nvs.net80211` are used by ESP-IDF's own Wi-Fi
stack and writes there can be silently overwritten by the radio's
internal state save.

### Captive-portal probe URLs

| OS | Probe | Expected response (no portal) | What we serve |
|----|-------|-------------------------------|---------------|
| Apple | `/hotspot-detect.html` | HTML containing "Success" | Setup form |
| Android | `/generate_204`, `/gen_204` | HTTP 204 No Content | Setup form |
| Windows | `/ncsi.txt`, `/connecttest.txt` | "Microsoft NCSI" / 200 | Setup form |
| (any) | wildcard via `onNotFound` | (DNS hijack catches everything else) | Setup form |

Returning **anything other than the expected response** triggers the
OS to show its captive-portal UI. We return our setup HTML directly,
so the popup contains the form immediately.

### iOS Chrome caveat

Apple still requires WebKit for any iOS browser engine, so Web
Bluetooth (and therefore the Improv-WiFi protocol) cannot work on iOS
— even in Chrome. That's why captive-portal is the only universal
onboarding method; if Improv is ever added it must be one of several
options, not the only one.

---

## NVS layout

The default Arduino-ESP32 NVS partition is **20 KB**. Initial attempts
at storing the 1440-sample schedule blob (~11.5 KB at 8 B/sample) on
top of Wi-Fi creds + geo + calibration produced
`ESP_ERR_NVS_NOT_ENOUGH_SPACE` once anything else needed to grow.
NVS chunks 12 KB blobs across multiple 4 KB pages, and any other
namespace's pages count against the global free pool.

I tried a custom partition table (`partitions_witness.csv`) that
expanded NVS to 64 KB. **It boot-looped twice**: esptool.py writes
firmware at the default offset `0x10000` even when the new partition
table puts `app0` at `0x20000`. The bootloader reads the new table,
can't find an app, halts. Even after `pio run -t erase`, the upload
still landed at the default offset. Reverted.

The fix that stuck: **shrink each sample to `int16` deci-degrees**.

```cpp
struct Sample {
  int16_t az_deci;   // user az × 10, range [-1800, 1800]
  int16_t el_deci;   // elevation × 10, range [-900, 900]
  inline float az() const { return az_deci * 0.1f; }
  inline float el() const { return el_deci * 0.1f; }
};
```

| Sample storage | Bytes/sample | Full 1440-sample blob | Resolution |
|----------------|--------------|------------------------|------------|
| `{float, float}` (original) | 8 | 11 520 | full float |
| `{float, float}` decimated 5× (rejected) | 8 | 2304 (288 samples) | ±5° error during overhead passes |
| `{int16, int16}` (current) | 4 | 5760 | 0.1° |

Final NVS map (all small enough to leave plenty of headroom):

| Namespace | Purpose | Size |
|-----------|---------|------|
| `fwcreds` | Wi-Fi SSID + password | ~80 B |
| `geo` | Observer lat/lon | ~32 B |
| `calib` | Mass offset deg + zero offset counts | ~16 B |
| `sched` | Header + 1440 × 4 B samples | ~5.8 KB |
| **Total** | | **~6 KB / 20 KB** |

### Format-change forward compatibility

`ScheduleStore::loadFromNVS()` validates blob size against
`sizeof(Sample) * samples_count`. When the firmware was upgraded from
float-decimated to int16-full, the existing 2304-byte blob mismatched
the expected 5760 bytes for 1440 samples → load failed → `fetchDue()`
returned true → fresh fetch repopulated NVS in the new format. **No
manual NVS erase needed for sample-format changes** — the size check
auto-invalidates stale blobs.

(Header struct changes still require a version field or manual erase
to handle gracefully; `_h.samples_count == 0 || > MAX_SAMPLES` is the
existing sanity gate.)

---

## Tracker rate-limit + eased entry

Two things go wrong if you write the satellite az directly to the
motor's position-mode register:

1. **At handoff.** The 1080° spin ends with the rotor at encoder
   `+1080°`. The first satellite sample (compass az ≈ 200°) unwraps
   to a target ~880° away. `nearestUnwrap` brings that within ±180°
   of `currentTargetDeg`, but the result is still 100°+ from the
   spin endpoint. The motor's production PID (Kp=30 M, Kd=40 M,
   1000 mA cap) treats this as a step input: peaks at **49 RPM**,
   overshoots by 60°, oscillates ±60° about target for several seconds.
2. **Mid-track wraps.** If the satellite az crosses 0°/360° boundary
   in a way `nearestUnwrap` flips the unwrap branch, the same step
   problem appears mid-flight.

### The two-mechanism fix

```cpp
// Recipes.h
constexpr float TRACK_MAX_RPM = 6.0f;  // rotor never exceeds this in tracking

// Tracker.cpp — tick() body
if (_entering) {
  // 8-second cosine ease-in from the entry start (typically the
  // +1080° spin endpoint) toward the *live* satellite az. The end
  // is itself a moving target, so we re-evaluate it each tick.
  float prog  = elapsed / 8000.0f;
  float eased = 0.5f * (1.0f - cosf(prog * PI));
  float wantedNearStart = nearestUnwrap(_enterStartDeg, candidate);
  target = _enterStartDeg + (wantedNearStart - _enterStartDeg) * eased;
  return;
}
// Steady-state path: cap the per-tick advance at TRACK_MAX_RPM.
const float maxStepDeg = Recipes::TRACK_MAX_RPM * 6.0f *
                         (UPDATE_PERIOD_MS / 1000.0f);  // = 7.2°/tick
float delta = wanted - _m.currentTargetDeg;
delta = constrain(delta, -maxStepDeg, +maxStepDeg);
target = _m.currentTargetDeg + delta;
```

### Behaviour observed end-to-end

Spin endpoint: `final = 1080.11°`. First satellite target unwraps to
`~1208°` (128° user-frame).

| Phase | Time after Tracker enabled | Tg | RPM observed | Notes |
|-------|----------------------------|-----|--------------|-------|
| Entry start | 0 s | 1080.23° | 0.3 | 0.12° step (cosine starting flat) |
| Mid-ramp | 4 s | ~1140° | 6–8 | Peak velocity, ±2° PID ringing |
| Entry end | 8 s | 1207.98° | 4 | "eased entry complete" log |
| Steady state | +5 s | 1209.94° | <0.5 | er = 0.32°, locked |

Compare to the **broken** behaviour (same handoff, no eased entry, no
rate limit):

| Time | p | Tg | er | RPM |
|------|---|-----|----|-----|
| 0 s   | 1079.35 | 977.20 | -102.15 | -1.5 |
| 0.4 s | 958.75  | 977.20 | +18.52  | **-49.4** ← peak |
| 0.6 s | 916.35  | 977.20 | +60.92  | -31.6 (overshot 60° below) |
| 1.5 s | 1052.25 | 977.40 | -74.92  | +47.5 (overshoot above) |
| 3 s   | (still oscillating ±60° about target) | | | |

### Why the motor oscillates on a step input

The Roller485's position mode is a **closed-loop position PID with
no internal trajectory generator**. There's no `max_velocity` or
`trajectory_time` register. Kp = 30 M is tuned so that a 0.5° error
produces a setpoint deviation that drives full current authority, so
the rotor tracks tightly during smooth motion. But the same gain
applied to a 100° error produces an instantaneous current saturation
at the 1000 mA cap, which accelerates the rotor to ~50 RPM —
maintained until kinetic energy + Kd contribution overcome it,
producing massive overshoot.

The firmware-side rate limit + eased entry is the only path; there's
no register to set on the motor.

### Why TRACK_MAX_RPM = 6 specifically

- `SPIN_CRUISE_RPM = 8` is the loudest velocity the sculpture ever
  produces in normal operation. Tracking should be **slower** than
  spin so the boot-spin is unambiguously the loudest sound.
- 6 RPM = 36°/sec is plenty fast to track any plausible satellite
  motion: even an overhead-pass zenith flip is ~3°/sec peak. The
  rate limit is dormant during normal operation; it's a safety net
  for steps.
- 6 RPM × 6°/sec/RPM × 0.2 s tick = 7.2° max per-tick advance.
  Recovery from a 7.2° gap takes one tick; from a 100° gap, ~14
  ticks (~3 sec) — fast enough to feel responsive, slow enough not
  to whipsaw.

---

## Per-sculpture identity

`#define SCULPTURE_NAME "Orbital Witness 1/12"` in `secrets.h` (per
unit, gitignored). Used in three places:

1. **SoftAP SSID** during provisioning — `WiFi.softAP(SCULPTURE_NAME, ...)`
   so the user sees `Orbital Witness 1/12` in their phone's Wi-Fi
   list, not a generic `FirstWitness-XXXX`. The MAC-tail suffix used
   previously is gone — the per-unit name is already unique.
2. **Captive-portal page** `<title>` and `<h1>`.
3. **Boot banner** on serial.

SSID supports up to 32 bytes UTF-8; "Orbital Witness 1/12" (20 chars)
is comfortably within. The forward slash `/` is legal SSID character
on the AP side; some clients display it strangely but iOS / Android /
macOS / Windows all render it correctly.

For a future iteration, the same constant could carry into
schedule-fetch headers / telemetry to address each unit individually
on the webservice.

---

## Pitfalls + workarounds

### XIAO ESP32-S3 USB-CDC reset on serial open

Native USB-CDC doesn't follow classic USB-UART DTR/RTS conventions.
Opening the serial port from pyserial reliably resets the chip even
with `dtr=False, rts=False, dsrdtr=False, rtscts=False`. Standard
ESP32 dev boards use the FT232/CP2102's RTS/DTR transistors to drive
EN/IO0; the XIAO's native USB has different routing.

Practical implications:
- Every `pio device monitor` attach reboots the chip.
- Cannot non-disruptively observe a running sculpture from a fresh
  serial monitor; have to wait through the boot ritual each time.
- esptool's `--after no_reset` works for upload but not for monitor
  attach.

For interactive testing, accept the reboot. For CI / scripted
behavioural tests, drive the chip via a permanently-attached
programmer / OTA test fixture.

### Custom partition table boot loops

Twice in this stage I tried a custom partition table with expanded
NVS. Both times esptool wrote firmware at the default `0x10000`
offset even though the new table specified `0x20000`. Bootloader
read the new table → no app at expected offset → boot loop. `pio run
-t erase` did not fix it.

Hypothesis: `platformio.ini`'s board build-flags or the framework's
linker script has the firmware load offset baked in, independent of
the partition CSV. Working around this would mean modifying the
linker script or board JSON, both of which are higher-blast-radius
than just shrinking the data.

The int16 sample format gave us a 2× reduction with no observable
loss — that path is much safer than fighting the partition system.

### Captive portal pre-scan

First versions did the Wi-Fi scan synchronously inside the form's
GET handler. The first page load took 3–5 seconds (the OS captive
popup hung waiting for HTTP response) and felt broken. Pre-scan once
before `httpServer.begin()`, cache up to 32 networks sorted by RSSI,
serve from cache. The `↻ Re-scan` link hits `/rescan` for explicit
refresh.

Combined with serving form HTML directly on captive probe URLs (no
302 redirect chain), the user-perceived time from "join SoftAP" to
"see setup form" is ~1 second.

---

## What this stage didn't solve (deferred to next)

- **Acoustics.** Steady-state tracking is silent (rotor near 0 RPM),
  but the 1080° spin and the eased entry produce audible BLDC whine
  + minor PID-ringing tick noise at the start of motion. Goal:
  gallery-quiet at conversational distance.
- **Motion ritual.** The boot 1080° spin is functional (proves the
  rotor is alive, settles gravity homing). A *deliberate gestural*
  ritual on first-tracker-lock or some periodic cadence would read
  as expressive instead of utilitarian.
- **Skip calibration on software reset.** Cold-power-cycle
  calibration is needed; reboots after firmware reflash redundantly
  re-run the 110-second ritual. Tracked in `TODO.md`.
- **Improv-WiFi / DPP** as alternative onboarding methods. Captive
  portal works universally; these would be polish for users who
  prefer a flow that doesn't require Wi-Fi-network swapping.
- **Sculpture-name as schedule-fetch identifier.** Currently the
  webservice has no per-unit identity. When telemetry arrives, the
  `SCULPTURE_NAME` constant is the natural key.

---

## Files touched in this stage

```
src/
  Provisioning.h           NEW   captive-portal interface
  Provisioning.cpp         NEW   SoftAP + DNS + HTTP form, deferred-save
  WifiCreds.h              NEW   raw nvs_* wrapper for fwcreds namespace
  WifiCreds.cpp            NEW   load/save/forget with read-back verify
  ScheduleStore.h          MOD   Sample → int16 deci-deg, az()/el() accessors
  ScheduleStore.cpp        MOD   accessors threaded through interp
  ScheduleClient.cpp       MOD   pack int16 on fetch, decimation removed
  Tracker.h                MOD   _entering / _enterStartMS / _enterStartDeg
  Tracker.cpp              MOD   8-s cosine ease + per-tick rate cap
  Recipes.h                MOD   TRACK_MAX_RPM = 6.0
  secrets.h                MOD   SCULPTURE_NAME = "Orbital Witness 1/12"
  secrets.h.example        MOD   document SCULPTURE_NAME placeholder
  main.cpp                 MOD   boot sequence: NVS-creds | secrets | portal
                                 + boot banner uses SCULPTURE_NAME
TODO.md                    MOD   next-iteration items
STAGE4_KNOWLEDGE.md        NEW   this file
```

End of Stage 4.
