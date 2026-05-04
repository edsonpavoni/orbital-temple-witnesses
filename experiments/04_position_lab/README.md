# 04_position_lab

Position-control bench rig for finding the right recipe to **move the
pointer to a specific angle and stop there**. Companion to
`03_motion_test`, which is the smooth-rotation lab. Different control
regime: closed-loop position mode with a shaped trajectory.

Created **2026-04-28** as the next step after the smooth-motion recipe was
locked in. The smooth recipe deliberately does *not* track speed precisely —
beautiful for free rotation, useless for stopping at a target. This file is
the experimental ground for the precise-stop regime needed for the satellite
choreography.

---

## Workflow

The motor boots **disabled** so you can hand-position the pointer freely.

1. Move the pointer with your hand to the visual zero. Press `0` + Enter.
   The firmware records the current encoder reading as the software zero
   offset and engages position-mode hold at that angle. You should now feel
   the motor resist if you try to push the pointer.
2. Press `r` + Enter to start the **ritual**. The pointer steps through:
   `step°`, `2·step°`, `3·step°`, ... holding `holdMS` at each. Targets keep
   climbing past 360° (encoder is cumulative — visually identical to wrap).
3. Tune live while the ritual runs: change PID, current, easing, durations.
4. Press `x` + Enter to release. Output disables, encoder zero is cleared.
   Press `0` again to re-engage.

Pressing `r` before `0` prints an error and does nothing.

---

## Trajectory model

Each step interpolates the **position setpoint** every 10 ms over `mt` ms:

```
sp(t) = startDeg + (endDeg - startDeg) * easing(t / mt)
```

The motor's internal position PID tracks this slowly-moving target. With
**easing OFF** (`eoff`), the final target is written instantly at move start
and the motor's PID is the only loop closing the gap — useful for observing
raw step response.

Easing curves available:
| Mode | Formula | Feel |
|------|---------|------|
| 0 LINEAR | `t` | Constant rate |
| 1 EASE_IN_OUT | `0.5·(1−cos(t·π))` | Cosine S-curve, default |
| 2 EASE_IN | `t²` | Slow start, fast end |
| 3 EASE_OUT | `1−(1−t)²` | Fast start, slow end |

---

## Serial commands

| Command | Effect | Default |
|---------|--------|---------|
| `0` | Set zero at current encoder, engage hold | — |
| `r` | Start / arm ritual | — |
| `x` | Release motor, clear zero | — |
| `step<deg>` | Ritual step size | 45 |
| `hold<ms>` | Hold time at each ritual position | 2000 |
| `mt<ms>` | Move (trajectory) duration | 1500 |
| `e<0–3>` | Easing curve | 1 |
| `eon` / `eoff` | Easing on / off | on |
| `c<mA>` | Position-mode max current | 600 |
| `kp<v>`, `ki<v>`, `kd<v>` | Position PID (writes register `0xA0`) | read from motor flash |
| `pidread` | Re-read position PID from motor | — |
| `goto<deg>` | One-shot move to absolute angle (no auto-advance) | — |
| `p` | Print full settings + live state | — |

PID and current defaults match the values pointer_v3 settled on for the same
physical pointer (P=15 000 000, I=1 000, D=40 000 000, 600 mA). Expect to
retune; the wall-mounted sculpture is a different mechanical context.

---

## Compressed delta log

Every 200 ms, one line of `key=value` pairs. **Only fields that changed
since the last line are emitted.** Silence == "still the same."

| Key | Meaning |
|-----|---------|
| `t` | millis() — always present (anchors the line) |
| `st` | State: `R` released / `H` holding / `M` moving |
| `ra` | Ritual active: `0` / `1` |
| `Tg` | Logical target deg (user-frame, what we're heading toward) |
| `sp` | Trajectory setpoint deg right now (equals Tg during HOLD) |
| `p` | Encoder readback in user-frame deg (raw − zeroOffset) ÷ 100 |
| `er` | Position error: `sp − p` |
| `a` | Actual RPM read from speed-readback register |
| `tr` | Move trajectory % (0–100, `-` when idle) |
| `tmp` | Motor temp °C (printed only while output is enabled) |
| `v` | Vin (V) |
| `mc` | Position max current (mA) |
| `e`, `eon` | Easing curve and on/off |
| `mt`, `hold`, `step` | Tunables (print only on change) |
| `kP`, `kI`, `kD` | Position PID |

Skipped lines = nothing changed. The first line after boot dumps everything
as the baseline.

### Example

```
t=2000,st=R,ra=0,Tg=0.00,sp=0.00,p=0.00,er=0.00,a=0.0,tr=-,tmp=44,v=15.09,mc=600,e=1,eon=1,mt=1500,hold=2000,step=45.00,kP=15000000,kI=1000,kD=40000000
t=4500,st=H,p=0.05,er=-0.05
t=8200,st=M,Tg=45.00,sp=2.31,er=2.26,a=4.5,tr=8
t=8400,sp=5.42,p=4.81,er=0.61,a=8.2,tr=18
... (trajectory progressing)
t=9700,sp=45.00,p=44.91,er=0.09,a=0.4,tr=-,st=H
t=11700,st=M,Tg=90.00,...
```

---

## What to look for during tuning

Each step is a controlled experiment: fixed amplitude (`step`), fixed
profile (`mt`, `e`), fixed PID. Look at the log for:

- **Tracking error during the move** — `er` magnitude while `tr` climbs.
  Large `er` = motor lagging the setpoint = need more P or shorter `mt`
  (faster trajectory still under-driven) or higher `mc`.
- **Overshoot at the end** — does `p` go past `Tg` once `tr=-`? Visible as
  `er` flipping sign briefly. More D damps this; lower P also helps.
- **Settling time** — how many log lines after `tr=-` until `er` stays under
  some tolerance (e.g. ±0.2°)? This is the key metric for "stop at the
  right angle."
- **Steady-state offset during HOLD** — does `er` settle to a consistent
  non-zero value? That's gravity bias the integral term should absorb. Try
  raising `kI`. (Pointer_v3 keeps I=1000 because gravity is symmetrical
  there. The wall sculpture may differ.)
- **Direction asymmetry** — gravity-helping moves vs gravity-fighting moves.
  If the ritual passes the pointer's gravity rest twice per revolution, you
  should see the same step profile produce different `er`/`a` traces
  depending on phase.

### Easing OFF mode (raw step response)

Set `eoff`, pick a `step` like 90, run the ritual. The setpoint jumps
instantly to the new target and the motor's PID alone closes the loop.
You'll see big initial `er`, possibly an overshoot, and a settling tail.
This is the classic step-response test for the position controller. Good for
seeing PID character before adding trajectory shaping.

---

## Out of scope (deliberately)

- No flash persistence (`SAVE_FLASH`) — settings are session-only. If you
  find a recipe you love, write the values down.
- No speed-mode kick + position-mode trim hybrid (pointer_v3 has it; this
  lab is the simpler position-only baseline first).
- No automated metric capture. The log is rich enough to extract
  overshoot/settling-time post-hoc; an automated rig can be a follow-up.
- CW only. No CCW or alternating direction yet.

---

## Files

- `platformio.ini` — XIAO ESP32-S3 target (same as `03_motion_test`)
- `src/main.cpp` — full firmware
- `README.md` — this file

## Hardware

Same as `03_motion_test`:
- Seeed XIAO ESP32-S3
- M5Stack Unit-Roller485 Lite (BLDC, integrated FOC)
- I²C at `0x64` on `SDA=GPIO8`, `SCL=GPIO9`
- 15 V via USB-C PD trigger to the motor's PWR-485 input
- Wall-mounted, slightly-unbalanced pointer, direct drive
