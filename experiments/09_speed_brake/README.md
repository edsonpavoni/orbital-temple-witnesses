# 09_speed_brake

Forked from `07_smooth_1800` on **2026-05-04**. Tests a **speed-mode cruise +
position-mode brake** hybrid for the 1800 deg (5 revolution) spin.

## The question this test answers

`07_smooth_1800` runs the entire journey in position mode. The setpoint
integrates deterministically, so gravity is partially cancelled by the position
PID the whole time. Return accuracy is high but the motion is "assisted."

`09_speed_brake` lets gravity act freely on the rotor during cruise (speed mode,
no position correction). Only the final brake switches to position mode for a
precision stop. The question: can a late position-mode brake absorb the
accumulated displacement and still land within acceptable angular error?

This is the control architecture the Orbital Witnesses production firmware will
use if the answer is yes.

## How `g` works

Four-phase state machine:

1. **SPIN_RAMP_UP** - Speed mode. `startTransition(8.0)` ramps 0 to 8 RPM with
   cosine ease over 2000 ms. Encoder displacement tracked from `g_spinStartEnc`
   (real encoder reading at `g` press, not a stale setpoint).

2. **SPIN_CRUISE** - Speed mode, holding 8 RPM. Encoder polled every tick.
   Fires brake when `(curEnc - g_spinStartEnc) >= 1797 deg`
   (= SPIN_TOTAL_DEG - BRAKE_LEAD_DEG = 1800 - 3).

3. **SPIN_BRAKE** - One-shot inline register sequence (no motorEnable toggle,
   rotor never coasts during the mode switch):
   - Write position PID (`kp=30M, ki=1k, kd=40M`) into motor flash
   - Switch mode to POSITION
   - Write brake max-current (1500 mA)
   - Write setpoint frozen at exactly `g_spinStartEnc + 1800 deg`

4. **SPIN_SETTLE** - Hold 5 s for position PID to converge, then print final
   report.

## Critical implementation detail: brake register order

Writing `REG_MODE = POSITION` before writing `REG_POS_PID` would start the
position loop with whatever PID values are currently in flash (potentially
stale or zeroed). The order is: PID first, then mode, then max-current, then
setpoint. This ensures the loop starts with correct gains on the very first
control cycle after the mode switch.

## Why no motorEnable toggle during brake

`07_smooth_1800`'s `switchToPositionMode()` helper does
`motorEnable(false)` -> write registers -> `motorEnable(true)`, adding ~200 ms
of disabled output while the rotor has momentum at 8 RPM. `09_speed_brake`
inlines the register writes with no enable toggle, so the position PID takes
over immediately with the rotor still spinning. This gives it more time to
absorb momentum before the setpoint.

## Phase markers in serial log

All markers use the `SPIN_SB` prefix (Speed-Brake), distinct from `07`'s
`SPIN` prefix, so logs from both sketches are unambiguous when compared:

```
>> SPIN_SB start enc=X.XX deg target=Y.YY deg
>> SPIN_SB cruise (encMoved=X.XX)
>> SPIN_SB brake fire (encMoved=X.XX, target=Y.YY, lead=Z.ZZ)
>> SPIN_SB lock setpoint=Y.YY
>> SPIN_SB done. final=A.AA  err=B.BB deg  phys-offset-from-start=C.CC deg
```

## Test procedure (same as 07)

1. Press **`f`** - motor releases, pointer falls under gravity to rest (X).
2. Wait for the pointer to fully settle.
3. Press **`g`** - speed-brake spin runs.
4. Read `phys-offset-from-start` in the final report.

Repeat steps 1-3 multiple times and compare repeatability against `07` runs.

## Serial commands

| Cmd | Effect |
|-----|--------|
| `f` | Release motor (free-spin). Pointer falls to gravity rest. Use before `g`. |
| `g` | Speed-mode 1800 deg spin (gravity-alive cruise), position-mode brake at target. |
| `s<rpm>` | Set target speed |
| `c<mA>` | Max current (100-2000 mA) |
| `t<ms>` | Transition duration |
| `e<0-3>` | Easing curve |
| `kp<v>`, `ki<v>`, `kd<v>` | Write speed-PID gains |
| `r` | Read PID back from motor |
| `p` | Print all settings + live Vin / encoder pos / temp |
| `x` | Emergency stop |

## Tunable constants

All in `src/main.cpp` near the top of the state machine section:

| Constant | Default | Purpose |
|----------|---------|---------|
| `BRAKE_LEAD_DEG` | 3.0 | Degrees before target at which brake fires. Tune from logs. |
| `BRAKE_MAX_MA` | 1500 | Brake phase max current (mA). From 08_brake1080. |
| `BRAKE_PID_P/I/D` | 30M/1k/40M | Position PID gains. From 04_position_lab. |
| `SETTLE_MS` | 5000 | How long to hold position before final report (ms). |

## Hardware

- **MCU:** Seeed XIAO ESP32-S3
- **Motor:** M5Stack Unit-Roller485 Lite (BLDC, integrated FOC, magnetic
  encoder) - I2C at `0x64`, SDA=GPIO8, SCL=GPIO9
- **Power:** 15 V via USB-C PD trigger
- **Mechanical:** Direct drive, slightly unbalanced pointer on horizontal axis

## Files

- `platformio.ini` - XIAO ESP32-S3 target (identical to 07_smooth_1800)
- `src/main.cpp` - full firmware
- `README.md` - this file
- `PLAN.md` - what the test is checking and what success looks like
