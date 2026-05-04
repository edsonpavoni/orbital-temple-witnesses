# First Witness — Stage 2 Knowledge (Offline behaviour & NVS gotchas)

What was learned hardening the tracking firmware so the sculpture keeps
moving when Wi-Fi or NTP isn't available — and the assorted ESP32 NVS
issues that surfaced along the way.

Stage 1 (Wi-Fi + tracking) is documented in `STAGE1_KNOWLEDGE.md`;
calibration / spin algorithms in `09_witness/KNOWLEDGE.md`.

---

## What works end-to-end (Stage 2)

The sculpture now degrades gracefully through three time regimes:

```
┌───────────────────┬─────────────────────┬─────────────────────────┐
│ NTP ever synced   │ Schedule cached     │ Behaviour               │
├───────────────────┼─────────────────────┼─────────────────────────┤
│ yes               │ yes (auto-refetch)  │ Real-time live tracking │
│ yes (was)         │ yes                 │ Real UTC + cached play  │
│ no, virtual anchor│ yes                 │ Replay-loop the cache   │
│ no                │ no                  │ Park at visual zero     │
└───────────────────┴─────────────────────┴─────────────────────────┘
```

The sculpture-as-art contract: **once it has seen one schedule, it
keeps moving forever, even if Wi-Fi never returns.** The cached day
loops as a "ghost" of a moment from satellite history.

---

## How the virtual clock works

`Network::nowUTC()` is a single function that picks the best clock
available, in order:

1. **Real UTC** — if `time(nullptr)` reads a sane value (>2023). True
   when NTP has ever succeeded this session, OR when the ESP32-S3's
   RTC has retained a previously-synced time across a soft reset (this
   surprises every newcomer; see "RTC retention" below).
2. **Virtual UTC** — if `s_virtualBaseUTC != 0`, returns
   `virtualBase + (millis() − virtualBaseMS)/1000`. The virtual anchor
   is set once at boot if NTP timed out AND a cached schedule exists.
   It points to the cache's `valid_from`, so replay starts at "the
   moment the cached day began."
3. **Zero** — no time available. Tracker refuses to activate and the
   pointer stays at visual zero.

Once real time arrives later (background SNTP poll succeeds), step 1
silently overrides — the tracker's next 200 ms tick reads the new
time and follows the live satellite from where the virtual clock was.

The schedule loop is implemented in `ScheduleStore::sampleAt()` — it
wraps the requested UTC into `[valid_from, valid_from + span)` modulo
the schedule span. In online operation this wrap never triggers
(daily fetches keep the cache fresh); in offline replay it turns the
24 h cache into an infinite loop.

---

## NVS gotchas — what we hit, in order

The ESP32's NVS partition is small (default ~20 KB on Arduino) and
shared by everything that wants persistence. Storing a 12 KB schedule
blob in there exposes several traps.

### Trap 1 — Wi-Fi creds eat NVS space

`WiFi.begin(ssid, pw)` writes the credentials to NVS by default
(`WiFi.persistent` defaults to `true`). Every connect attempt rewrites
them. With `WiFi.disconnect(true, true)` between attempts (which our
weak-signal retry loop does) the namespace fragments quickly. After
a few connect cycles the partition has < 12 KB free space for a blob
even though there's "nothing" persisted.

Fix: `WiFi.persistent(false)` once at startup, before `WiFi.mode()`.
The credentials are passed at runtime each connect — we don't need
them re-saved.

### Trap 2 — Stale namespace data blocks blob writes

Even after fixing #1, repeated schedule fetches can fragment the
`sched` namespace. Each fetch calls `replace()` which calls
`putBytes(KEY_BLOB, ...)`. Old data is supposed to be reclaimed
internally, but in practice we saw `nvs_set_blob fail: blob
NOT_ENOUGH_SPACE` after a few cycles.

Fix: call `Preferences::clear()` at the start of `saveToNVS()` —
free the namespace's keys before writing the new ones. The new blob
gets a clean run of pages. Empirically this clears the
NOT_ENOUGH_SPACE issue.

### Trap 3 — Custom partition tables and esptool's offset

We tried bumping the NVS partition to 64 KB via a custom
`partitions_witness.csv`. The board went into a boot loop. Cause:
PlatformIO/esptool wrote the new app at the *old* default app0 offset
(0x10000) while the new partition table said app0 should live at
0x20000. The bootloader read the new partition table, jumped to
0x20000, found garbage, and reset.

Lesson: changing `board_build.partitions` requires `pio run -t erase`
first to wipe the flash, otherwise the firmware lands at the wrong
offset. Even after erase, the issue is non-trivial — easier to keep
the default partition and shrink what we store.

We ended up not needing a bigger NVS — Trap 1 + Trap 2 fixes were
enough on the default 20 KB partition once we did one full erase to
clear accumulated stale data.

### Trap 4 — RTC retention across soft reset

Reflashing via `pio run -t upload` does a *soft* reset (DTR/RTS
toggle), not a hard EN-pin reset. ESP32-S3's RTC clock domain
survives soft resets — `time(nullptr)` returns whatever time was last
set (by a prior NTP sync), not zero.

This made the offline-replay test deceptively pass without exercising
the virtual-clock path: the firmware booted with bogus Wi-Fi and no
fetch, but `time(nullptr)` was already valid from the previous boot's
NTP, so the replay anchor never fired.

To genuinely test the virtual-clock path you have to physically
power-cycle the device (USB unplug + wait + replug). The test we did
validated everything except that one path; the code itself is correct
and was reasoned about with full knowledge of the RTC retention
behaviour. (For a future deployment validation, fully-erased + bogus
Wi-Fi + power cycle = full virtual-anchor exercise.)

---

## Wi-Fi reconnect cadence

Edson's directive: "Maybe try wifi update once an hour. No more."

Implementation: `Network::tick()` rate-limits the reconnect attempt
to one per hour. SNTP background polling (scheduled by `configTime()`
with default 1 h interval) handles re-syncing time once the link is
back — we don't need explicit logic for that.

```
RETRY_PERIOD_MS = 60UL * 60UL * 1000UL;    // one hour, exact
```

Side benefit: lower duty cycle on the radio, which is the second-
biggest power consumer after the motor.

---

## What changed in 12_witness_polish vs 11

| File | Change |
|---|---|
| `Network.h` | Added `anchorVirtualUTC()`, `isVirtualClock()`. Doc-comment describes the three time regimes. |
| `Network.cpp` | `nowUTC()` checks `time(nullptr)` first then virtual fallback. `isTimeSynced()` reads ground truth from `time()` rather than caching a flag. `WiFi.persistent(false)` added. Wi-Fi reconnect throttled to 1 h. |
| `ScheduleStore.cpp` | `saveToNVS()` calls `Preferences::clear()` before each blob write. `sampleAt()` wraps `nowUTC` modulo the schedule's span (loops the cache offline). Logs partial-write failures. |
| `main.cpp` | Anchors virtual UTC at boot if no NTP and cache exists. Tracker activation gated on `nowUTC() != 0` rather than `isTimeSynced()` (so it fires in either real or virtual mode). `report` shows `Clock mode: real|virtual|none`. |
| `TODO.md` | Added "End-user Wi-Fi onboarding" item from Edson's note. |

`MotorIO`, `MotorState`, `Recipes`, `SinusoidFit`, `Calibration`,
`Calibrator`, `MoveOperator`, `SpinOperator`, `Logger`, `PowerGate`
were not touched.

---

## Validated performance (2026-04-30)

End-to-end live tracking (real Wi-Fi + real NTP):
- Sculpture pointer matches the visualization within PID-tracking lag.
- `report` shows `Clock mode: real (NTP)`, schedule fetched, tracking
  az/el live.

Offline test (bogus SSID, schedule pre-populated):
- Wi-Fi: disconnected (4 attempts × 20 s + scan, all fail).
- Schedule loads from NVS (blob persisted across reboot).
- Tracker activates in HOLDING after spin completes.
- Sculpture follows cached azimuth.
- (Virtual-anchor path itself was not exercised because RTC retained
  previous NTP time across the soft reset; see Trap 4.)

---

## Tasks remaining → see TODO.md

- IP geolocation for observer position (next phase, `13_witness_geo`).
- End-user Wi-Fi onboarding (captive portal / BLE / etc.).

`#1 (skip calibration on software reset)` was deferred — Edson called
"pass" on it for now.
