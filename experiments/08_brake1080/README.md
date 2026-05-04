# 08_brake1080

Forked from `03_motion_test` on **2026-04-30**. Validated motion for
the wall-mounted unbalanced pointer: a slow controlled 1080° rotation
that stops at the angle it started, plus a separate slow reposition
primitive for setting an arbitrary starting angle.

This is **the chosen motion** for the artwork. Earlier attempts:

- `06_mix` — autonomous random-target cycle. Had the right
  scaffold but not the right rotation behavior.
- `07_smooth1080` — landed within ±0.5° but the rotation was a
  smooth eased ramp, not a deliberate stop. Reserved as a reference;
  see `07_smooth1080/FINDINGS.md`.

---

## Motion model

Two primitives, both running in **position-mode rate-integrated**
control. The motor's position PID tracks a setpoint that we move
deterministically. This eliminates the gravity-runaway that loose
speed-mode PID exhibited (the rotor hit 19 RPM during 8 RPM cruise via
gravity assist before this approach was adopted).

### `r<deg>` — slow reposition

Move the pointer to an absolute physical angle. Always picks the shorter
direction (CW or CCW).

```
REPOS_RAMP_UP    cosine ease 0 → ±REPOS_PEAK_RPM (4) over rampMs
REPOS_CRUISE     hold ±4 RPM until remaining = ramp-down travel
REPOS_RAMP_DOWN  cosine ease ±4 → 0 over rampMs
STILL_WAIT       (no chain) → IDLE
```

Ramp time scales with move distance: `rampMs = (dist/48) × 2000` for
moves shorter than 48°, capped at 2 s. This keeps short moves from
overshooting (a 30° move at full 2 s ramps would integrate past the
target).

### `g` — 1080° spin + brake at start

```
STILL_WAIT       wait for ≥ STILL_HOLD_MS (2 s) of motionless encoder
SPIN_RAMP_UP     cosine ease 0 → SPIN_CRUISE_RPM (8) over 2 s
SPIN_CRUISE      hold 8 RPM until setpoint has moved 1080°
SPIN_BRAKE       freeze setpoint at start + 1080°; zero velocity instantly.
                 Position PID with bumped current ceiling (BRAKE_MAX_MA)
                 brakes the rotor's residual kinetic energy in ~1°.
SPIN_SETTLE      hold 2 s, print final encoder + error, stay locked.
```

Total runtime ≈ 24.5 s (2 s ramp-up + ~22 s cruise covering 1080° at
8 RPM = 48 deg/s + brake/settle).

### `q<deg>` — chained

`r<deg>` → `STILL_WAIT` → `g` in one command. This is the test routine
used during validation.

### Stillness gate

Before any spin starts, the firmware reads the encoder repeatedly and
requires `STILL_HOLD_MS` (2 s) of consecutive motionlessness — defined
as the encoder staying within `STILL_THRESH_DEG` (0.4°) of its reference
reading. Any deviation resets the stillness timer. Guards against
starting a spin while the motor is still settling from a recent
reposition or external disturbance.

---

## Validated performance (2026-04-30)

10 consecutive `q<random_deg>` cycles, random angles spanning the full
360° range:

| metric | value |
|---|---|
| spins completed | 10 / 10 |
| max abs error | **1.70°** |
| mean abs error | **0.68°** |
| within ±1.0° | 8 / 10 |
| within ±0.5° | 5 / 10 |
| brake-fire RPM | 6.7 – 7.9 (commanded 8) |
| motor temp range | 48–51 °C (started 51, ended 48–50) |
| total runtime | 6:52 |

Temperature stayed flat across the 7-minute run. The position-mode
cruise draws far less current than the gravity-runaway speed-mode
cruise that was briefly tried (which would have thermal-limited).

---

## Constants

```cpp
SPIN_TOTAL_DEG       1080.0    // 3 full revolutions
SPIN_CRUISE_RPM      8.0       // setpoint speed during spin cruise
REPOS_PEAK_RPM       4.0       // peak speed during reposition
BRAKE_MAX_MA         1500      // position-mode current ceiling (live-tunable: B<mA>)
SETTLE_MS            2000      // hold after brake before reporting
STILL_HOLD_MS        2000      // continuous stillness required to allow spin
STILL_THRESH_DEG     0.4       // max enc dev to count as "still"
BRAKE_PID            kp=30M, ki=1k, kd=40M  // production gains from 04_position_lab
```

---

## Serial commands

| Cmd | Purpose |
|-----|---------|
| `r<deg>` | Reposition slowly to absolute physical angle (0–359) |
| `g` | Spin 1080°, brake at start angle (waits for stillness) |
| `q<deg>` | Reposition → stillness check → spin, in one command |
| `B<mA>` | Set position-mode max current (brake authority) |
| `s<rpm>`, `c<mA>`, `t<ms>`, `e<0-3>`, `kp/ki/kd<v>`, `m`, `k`, `p`, `x` | Inherited from 03 for manual tuning |

`k` reads PID from motor (was `r` in 03; renamed because `r` now means
reposition).

---

## How to flash + run

```bash
cd code/08_brake1080
~/.platformio/penv/bin/pio run -t upload
~/.platformio/penv/bin/python tools/lab.py start          # background bridge
~/.platformio/penv/bin/python tools/lab.py send "g"       # one spin in place
~/.platformio/penv/bin/python tools/lab.py send "q180"    # reposition + spin
~/.platformio/penv/bin/python tools/lab.py events 30      # see what happened
```

Position the pointer with your hand before powering on so the firmware's
"start angle" matches the physical zero you want. Every spin returns to
the angle that spin started from.

---

## Hardware

- Seeed XIAO ESP32-S3 (USB-C)
- M5Stack Unit-Roller485 Lite over I²C at `0x64` (SDA=GPIO8, SCL=GPIO9)
- 15 V via USB-C PD trigger to the motor's PWR-485 input. The motor
  needs the full 15 V; at 11 V there's not enough torque to break
  static friction with the unbalanced pointer (see 06's voltage-gate
  findings).
- Direct-drive wall-mounted slightly-unbalanced pointer

---

## Why this approach works

The unbalanced pointer creates an angle-dependent gravity torque that
sometimes assists rotation and sometimes opposes it. Loose speed-mode
PID (03's recipe, the source of its "beautiful" gravity-affected swing)
amplifies that variability — the rotor accelerates downhill and stalls
uphill. Across 1080°, the angular travel becomes unpredictable and the
end angle can drift by tens of degrees.

Position-mode rate-integrated control inverts the relationship: instead
of commanding a velocity and letting the rotor's position emerge, we
command a position setpoint that advances at exactly 8 RPM and let the
motor's position PID enforce tracking. The setpoint always covers
exactly 1080° in cruise, so the end angle is deterministic. Gravity
ripple still nudges the rotor a degree or two ahead/behind the setpoint
(visible in `brakeRPM=6.7–7.9` instead of exactly 8.0), but those
deviations are bounded — and the position-PID brake at the end snaps
them out.

The trade-off: 03's pendulum-swing aesthetic is gone. The motion is
smooth in a kinematic sense (cosine eased velocity profile) but not
"alive" the way a gravity-affected free swing is. For this artwork,
deterministic stop angle won out.
