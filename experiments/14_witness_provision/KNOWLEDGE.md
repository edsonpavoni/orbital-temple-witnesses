# First Witness — Knowledge

Consolidated reference for the firmware. Everything load-bearing for
day-to-day work, debugging, and commissioning sculptures 2/12 → 12/12.
Operational commands live in `README.md`; this file is the *why*.

---

## 1. What the sculpture does

Wall-mounted unbalanced pointer driven by an M5Stack Unit-Roller485 Lite
(BLDC + integrated FOC + magnetic encoder + on-chip position PID),
controlled over I²C by a Seeed XIAO ESP32-S3.

Boot sequence on every power-up:

```
power-gate (Vin >= 14 V)
  → Wi-Fi connect (4 attempts, max TX power, RSSI logged)
  → NTP sync                                 │ if either fails,
  → schedule fetch from /api/schedule        │ fall through
  → 3 s pause
  → sensorless gravity homing  (re-anchor zero = visual-up)
  → 1080° spin   (3 revolutions, brake at start angle)
  → tracking      (pos setpoint = current satellite az, every 200 ms)
```

Total ritual ~117 s. Contract: **after homing, the pointer always
returns to visual zero**. Replay-mode contract: **once it has seen one
schedule, it keeps moving forever, even if Wi-Fi never returns.**

Hardware: XIAO ESP32-S3 (USB-C). Motor at I²C `0x64`,
`SDA=GPIO8 / SCL=GPIO9 @ 100 kHz`. Motor PWR-485 fed 15 V via USB-C
PD trigger. Encoder: 36 000 counts = 360°.

---

## 2. Calibration — physics + algorithm

### Physics

The unbalanced pointer's mass center is offset from the rotation axis,
so gravity exerts position-dependent torque
`τ_gravity(θ) = −m·g·r·sin(θ_mass)`. Under steady rotation, the motor
current draw traces this sinusoid; the phase pinpoints the *unstable*
equilibrium ("gravity up").

`mass_offset_deg` is the **fixed** angular distance between gravity-up
and the artist-defined visual-up. Mechanical property of the pointer
geometry; calibrate once after every (re)mount, persist in NVS.

`visual_up = gravity_up − mass_offset_deg`

### Two-direction sweep cancels friction

```
I_CW(θ)  + I_CCW(θ)  = 2 · gravity_signal(θ)    // friction is anti-symmetric
I_CW(θ)  − I_CCW(θ)  = 2 · friction_signal(θ)   // gravity is symmetric
```

Sweep one rev CW, pause 1.5 s, one rev CCW, accumulate `(encoderDeg,
currentMA)` samples into a 3-parameter least-squares fit
`cur(θ) = a·sin(θ) + b·cos(θ) + dc`. Phase `atan2(b, a)` → gravity-up.

Skip the first half of each sweep (`DIAG_FIT_SKIP_FRACTION = 0.5`) —
those samples include the static-friction breakthrough and bias the fit.

### Calibration recipe (in serial)

```
release                            # motor off, hand-position pointer
  ... position pointer at TRUE visual-up (engraved mark) ...
hold                               # latch this as user 0
diag                               # ~80 s sweep + fit, prints:
   >> fit: gravity-up at user-frame X.XX deg ; mass_offset_deg ... ; fit_N N
   >> fit quality: amplitude=A.A mA  dc=D.D mA  (a=... b=...)
                                   # X.XX is your new mass_offset_deg
diag                               # repeat 2–3× — values within ±0.5° = solid
setcal X.XX                        # set in RAM
savecal                            # persist to NVS
home                               # verify ritual lands at visual zero
```

**Fit-quality canary: amplitude (mA).** Higher = stronger gravity
signal. >30 mA = solid; <5 mA = noise (something mechanical is wrong).
A rigid wall mount typically yields 70–80 mA; a wobbly mount produces
biased fits at much lower amplitude (this is *why* OW1/12 swung
~37° from `+21.74°` to `-15.96°` between loose-mount and rigid-mount
calibrations).

**Use the dedicated `15_calibration/` sketch for re-calibration after
mounting.** It boots IDLE (no auto-home, no auto-spin, no Wi-Fi/NTP),
so the production firmware never runs on the stale value mid-procedure.
Same NVS namespace — `savecal` here carries to production. Adds the
amplitude / DC / raw a/b output above.

---

## 3. Two control regimes — when to use which

| Regime | Use for | PID | Quality |
|---|---|---|---|
| Position, motor's eased trajectory | Move-to-angle (post-homing) | Production (kp=30 M) | ±0.5° accurate |
| Position, software rate-integrated | 1080° spin cruise | Production | Deterministic angular travel |
| Position, snap-and-brake | Spin stop | Production, 1500 mA cap | ≤ 1° overshoot |
| Position, looser-PID slow trajectory | Calibration sweep | Diagnostic (kp=1.5 M) | Gravity legible in current |
| Speed-mode, loose PID | (rejected — `03_motion_test`) | Loose flash PID | "Alive" but unpredictable end angle |

The artwork uses the first three. The fourth is the cal sweep. The
fifth is reserved for any future artwork that wants the swing
aesthetic without precision constraint.

**Why position-mode rate-integration kills the gravity swing:** in
loose speed mode the rotor accelerates downhill (peaks ~19 RPM during
8 RPM cruise) and stalls climbing. In position-mode rate-integration,
the *setpoint* moves at exactly 8 RPM, and the stiff PID drags the
rotor along — staying within 6.8–7.9 RPM at brake-fire, gravity
ripple bounded to a degree of tracking lag. Trade-off chosen
deliberately for deterministic end angle.

---

## 4. Validated recipes (constants)

```
PRODUCTION POSITION PID  (moves + spin cruise + brake)
  kp = 30 000 000   ki = 1 000   kd = 40 000 000
  mc = 1 000 mA cruise / 1 500 mA brake   easing = cosine S-curve
  mt = 33.33 ms / deg

DIAGNOSTIC POSITION PID  (calibration sweep only)
  kp =  1 500 000   ki =    30   kd = 40 000 000
  mc = 1 000 mA   easing = cosine S-curve   mt = 50 ms / deg  (~5 RPM)

SPIN
  total_deg          = 1080.0   (3 revs)
  cruise_rpm         =    8.0
  ramp_ms            = 2 000    (cosine 0 → 8 RPM)
  brake_grace_ms     =   300
  settle_ms          = 2 000
  brake_max_ma       = 1 500    (bumped from 1 000 for brake authority)

CALIBRATION
  diag_revs/dir      = 2.0     diag_pause_ms = 1 500
  fit_skip_fraction  = 0.5
  mass_offset_deg    = -15.96  (OW1/12 current; in NVS)

TRACKING
  TRACK_MAX_RPM      = 6.0     UPDATE_PERIOD_MS = 200
  ENTRY_MS           = 8 000   (cosine ease from spin-end into live az)

POWER GATE
  ENGAGE_VIN_V    = 14.0   (lowered from 15.0 on 2026-05-01: PD-15V
                            profile measures 14.91 V steady, well within
                            USB-PD ±5 % spec range 14.25–15.75 V)
  DROP_VIN_V      = 13.5  (asymmetric, debounced 3 × 500 ms = 1.5 s)
```

**Validated 2026-04-30:** end-to-end ritual lands within 0.3° of
visual zero across 6 back-to-back runs. Motor temp stable 46–47 °C.

---

## 5. Network / tracking architecture

The webservice owns the math. Each sculpture is a thin client.

```
[ celestrak ]   →   [ orbital-temple.web.app/api/schedule ]
                          ↓ TLE + 24 h × 1-min (az, el) samples
       ┌──────────────────┴──────────────────┐
[ Witness sculpture ]              [ v3-dark.html visualization ]
  • caches in NVS                    • parses same TLE in browser
  • interpolates samples                via satellite.js
  • drives the rotor                 • drives on-screen pointer
```

Both clients fetch the same endpoint. Both end up using SGP4 against
the same TLE — sculpture via server-pre-computed samples, viz via
in-browser `satellite.js`. **Design contract: they cannot drift more
than the schedule's 1-min interpolation floor (sub-degree at typical
az rates).**

Per Edson: "Does it matter? Our interface should flatten the axis."
The firmware sets `user_target = +az` directly — the artwork's
contract is *visualization-pointer and sculpture-pointer move
together*, NOT *sculpture points at true compass North*. Visual-up = az
0° in software, regardless of which compass direction the wall faces.

Tracker handoff: a step input from spin-end (~+1080°) to live az would
peak the rotor at ~50 RPM with 60° overshoot. Two-mechanism fix:

- **8 s cosine ease-in** from `_enterStartDeg` toward live az (the end
  is itself a moving target; re-evaluated each tick).
- **Per-tick rate cap** at `TRACK_MAX_RPM × 6°/RPM/s × 0.2 s = 7.2°`
  per tick. Dormant during normal motion; safety net for steps.

`TRACK_MAX_RPM = 6` — slower than the 8 RPM spin so the boot spin is
unambiguously the loudest sound; fast enough for any plausible
satellite az rate (zenith flips ~3°/s peak).

### Time regimes (degrades gracefully)

| NTP synced | Schedule cached | Behaviour |
|---|---|---|
| yes | yes | Real-time live tracking |
| yes (later) | yes | Real UTC + cached play; SNTP overrides virtual silently when it returns |
| no, virtual anchor | yes | Replay-loop the cache modulo `[valid_from, valid_until)` |
| no | no | Park at visual zero |

`Network::nowUTC()` picks: real `time(nullptr)` if sane (>2023) →
virtual anchor → 0. Virtual anchor is set once at boot if NTP timed
out AND a cached schedule exists; points at `valid_from` so replay
starts at "the moment the cached day began."

### Schedule fetch policy

`fetchDue(nowUTC)` returns true when:
- no cached data, OR
- `nowUTC >= valid_until_utc` (expired), OR
- `nowUTC − fetched_at_utc >= 24 h`.

Polled hourly from main loop + once at boot. **Caveat:** at any
random moment, the cache can be up to 23 h old. For LEO satellites,
even a few hours of TLE staleness can produce tens of degrees of
sculpture-vs-viz divergence. **`forcefetch` serial command bypasses
the policy** and pulls a fresh schedule immediately — use when sculpture
and visualization disagree. Production policy worth tightening to
~6 h for sculptures 2/12+.

---

## 6. NVS layout (default 20 KB partition)

| Namespace | Purpose | Size |
|---|---|---|
| `fwcreds` | Wi-Fi SSID + password | ~80 B |
| `geo` | Observer lat/lon (auto-detected via ipapi.co on first online boot) | ~32 B |
| `calib` | `mass_offset_deg` + `zeroOffsetCounts` | ~16 B |
| `sched` | Header + 1440 × 4 B `int16` deci-deg samples | ~5.8 KB |
| **Total** | | **~6 KB / 20 KB** |

### Gotchas, learned the hard way

- **`WiFi.persistent(false)` once at startup**, before `WiFi.mode()`.
  Default is `true`, which writes creds to NVS every connect. With
  `WiFi.disconnect(true, true)` between retry attempts, the namespace
  fragments fast and a 12 KB blob hits `NOT_ENOUGH_SPACE`.
- **`Preferences::clear()` at the start of every blob save.** Repeated
  schedule fetches fragment the `sched` namespace; clearing the keys
  before writing gives the new blob a clean run of pages.
- **Custom partition tables boot-loop on PlatformIO.** esptool writes
  firmware at the default `0x10000` offset even when the new table
  puts `app0` at `0x20000`; bootloader can't find the app, halts.
  Even `pio run -t erase` doesn't fix it. **Don't fight the partition
  system — shrink what we store.** That's why samples are `int16`
  deci-deg (2 B each, 0.1° resolution) instead of `float`.
- **Sample-format forward compatibility:** `loadFromNVS()` validates
  blob size against `sizeof(Sample) * samples_count`. A format change
  invalidates the existing blob, `fetchDue()` returns true, fresh
  fetch repopulates in the new format. No manual erase needed.
- **RTC retention across soft reset:** ESP32-S3's RTC clock domain
  survives DTR/RTS soft resets (`pio run -t upload`). `time(nullptr)`
  returns the previously-synced time, not zero. To genuinely test the
  virtual-clock path you must physically power-cycle.
- **`mass_offset_deg` default in `main.cpp` is 21.74°.** NVS takes
  precedence; the default only kicks in if NVS is wiped.
- **Stall protection is disabled at boot** (`writeReg8(REG_STALL_PROT,
  0)`) so the slow diag sweep doesn't trip a false jam alarm. Don't
  re-enable without re-validating.

---

## 7. Per-sculpture identity (provisioning)

`#define SCULPTURE_NAME "Orbital Witness 1/12"` in `secrets.h` (per
unit, gitignored). Surfaces in:

1. SoftAP SSID during provisioning — user sees the sculpture's own
   name in the Wi-Fi list, not a generic ID.
2. Captive-portal page title and heading.
3. Boot banner on serial.

Captive portal is a `[[noreturn]]` blocker: when both NVS and the
`secrets.h` fallback fail to yield Wi-Fi creds, sculpture raises a
SoftAP at `192.168.4.1` (channel 1, max TX power, open / no password,
max one client). Wildcard DNS hijack (`*` → AP IP). HTTP server on
port 80 serves the form HTML directly on every captive-probe URL
(`/hotspot-detect.html`, `/generate_204`, `/ncsi.txt`, etc.) — no 302
redirect, instant popup with content. Pre-scans visible Wi-Fi networks
once before the loop starts (cached, sorted by RSSI, 32 max) so first
page-load isn't a 3–5 s hang.

**Deferred-save pattern is required.** Handler cannot tear down its own
HTTP server / SoftAP — calling `httpServer.stop()` inside a callback
crashes the chip. Instead: handler stashes creds + sets a flag, returns
the "Saved" page; main `for(;;)` loop notices the flag, waits ~800 ms
for the response to leave the wire, writes NVS, `ESP.restart()`.

iOS Lockdown Mode disables the auto-popup; the user must browse to
`http://192.168.4.1` manually. Workaround documented in user-facing
provisioning notes.

---

## 8. Field commissioning playbook (the five things that broke)

Distilled from OW1/12 commissioning, 2026-05-03 night → 2026-05-04
drop-off. Each was a real failure with real diagnostic and fix.

### 8.1 SCL stuck low after first mount

**Symptom:** `ERROR: Motor not found at 0x64. Halting.` immediately
after first wall mount; bench had been clean.

**Diagnostic:** `scanner_i2c/` sketch reads idle line states with
`INPUT_PULLUP` *before* configuring `Wire`. Output `SDA=HIGH SCL=LOW`
= bus stuck (SCL pinched against chassis at the cable entry). Software
cannot fix a shorted wire.

**Fix:** disassemble, reroute SCL with chassis clearance, reassemble.
For 2/12+: pre-flight `scanner_i2c` on every freshly-assembled unit
*before* closing the lid.

### 8.2 PD brick latched its protection circuit

**Symptom:** Vin degraded `15.15 V → 11.04 V → 4.98 V → 0 V (brick
wouldn't even charge a phone)`. Boot ritual stuck in `waitForPower()`.

**Recovery:** unplug brick from wall, leave 10 min, replug. Bulk caps
discharge, internal protection re-arms.

**Takeaway:** spec **65 W+ USB-C PD charger** for sculptures 2/12+
(2× thermal headroom over the ~30 W load) with **15 V** explicitly
listed in the PDO table. Avoid PPS-only / "fast charge"-only bricks
that may skip the 15 V fixed PDO. **Treat anything < 14.5 V on the
SYSTEM STATUS Vin as a brick replacement signal**, not a transient —
the 11 V reading was the canary an hour before complete failure.

### 8.3 I²C boot-time race (FIXED in firmware)

**Symptom:** intermittent `Motor not found at 0x64` halts even though
the motor LED was on and `scanner_i2c` found it cleanly seconds later.

**Cause:** the M5 motor takes >1 s to bring up its I²C interface
after its 15 V rail rises. `MotorIO::begin()` had only `delay(100)` —
fine when motor was already powered before XIAO booted, race
condition when both came up at the same instant.

**Fix:** `MotorIO.h:160` — `delay(100)` → `delay(1500)`. Validated
across five subsequent cold boots.

### 8.4 Schedule cache vs live TLE divergence

**Symptom:** sculpture parked at user-frame ~183°, visualization
showed az ~253° — 70° apart. Design contract says ≤ 1°.

**Cause:** firmware's schedule was 2.5 h old; visualization fetches
live each page-load. Server may have refreshed TLE; LEO satellite
position drifts tens of degrees over hours of TLE age.

**Fix part 1:** `forcefetch` serial command bypasses the 24 h
`fetchDue` policy.

**Fix part 2 — UI:** `v3-dark.html` was displaying az via
`(r * 180 / Math.PI).toFixed(1)` which yields `[-180, +180]`. Added
separate `fmtAz` formatter that wraps to `[0, 360)` to match
sculpture's user-frame convention. Non-az fields (lat/lon/el) keep
the original signed `fmt`.

### 8.5 Calibration shifted ~37° between loose and rigid mount

**Symptom:** `mass_offset_deg = 21.74°` saved with a wobbly mount
produced visibly off-vertical landings after rigid mount.

**Cause:** wall flex during the previous diag sweep added position-
dependent phase to the gravity-current sinusoid, biasing the fit.

**Quantitative confirmation:** sinusoid fit amplitude jumped from
~30 mA (noisy, loose mount) to **78.0 mA** (clean, rigid mount).

**Takeaway:** **always recalibrate after final rigid mount.** Use
`15_calibration/` for safety. Amplitude is the new fit-quality canary.

---

## 9. Pre-flight checklist for sculptures 2/12 → 12/12

If commissioning takes more than 2 hours, something has gone wrong;
all 12 boxes here are bullets to dodge.

**Before mounting:**
1. Flash `scanner_i2c/`. Confirm clean ACK at `0x64` with `SDA=HIGH,
   SCL=HIGH` idle states.
2. Confirm 15 V brick is rated ≥ 65 W and lists 15 V in PDO. Multimeter
   on trigger output should read 14.5–15.5 V no-load.
3. Confirm USB-C cable carries data, not charge-only: XIAO must
   enumerate as `/dev/cu.usbmodem*`.
4. Bench-test full boot ritual on `14_witness_provision/` with pointer
   attached. Verify lands within 1° of engraved visual-up.

**During mounting:**
5. Route SCL with clearance from chassis metal at the enclosure entry.
6. Keep the XIAO USB-C port accessible after mounting — only escape
   route for in-situ recalibration.
7. Don't swap any element of the validated power chain (brick, cable,
   PD trigger) when mounting.

**After mounting:**
8. Power-cycle. Watch SYSTEM STATUS for 14.5+ V Vin and green checks
   across motor / calibration / schedule / network.
9. Flash `15_calibration/`. Run recipe. Verify amplitude > 30 mA on
   each `diag` (run 2–3×, values within ±0.5° = solid).
10. Reflash `14_witness_provision/`. Power-cycle. Verify
    `phys-from-zero` < 1° on the post-spin landing.
11. Send `forcefetch`. Confirm sculpture's tracking az matches
    visualization within a degree.
12. Surface treatment (wax) the metal lids.

---

## 10. OW1/12 calibration history

| Date | `mass_offset_deg` | Context |
|---|---|---|
| 2026-04-29 | 10.83° | Initial pointer mount (`05_calibration` session) |
| 2026-04-30 | 21.74° | Pointer re-mount (`09_witness` session) |
| **2026-05-04** | **−15.96°** | **Final rigid wall mount; fit amplitude 78.0 mA. In NVS.** |

End-state at drop-off (2026-05-04 ~00:08 EDT): TRACKING, Vin 14.95 V,
RSSI −58 dBm, NTP synced, schedule fresh, post-spin landing 0.26°
from visual zero. Sculpture and visualization aligned to within
tracking-lag tolerance.

---

## 11. Serial commands

See `README.md` for the full table. Most-used:

```
release | hold | diag | home | spin | move <deg>
report | cal | setcal <deg> | savecal | loadcal
fetch | forcefetch | track | untrack | geo | regeo | forget_wifi
```

**Pre-boot SYSTEM STATUS block** (added 2026-05-04) prints right
before `power.waitForPower()` — consolidated snapshot of motor I²C,
Vin, calibration, schedule, geo, Wi-Fi, NTP, and boot flags. When
something hangs at the power gate, you can see exactly which
subsystem is wrong without guessing.

---

## 12. Pitfalls

- **XIAO ESP32-S3 USB-CDC reset on serial open.** Native USB-CDC
  doesn't follow classic DTR/RTS conventions. Opening the port from
  pyserial reliably resets the chip even with all flow-control flags
  off. Implications: every `pio device monitor` attach reboots the
  chip; cannot non-disruptively observe a running sculpture from a
  fresh monitor — wait through the boot ritual each time.
- **Wi-Fi connect on weak signal.** Default scan window is small.
  Patched in `Network::connect()`: 4 retry attempts with full
  `disconnect(true, true)` between, `WIFI_POWER_19_5dBm` (max),
  `setSleep(false)`. On total failure, dumps visible SSIDs +
  RSSI + channel for diagnosis. Comfortable connect at −52 dBm;
  −65+ wants a closer router.
- **Voltage gate hysteresis.** Originally tripped during spin-brake
  current spikes (Vin sagged 15.03 → 14.93 V for ~200 ms). Fixed by
  asymmetric thresholds + debounce: engage at 14.0 V, drop only at
  Vin < 13.5 V *for 3 consecutive 500 ms checks* (~1.5 s of real low
  voltage).
- **HTTPClient `getString()` for schedule fetch.** ArduinoJson's
  streaming parser couldn't keep up with TLS chunked response on
  weak Wi-Fi (`IncompleteInput` errors). Switched to buffer-then-parse:
  ~51 KB body, well within ESP32-S3 heap. If heap pressure ever rises,
  fall back to streaming-to-LittleFS.
- **Captive portal pre-scan.** Synchronous scan inside the form's
  GET handler made first page-load a 3–5 s hang. Pre-scan once before
  `httpServer.begin()`; serve from cache; `↻ Re-scan` link hits
  `/rescan` for explicit refresh.
- **Custom partitions.** Don't. See §6.

---

## 13. Motor reference + critical limits

Authoritative motor reference: `documentation/motor-roller485-lite/`
(register map, RS-485 / I²C protocols, library API, schematics). The
firmware's register addresses (`0x80` POSITION, `0xA0` POSITION_PID,
`0xC0` CURRENT_READ, etc.) come from there.

```
Max input voltage           16 V    (exceed → E:1 Over Voltage, motor disabled)
Max continuous phase current 0.5 A  (1 A = short-term burst, validated)
Brake-phase bursts          1.5 A   (rare, brief, no thermal limit)
Encoder scaling             36 000 counts = 360°
Absolute mechanical alignment ±2°    (calibration accuracy floor)
```

---

## 14. Open work (TODO)

**Per Edson's directive, none of these block shipping OW1/12.**

- **Quieter sculpture.** PID retuning lower-Kp tail at micro-corrections;
  softer stop transitions; mass damping on pointer carrier; isolation
  between motor housing and wall plate. Goal: gallery-quiet at
  conversational distance.
- **Motion ritual.** Design a deliberate gestural ritual on first
  tracker-lock or periodic cadence — phrase of motion that reads as
  intentional rather than utilitarian.
- **Tighten schedule fetch policy** from 24 h → 6 h for production
  deployments. Trivial data cost; real alignment improvement.
- **Skip calibration on software reset.** `esp_reset_reason()` →
  `ESP_RST_SW` skips diag, re-anchors zero from NVS-persisted
  `zeroOffsetCounts` if encoder is within tolerance of saved position.
  Saves ~110 s per dev reflash.
- **Wall-orientation offset.** A `wall_north_offset_deg` constant +
  serial command + NVS slot for installations that need true compass
  alignment. Skipped per Edson: "our interface should flatten the
  axis."
- **Sign-flip via serial.** `Tracker::setSignFlip()` exists but isn't
  reachable from the serial console. Add `tracksign +|−` command +
  NVS slot.
- **Schedule fetch resilience.** Streaming JSON to LittleFS, or a
  custom skim parser, if heap pressure ever rises.
- **Per-unit telemetry.** `SCULPTURE_NAME` as the schedule-fetch
  identifier so the webservice can address each unit individually.

---

## 15. File map

```
src/
  Calibration.{h,cpp}    NVS persistence of mass_offset_deg
  Calibrator.{h,cpp}     diag sweep + sinusoid fit + state machine
  Geolocation.{h,cpp}    ipapi.co IP geolocation + NVS persistence
  Logger.{h,cpp}         delta-encoded serial log
  MotorIO.h              I²C primitives + register addresses
  MotorState.h           shared state struct + enums
  MoveOperator.{h,cpp}   move-to-angle with eased trajectory
  Network.{h,cpp}        Wi-Fi + NTP + virtual UTC anchor
  PowerGate.{h,cpp}      Vin interlock + asymmetric thresholds
  Provisioning.{h,cpp}   captive portal: SoftAP + DNS + HTTP form
  Recipes.h              all validated PID + spin + cal constants
  ScheduleClient.{h,cpp} HTTPS fetch + JSON parse
  ScheduleStore.{h,cpp}  NVS schedule blob + sample interpolation + replay loop
  SinusoidFit.{h,cpp}    3-parameter least-squares fit
  SpinOperator.{h,cpp}   1080° spin: ramp / cruise / brake / settle
  Tracker.{h,cpp}        eased entry + rate-limited live tracking
  WifiCreds.{h,cpp}      raw nvs_* wrapper for fwcreds namespace
  secrets.h              SCULPTURE_NAME + Wi-Fi fallback + observer fallback
  main.cpp               orchestration: setup + loop + serial commands

../15_calibration/       slim non-network sketch — boots IDLE, no auto-home,
                         extended fit-quality output. Use for re-calibration
                         after physical re-mount.

../scanner_i2c/          two-pass I²C bus scanner with idle line-state read.
                         Pre-flight tool for sculptures 2/12+.

documentation/motor-roller485-lite/   authoritative motor reference.
```
