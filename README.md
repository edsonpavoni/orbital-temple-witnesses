# Orbital Temple Witnesses — Firmware

Firmware for the *First Witness* kinetic sculptures from the Orbital Temple
project by Edson Pavoni. Each sculpture is a wall-mounted unbalanced pointer
driven by a brushless motor. On power-up the pointer autonomously calibrates
its orientation using a sensorless gravity algorithm, spins five full
revolutions with a smooth gravity-affected arc, and then locks to the live
azimuth of the Orbital Temple satellite. The cycle repeats continuously,
pointing at the sky and spinning away from it, forever.

**Production firmware:** `firmware-v1.0/`
**Build tool:** PlatformIO

---

## Quick start

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- XIAO ESP32-S3 connected via USB-C
- Motor powered at 15 V via USB-C PD trigger

### Build

```bash
cd firmware-v1.0
~/.platformio/penv/bin/pio run
```

### Flash

```bash
cd firmware-v1.0
~/.platformio/penv/bin/pio run -t upload
```

Then power-cycle the 15 V motor supply (not the XIAO). The boot ritual
starts immediately: gravity calibration (~80 s) then the 1080° boot spin
then the satellite-tracking choreography loop.

### Monitor serial

```bash
~/.platformio/penv/bin/pio device monitor
```

115200 baud. Press `?` to see available commands.

---

## What the sculpture does

On every power-on:

```
1. Power gate check  —  confirms 15 V motor supply is live
2. Wi-Fi + NTP       —  connects, syncs clock, fetches satellite schedule
3. Gravity homing    —  2-rev CW + 2-rev CCW sweep, sinusoid fit, finds
                        visual-up within ±2°. (~80 s)
4. Boot spin         —  1080° (3 revolutions) position-mode spin, brakes
                        at visual zero within ±0.5°
5. Choreography loop —  repeating ~110 s cycle:
     A. ENTER_SAT   ~8 s   eased cosine ramp to live satellite azimuth
     B. HOLD_SAT    30 s   frozen at captured azimuth
     C. SPIN        ~42 s  speed-mode cruise 8 RPM + position-mode brake
                           at 1800° (5 revolutions CW)
     D. SPIN_HOLD   30 s   hold at brake landing
     E. repeat from A with re-acquired satellite position
```

The pointer stops at the live satellite azimuth (e.g., 151°) and holds
there for 30 seconds so visitors can see exactly where Orbital Temple is.
The 5-revolution spin is the memory of that direction departing. Then it
finds the satellite again.

---

## Hardware

| Component | Part |
|-----------|------|
| Microcontroller | Seeed XIAO ESP32-S3 (USB-C) |
| Motor | M5Stack Unit-Roller485 Lite (BLDC + integrated FOC, I2C `0x64`) |
| I2C pins | SDA = GPIO8, SCL = GPIO9, 100 kHz |
| Motor power | 15 V via USB-C PD trigger to PWR-485 port |
| Mechanical | Direct-drive wall-mounted unbalanced pointer |

The motor needs the full 15 V. At 11 V it cannot break static friction
with the unbalanced pointer.

### Credentials

Copy `firmware-v1.0/src/secrets.h.example` to
`firmware-v1.0/src/secrets.h` and fill in your Wi-Fi and API credentials.
`secrets.h` is gitignored and never committed.

---

## Repository layout

```
firmware-v1.0/          Production firmware (v1.0, 2026-05-04)
  src/                  C++ source files
  tools/                lab.py serial bridge for development
  platformio.ini
  README.md             Operational guide (cycle, commands, commissioning)
  KNOWLEDGE.md          Architecture, algorithms, bug history, tuning

experiments/            All prior numbered iterations (archaeology)
  LINEAGE.md            One-paragraph summary of what each experiment was for
  03_motion_test/       Speed-mode rotation lab
  04_position_lab/      Position-mode landing lab
  05_calibration/       Sensorless gravity homing (standalone)
  05b_calibration_redo/ Calibration redo session (stages 1-4)
  06_mix/               First unified loop (point + spin, no calibration)
  07_smooth1080/        1080° speed-mode spin with position-mode lock (3 rev)
  07_smooth_1800/       1800° variant of 07 (5 rev, same control scheme)
  08_brake1080/         Position-mode rate-integrated 1080° spin (deterministic)
  09_speed_brake/       Speed-mode cruise + position-mode brake hybrid (lab)
  09_witness/           First complete unified firmware (calibrate + spin)
  10_witness_modular/   Refactored into module classes (Calibrator, SpinOp...)
  11_witness_tracking/  Added live satellite tracking (Tracker, ScheduleClient)
  12_witness_polish/    Stability polish and Wi-Fi hardening
  13_witness_geo/       Added geolocation for observer lat/lon (Geolocation)
  14_witness_provision/ Added Wi-Fi captive portal provisioning + NVS storage
  16_witness_choreography/ Added recurring choreography loop (ChoreoSpin, drop-and-spin)

reference/              Reference material for active development
  motor-roller485-lite/ Full Roller485 Lite register map and electrical specs
  scanner_i2c/          I2C scanner utility for bus debugging

archive/                Kept for archaeology, not active use
  legacy-examples/      Pre-numbered prototype sketches from the earliest phase
```

---

## Key documents

| Document | Purpose |
|----------|---------|
| `firmware-v1.0/README.md` | Operational: cycle, serial commands, commissioning checklist |
| `firmware-v1.0/KNOWLEDGE.md` | Architecture deep-dive, bug history, tuning knobs |
| `experiments/LINEAGE.md` | Development history, what each iteration added and why |
| `KNOWLEDGE.md` (this folder) | Consolidated architecture + integration bugs for future developers |
| `CHANGELOG.md` (this folder) | Version history |

---

## License

MIT License. See LICENSE file for details.

## Author

Edson Pavoni — Orbital Temple Project
