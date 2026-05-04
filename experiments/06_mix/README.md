# 06_mix

First Witness sculpture firmware that **mixes Mode 1 (precise position)
and Mode 2 (smooth rotation)** into a single autonomous behavior loop.
Created **2026-04-29**.

This is intentionally a *small* mix — no calibration, no NVS, no extra
features. Just the two motion regimes, glued together with a state
machine. If you (future Claude or future Edson) are picking this up cold,
this README plus the source files should be enough to be productive
within minutes.

---

## What the sculpture does

User physically positions the pointer at visual zero with their hand
**before powering the device on**. That's the only hand-positioning ever
needed; nothing else assumes a calibration step.

On power-on:

```
1 s wait   ─►  engage hold-here (latch encoder = user-frame zero)
              │
              ▼
        ┌──────────────────────────────────────────────┐
        │   pick random angle in [0, 360°)             │
        │   precise.moveTo(currentTarget + delta)      │ Mode 1
        │   wait for arrival                           │ (precision ±0.5°)
        │   hold 3 s                                   │
        ├──────────────────────────────────────────────┤
        │   smooth.start360CW()                        │ Mode 2
        │   wait for rotation done                     │ (smooth pendulum)
        │   hold 5 s                                   │
        │   ┌─ if rotations remain ─► back to spin ───┘
        │   └─ else (3 done) ─►                        │
        └──── back to top of loop (new random) ────────┘
```

Spec confirmed by Edson 2026-04-29:

| Detail | Value |
|--------|-------|
| Boot pre-delay | 1 s |
| Hold at random target | 3 s |
| Hold between rotations | 5 s |
| Hold after final rotation | 5 s (then immediate new random) |
| Rotations per random target | 3 |
| Random angle range | full `[0, 360°)` |
| Rotation direction | always CW |
| Smooth-rotation cruise speed | 8 RPM |
| Smooth-rotation ramps | 2000 ms cosine S-curve each |

---

## Architecture

`main.cpp` keeps **only** the structural concerns — Wire init, serial,
the eternal-cycle state machine, periodic delta log. Each of the two
motion regimes lives in its own class.

```
src/
├── main.cpp              # state machine + serial + delta log (251 lines)
├── MotorIO.h             # header-only: I²C primitives, register defines (100 lines)
├── PreciseOperator.h     # Mode 1 declarations (67 lines)
├── PreciseOperator.cpp   # Mode 1 implementation (108 lines)
├── SmoothOperator.h      # Mode 2 declarations (60 lines)
└── SmoothOperator.cpp    # Mode 2 implementation (78 lines)
```

### `MotorIO.h` (header-only)
The only file that references `Wire`. Declares register addresses, exposes
`writeReg8/32/96`, `readReg32`, and convenience readbacks (`encoderRaw()`,
`actualRPM()`, `currentMA()`, `vinV()`, `tempC()`). Both classes use
these. Also declares `easeInOut()` so neither class has to redeclare it.

### `PreciseOperator` — Mode 1
Closed-loop position-mode moves with the production recipe validated by
`04_position_lab` (test 2H + test 3, ±0.5° accuracy from any start).

**Recipe (compile-time constants in `PreciseOperator.h`):**
```
kp = 30,000,000   ki = 1,000   kd = 40,000,000
mc = 1000 mA      easing = cosine S-curve
mt = 33.33 ms per deg of move distance
```

**Public methods:**
- `engageHoldHere()` — Latch current encoder as user-frame 0; switch motor
  to POSITION mode + apply production PID; hold. Called once after the
  1 s boot delay.
- `release()` — Disable motor output.
- `moveTo(deg)` — Begin an eased move from the current target to `deg`.
  `deg` is in user-frame degrees; can exceed 360 (encoder is cumulative).
- `tick()` — Drive the trajectory each loop iteration. **No-op while
  disengaged** (i.e., during a smooth rotation).
- `reEngageAfterSpeedMode()` — `SmoothOperator` calls this at the end of
  each rotation to put the motor back in POSITION mode at whatever
  encoder it's at, **keeping the original zero offset**. The pointer is
  at the same physical angle but `currentTargetDeg` is now ~360° larger.
- Status/getters: `isEngaged()`, `isMoving()`, `currentTargetDeg()`,
  `currentSetpointDeg()`, `progressPct()`, `zeroOffsetCounts()`.

### `SmoothOperator` — Mode 2
Speed-mode 360° rotation. Recipe from `03_motion_test` — the motor's
loose internal speed PID lets gravity dynamics show through, producing
the "pendulum swing" feel.

**Recipe (compile-time constants in `SmoothOperator.h`):**
```
target speed   = 8 RPM
max current    = 1000 mA
each ramp      = 2000 ms cosine S-curve (0 → 8 → 0 RPM)
cruise window  = 5500 ms (so total angular travel ≈ 360°)
speed PID      = whatever's in motor flash
                 (validated as kp=1.5M, ki=1k, kd=4M in 03's lab)
```

The 5500 ms cruise is calculated:
- each 2 s ramp covers ~48° (avg of cosine = 0.5 × 8 RPM × 6 deg/s × 2 s)
- two ramps = 96° total
- cruise needs to cover 360° − 96° = 264°
- cruise time = 264° / (8 × 6 deg/s) = 5500 ms

**Public methods:**
- `start360CW()` — Switch motor to SPEED mode (jolt-safe disable → mode
  change → start at 0 RPM → re-enable), begin the ramp-up phase.
- `tick()` — Drive the state machine `RAMP_UP → CRUISE → RAMP_DOWN → IDLE`
  each loop iteration. At the end of `RAMP_DOWN`, calls
  `precise.reEngageAfterSpeedMode()` to hand control back.
- `isRotating()` — true while the rotation is in flight.

**Constructor takes a reference to `PreciseOperator`** so it can hand
control back at the end of each rotation. (One-way coupling: precise
doesn't know smooth exists.)

### `main.cpp`

Holds the **eternal-cycle state machine**:

```
WAIT_BOOT (1 s)
  → ENGAGE_HOLD (one-shot)
  → PICK_RANDOM (one-shot)
  → MOVE_TO_RANDOM (until !precise.isMoving())
  → HOLD_AT_RANDOM (3 s)
  → SPIN (until !smooth.isRotating())
  → HOLD_AFTER_SPIN (5 s)
        ├─ if rotations_remain > 0 → SPIN
        └─ else                    → PICK_RANDOM
```

Plus:
- One-time `Wire.begin()` and motor sanity check
- Serial banner + log schema printout
- Periodic delta-log emitter (200 ms cadence)
- 100 Hz main loop (`delay(10)`)

---

## Compressed delta log

Emitted to Serial every 200 ms. **Only fields that changed since the
previous emitted line appear.** If nothing changed, the line is suppressed
entirely (silence == still the same).

| Key | Meaning |
|-----|---------|
| `t` | millis() — always present (anchors the line) |
| `st` | State char: `B`=wait_boot `E`=engage_hold `P`=pick_random `M`=move `H`=hold_at_random `S`=spin `h`=hold_after_spin |
| `Tg` | Logical target deg (user frame; can exceed 360°) |
| `sp` | Trajectory setpoint deg (interpolated NOW; equals Tg when idle) |
| `p` | Encoder readback in user-frame deg |
| `er` | `sp − p` |
| `a` | Actual RPM (signed) |
| `tr` | Move trajectory % (0–100, `-` when not moving) |
| `tmp` | Motor temperature (°C) |
| `v` | Vin (V) |

---

## How to flash + run

```bash
cd code/06_mix
~/.platformio/penv/bin/pio run -t upload
~/.platformio/penv/bin/pio device monitor
```

Position pointer at visual 0 with your hand **before** powering on. After
the 1-second boot delay the pointer will engage and the cycle begins.

---

## `tools/lab.py` — remote closed-loop dev

`tools/lab.py` is a serial bridge. It owns the `/dev/cu.usbmodem101` port,
streams the firmware's compressed log into a file in `/tmp/`, and lets
another process (you, this README, automated tests, a future-Claude
session) inject commands via a FIFO. **You can't have PlatformIO Monitor
open at the same time** — only one process can hold the serial port.

### Why it exists

Without the bridge, every iteration goes:
1. Stop the monitor
2. Edit firmware
3. Build + upload
4. Restart monitor
5. Watch behavior

The bridge stays connected continuously. You can edit, upload, and watch
the firmware re-boot all without losing context. It also lets a Claude
session capture firmware output in real time and react to it — closed-
loop development.

### Subcommands

```
~/.platformio/penv/bin/python tools/lab.py start    # background bridge — use run_in_background
~/.platformio/penv/bin/python tools/lab.py send "<cmd>"   # forward one line to the motor
~/.platformio/penv/bin/python tools/lab.py events 15      # last N event lines (cmds + firmware msgs only)
~/.platformio/penv/bin/python tools/lab.py session         # current-session log only (incl. delta lines)
~/.platformio/penv/bin/python tools/lab.py log            # full log file (every session)
~/.platformio/penv/bin/python tools/lab.py tail 50        # last 50 lines of the full log
~/.platformio/penv/bin/python tools/lab.py clear          # truncate the log
~/.platformio/penv/bin/python tools/lab.py status         # bridge status
~/.platformio/penv/bin/python tools/lab.py stop           # SIGTERM the bridge
~/.platformio/penv/bin/python tools/lab.py run plan.txt   # execute a plan file
```

### Token efficiency

`events N` is the cheap way to see what happened. It returns only `[->]`
markers (commands sent) and `>>` messages (firmware prints), filtering
out the dense delta-log lines. Use it instead of `log` for routine
debugging. `session` is a middle ground — full data but only since the
most recent `=== bridge start ===` marker.

### Files it creates

| Path | Purpose |
|------|---------|
| `/tmp/lab_pos.fifo` | Named pipe for command injection |
| `/tmp/lab_pos.log` | Append-only capture of all firmware output |
| `/tmp/lab_pos.pid` | PID of the running bridge |

Cleaned up on `stop`.

### Plan files

A plan is plain text, one per line:
- `<command>` — sent to the motor verbatim
- `wait <seconds>` — pause before continuing
- lines starting with `#` — comments

Example:
```
# wait for the boot ritual to complete
wait 5
# (this firmware doesn't accept commands during the cycle, but
# future iterations might)
```

### Quick recipe — closed-loop dev session

```bash
# in one terminal:
cd code/06_mix
~/.platformio/penv/bin/python tools/lab.py start

# in another terminal:
~/.platformio/penv/bin/python tools/lab.py events 30   # see what's happening
```

Or, for Claude doing the same:
```python
# 1. Stop any prior bridge
lab.py stop
# 2. Start in background
lab.py start  (with run_in_background=True)
# 3. Wait for output
sleep N
# 4. Inspect
lab.py events 20
# 5. Stop when done
lab.py stop
```

---

## State of the project at handoff

**What works (validated 2026-04-29):**
- Builds clean (RAM 5.7%, Flash 8.5%).
- Architecture is the small one Edson asked for: two operator classes +
  a thin main.cpp + a header-only motor helper.

**What's NOT validated yet:**
- The actual eternal cycle on the wall sculpture. Edson hasn't flashed +
  observed at the time of this README. The previous attempt (06_artwork)
  wasn't doing the smooth motion correctly during the 360°; this version
  uses true SPEED mode for the rotation, matching what 03_motion_test
  did, so the pendulum-swing feel should be back.
- The handoff between SmoothOperator and PreciseOperator at the end of
  each rotation. The implementation is straightforward (smooth disables
  output, precise re-enables in position mode), but the encoder-frame
  bookkeeping deserves observation: after each rotation, `currentTargetDeg`
  in PreciseOperator grows by ~360° (cumulative encoder, same physical
  angle). Random-target picking adds a forward delta to that, so absolute
  encoder counts grow monotonically. int32 overflow not a practical concern.

**Likely first issue to investigate if the cycle misbehaves:**
- Whether the speed PID currently in motor flash is the same loose recipe
  03_motion_test used (kp=1.5M, ki=1k, kd=4M). If the motor flash
  has been overwritten with something else (e.g. by 04 or 05's tuning),
  the smooth rotation may not look smooth. Fix: write the speed PID
  explicitly in `SmoothOperator::start360CW()` before enabling output.

---

## Pointers to background context

If you need more depth on any of the recipes:

- **Mode 1 derivation** — `code/04_position_lab/RECIPE-AND-FINDINGS.md`
  explains why kp=30M, ki=1k, kd=40M is the right choice and how it was
  validated across the full 0–360° range.
- **Mode 2 derivation** — `code/03_motion_test/README.md` covers the
  ingredients of "the pointer rotates beautifully": cosine S-curve velocity,
  2000 ms transitions, loose PID, direct drive.
- **Sensorless calibration** (NOT used here — Edson explicitly said hand-
  position is enough) — `code/05_calibration/FINDINGS.md`.
- **Motor reference** —
  `code/documentation/motor-roller485-lite/CLAUDE-REFERENCE.md` has the
  full register map, electrical limits, and protocol details.

---

## Hardware

- Seeed XIAO ESP32-S3
- M5Stack Unit-Roller485 Lite (BLDC + integrated FOC + magnetic encoder)
- I²C at `0x64` on `SDA = GPIO8`, `SCL = GPIO9`
- 15 V via USB-C PD trigger to the motor's PWR-485 input
- Wall-mounted slightly-unbalanced pointer, direct drive
