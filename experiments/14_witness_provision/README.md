# 09_witness — First Witness, final firmware

The unified firmware for the *First Witness* sculpture. On every power-on
the wall-mounted unbalanced pointer autonomously calibrates its absolute
orientation, rotates to true visual vertical, performs one 1080° spin
(three full revolutions), and brakes precisely back to visual zero.

**Validated 2026-04-30.** End-to-end ritual lands within ±0.3° of visual
zero. Mass-offset constant (21.74°) persisted in flash, survives power
cycles. No human intervention needed at runtime.

For the design rationale, the journey from 03 through 09, and what to do
when the calibration drifts → see [`KNOWLEDGE.md`](KNOWLEDGE.md).

---

## What the sculpture does on power-on

```
t=0     Boot, load mass_offset_deg from NVS.
t=2s    "Kick in 3 s..." — step back from the sculpture.
t=5s    Latch current encoder as working zero.
t=5s    Diagnostic 2-rev CW sweep (loose PID, slow).
t=42s   Pause 1.5 s.
t=43s   Diagnostic 2-rev CCW sweep back to start.
t=80s   Sinusoid fit on (encoder, current) samples → gravity-up angle.
        Apply mass_offset_deg → visual-up angle. Re-anchor zero.
        Move 0–13° to land at visual vertical.
t=82s   1080° spin: ramp 0 → 8 RPM, cruise (controlled), brake at exact
        start angle (= visual zero).
t=110s  Done. Hold at visual zero indefinitely.
```

That's the entire artwork loop from a power-on. To trigger it again
manually without rebooting, send `home` over serial.

---

## Hardware

- **MCU** Seeed XIAO ESP32-S3 (USB-C)
- **Motor** M5Stack Unit-Roller485 Lite (BLDC + integrated FOC + magnetic
  encoder + position/speed/current PIDs all on-chip)
- **I²C** address `0x64` on `SDA = GPIO8`, `SCL = GPIO9` @ 100 kHz
- **Power** 15 V via USB-C PD trigger to the motor's PWR-485 input. The
  motor needs the full 15 V — at 11 V it cannot break static friction
  with the unbalanced pointer (validated 2026-04-29 in 06).
- **Mechanical** Direct-drive wall-mounted slightly-unbalanced pointer
  (gravity creates angle-dependent torque — the algorithm exploits this).

---

## How to flash

```bash
cd code/09_witness
~/.platformio/penv/bin/pio run -t upload
```

Then power-cycle the motor (just the XIAO USB is enough for testing
without the 15 V brick — the firmware will run but the homing sweep
won't have torque to spin the pointer).

For development, use the serial bridge:

```bash
~/.platformio/penv/bin/python tools/lab.py start          # background
~/.platformio/penv/bin/python tools/lab.py events 30      # see what happened
~/.platformio/penv/bin/python tools/lab.py send "home"    # re-run ritual
~/.platformio/penv/bin/python tools/lab.py stop
```

---

## Serial commands (for tuning / diagnostics)

These are NOT required for the artwork to function — the boot ritual is
fully autonomous. They exist for development.

| Command | Effect |
|---------|--------|
| `release` | Disable motor output (free-coast — for hand-positioning) |
| `hold` | Latch current encoder as user 0, hold in position mode |
| `diag` | Run a 2-rev sweep without applying homing — research only |
| `home` | Re-run the homing ritual + auto-spin |
| `spin` | Trigger one 1080° spin (motor must already be holding) |
| `move <deg>` | Move to a user-frame angle (production PID) |
| `report` | Live readbacks + current state |
| `cal` | Print current `mass_offset_deg` |
| `setcal <deg>` | Set `mass_offset_deg` in RAM only |
| `savecal` | Persist current `mass_offset_deg` to NVS |
| `loadcal` | Reload `mass_offset_deg` from NVS |
| `fetch` | Refresh schedule (only if `fetchDue` thinks it's time — 24 h policy) |
| `forcefetch` | Refresh schedule **now**, bypassing the 24 h policy. Use when sculpture and visualization disagree because the server's TLE has moved on. |
| `geo` / `regeo` | Print / re-fetch IP-geolocated observer location |
| `forget_wifi` | Wipe stored Wi-Fi creds, reboot to captive portal |
| `track` / `untrack` | Enable / disable satellite tracking |

---

## Recalibration (when needed)

If the pointer ever lands visually off-zero after the boot ritual (most
likely: the pointer was re-mounted, or temperature/aging shifted the mass
distribution), recalibrate `mass_offset_deg`:

```
release                # motor coasts; you can hand-position
... position pointer at TRUE visual vertical ...
hold                   # latch this position as user 0
diag                   # 2-rev research sweep
                       # firmware prints: ">> fit: gravity-up at user-frame X.XX"
setcal X.XX            # take that X.XX value
savecal                # persist to NVS
home                   # verify it lands on visual zero
```

This was the recovery path used 2026-04-30: the previous 10.83° offset
landed visually off by ~11° after the pointer was re-mounted; a fresh
hand-positioned recalibration produced 21.74°, which persists in NVS.

---

## Compressed delta log

Every 200 ms the firmware emits one line of `key=value` pairs over
serial — only fields that *changed* are emitted.

| Key | Meaning |
|-----|---------|
| `t` | millis() — always present (anchors the line) |
| `ph` | Phase: `R` released, `H` holding, `M` moving, `C` diag-CW, `P` diag-pause, `A` diag-CCW, `u` spin-rampup, `S` spin-cruise, `B` spin-brake, `W` spin-settle |
| `Tg` | Logical target deg (user frame) |
| `sp` | Trajectory setpoint deg (interpolated NOW) |
| `p` | Encoder readback (user frame) |
| `er` | `sp − p` |
| `a` | Actual RPM |
| `cur` | Motor current draw (mA) — gated on output enabled |
| `tmp` | Motor temp (°C) — gated on output enabled |
| `v` | Vin (V) |

---

## Files

```
09_witness/
├── README.md           # this file
├── KNOWLEDGE.md        # design journey + algorithms + final design
├── FINDINGS.md         # original 05 calibration journey (kept verbatim for reference)
├── platformio.ini
├── src/main.cpp        # firmware
└── tools/
    ├── lab.py                 # serial bridge
    ├── phase0_plot.py         # off-board calibration dev tool (history)
    ├── homing_analysis.py     # off-board calibration dev tool (history)
    └── results/               # captured logs from 05 validation
```
