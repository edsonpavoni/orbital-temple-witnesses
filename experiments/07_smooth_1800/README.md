# 07_smooth_1800

Forked from `07_smooth1080` on **2026-05-04**. Goal: same smooth position-mode
spin as 07, but for **1800 deg (5 full revolutions)** instead of 1080 (3).
1800 deg mod 360 deg = 0, so the pointer returns to the same physical angle.

New in this fork: the **'f' (free) command** releases motor holding torque so
the pointer falls to its natural gravity rest position. This defines X.

## Test procedure (release-then-go)

The test framing: "Start at X, finish at X. X is wherever the pointer sits
after you release the motor and wait for it to settle. Testing both smooth
motion AND return-to-start accuracy."

1. Press **`f`** - motor releases, pointer falls under gravity to rest (X).
2. Wait a few seconds for the pointer to fully settle.
3. Press **`g`** - motor re-engages, captures current encoder as start,
   runs the 1800 deg smooth profile, locks back at X.
4. Read the final report: `phys-offset-from-start` should be near 0.0 deg.

Repeat steps 1-3 multiple times to check repeatability.

## How `g` works

Four-phase state machine (identical logic to 07_smooth1080, scaled to 1800°):

1. **RAMP_UP** - `startTransition(8.0)` ramps 0 to 8 RPM with cosine ease over
   `transitionTimeMS` (default 2000 ms).
2. **CRUISE** - position mode at 8 RPM (setpoint integrates deterministically).
   Terminates when setpoint has moved >= 1752 deg (= 1800 - 48, see below).
3. **RAMP_DOWN** - `startTransition(0.0)` ramps 8 to 0 RPM eased over 2 s.
   The cosine integral adds ~48 deg, bringing total setpoint travel to 1800 deg.
4. **LOCK** - setpoint snapped to exactly start + 1800 deg. Position PID
   (kp=30M, ki=1k, kd=40M, written into flash before each spin) corrects
   the small residual. Held 5 s, then final position + error printed.

## Cruise-end threshold: why 1752 deg

The cosine ramp-down (8 RPM to 0 over 2 s, 100 Hz loop) integrates to:

```
Each tick increment = rpm * 360 / 60 / 100
Speed at tick t (of 200) = 8 * 0.5 * (1 + cos(t * pi / 200))
Total = sum over 200 ticks
      = 0.48 * 0.5 * (200 + sum_of_cos)
      = 0.48 * 0.5 * 200  (cosine sums to ~0 over half-period)
      = 48 degrees
```

So: CRUISE_END = 1800 - 48 = **1752 deg** (SPIN_CRUISE_END_DEG in code).

## Serial commands

| Cmd | Effect |
|-----|--------|
| `f` | Release motor (free-spin). Pointer falls to gravity rest. Use before `g`. |
| `g` | Smooth 1800 deg spin, lock at start angle X. |
| `s<rpm>` | Set target speed (negative = reverse) |
| `c<mA>` | Max current (100-2000 mA) |
| `t<ms>` | Transition duration (100-30000 ms) |
| `e<0-3>` | Easing curve |
| `m` | Toggle SPEED / POSITION mode |
| `kp<v>`, `ki<v>`, `kd<v>` | Write speed-PID gains |
| `r` | Read PID back from motor |
| `p` | Print all settings + live Vin / encoder pos / temp |
| `x` | Emergency stop |

## Hardware

- **MCU:** Seeed XIAO ESP32-S3
- **Motor:** M5Stack Unit-Roller485 Lite (BLDC, integrated FOC, magnetic
  encoder) - I2C at `0x64`, SDA=GPIO8, SCL=GPIO9
- **Power:** 15 V via USB-C PD trigger
- **Mechanical:** Direct drive, slightly unbalanced pointer on horizontal axis

## Thermal note

At 8 RPM, 1800 deg takes ~37.5 s cruise + ~4 s ramps = ~42 s motor-on at
1000 mA. 07_smooth1080 validated thermals at ~45 C for intermittent cycles
(23 s cruise). First runs: watch temperature readout for any upward trend
beyond the 45-50 C range. If thermals climb, reduce max current or add
longer rest between spins.

## Files

- `platformio.ini` - XIAO ESP32-S3 target (unchanged from 07_smooth1080)
- `src/main.cpp` - full firmware
- `README.md` - this file
- `FINDINGS.md` - session findings
