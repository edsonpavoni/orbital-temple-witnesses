# 16_witness_choreography — recurring sat/release/spin/release cycle

Forked from `14_witness_provision` on **2026-05-04**. All boot, calibration,
Wi-Fi, NTP, and schedule plumbing is identical. The only change is the
post-boot behavior: instead of continuously following the satellite, the
sculpture runs a 36-second choreographed cycle forever:

```
A. ENTER_SAT  8 s  — eased cosine ramp from current angle → live sat az
B. HOLD_SAT  30 s  — frozen at the captured az; tracker disabled
C. RELEASE_1  2 s  — REG_OUTPUT=0; pointer free-falls to gravity-down
D. SPIN     ~12 s  — 07-style speed-mode 1080° at 10 RPM (random CW/CCW)
E. → loop back to A from the spin endpoint (no second release)
```

Total cycle ≈ 52 s. Per Edson 2026-05-04: the second release was cut —
the pre-spin drop is the gesture; a second drop just before re-finding
the satellite is redundant and only adds drift.

**Design notes:**

- **The release is a deliberate gesture.** The unbalanced pointer falls
  visibly to gravity-down between phases. Two seconds is enough to read
  as a "drop" without dragging out the cycle. Bump
  `Recipes::CHOREO_RELEASE_MS` if the pointer hasn't settled by spin start.

- **Spin uses 07-style loose speed-PID.** Speed mode, P=1.5e6, I≈0,
  D=4e7. Gravity-affected swing (rotor accelerates downhill, slows
  climbing). End angle is non-deterministic by ±5–30°. *This is fine* —
  the next phase re-finds the satellite, so landing precision is
  irrelevant. The validated boot ritual still uses position-mode
  rate-integration (SpinOperator) to land at exactly visual zero — the
  speed-mode spin is reserved for choreography only.

- **Random direction.** `esp_random()` picks CW vs CCW each cycle.

- **Schedule degradation.** If the sat schedule isn't cached or the
  clock isn't synced, ENTER_SAT falls through and the pointer just
  holds at its current angle for the regular HOLD window. The cycle
  continues — the artwork is alive even fully offline.

- **State during spin.** The Choreographer sets `motor.state =
  ST_SPIN_CRUISE` while ChoreoSpin runs, so the delta logger emits
  `ph=S` and the spin is observable in the log even though the underlying
  control mode is speed (not the position-mode spin tracked by
  SpinOperator).

---

## How to flash

```bash
cd code/16_witness_choreography
~/.platformio/penv/bin/pio run -t upload
```

Then power-cycle the motor (15 V brick), or just leave the XIAO USB on
for development without torque.

---

## Serial commands

All commands from `14_witness_provision` work identically. Two new
commands manage the choreography:

| Command  | Effect |
|----------|--------|
| `choreo` | Start the choreography loop (motor must be holding) |
| `chstop` | Halt the loop. Motor stays in whatever state it was in. |

Other motion commands (`release`, `hold`, `home`, `spin`, `track`,
`move`) implicitly call `choreo.stop()` so the loop doesn't fight you.

The boot ritual auto-starts the choreographer once the boot 1080° lands
at visual zero. To disable that, set `BOOT_CHOREO_ENABLED = false` in
`main.cpp`.

---

## Compressed delta log — phases

Standard log schema from 14, plus a new phase code for the spin:

| Code | Phase |
|------|-------|
| `R`  | Released (motor output OFF — RELEASE_1, RELEASE_2) |
| `H`  | Holding (HOLD_SAT, post-spin re-engage moment) |
| `T`  | Tracking (the 8 s ENTER_SAT eased ramp) |
| `S`  | Spin cruise (the choreography spin) |
| `M`  | Moving (boot ritual move-to-zero) |
| `C/P/A` | Calibration sweep phases |
| `u/B/W` | Boot 1080° spin sub-phases |

Choreographer phase transitions also emit `>> Choreo: -> PHASE` lines
so the cycle is legible without parsing the delta log.

---

## Files

```
16_witness_choreography/
├── README.md                # this file
├── KNOWLEDGE.md             # inherited from 14, see "Choreography" addendum
├── platformio.ini
├── partitions_witness.csv
├── src/
│   ├── ChoreoSpin.{h,cpp}        # NEW: 07-style speed-mode 1080° spin
│   ├── Choreographer.{h,cpp}     # NEW: cycle state machine
│   ├── MotorIO.h                 # extended: speed-mode primitives
│   ├── Recipes.h                 # extended: SPIN07_* + CHOREO_* constants
│   ├── Tracker.h                 # extended: isEntering()
│   ├── main.cpp                  # extended: boot-to-choreo trigger + commands
│   └── ... (everything else identical to 14)
└── tools/                        # identical to 14 (lab.py, etc.)
```
