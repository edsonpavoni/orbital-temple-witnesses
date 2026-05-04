# First Witness - Motor Smoothness Test Log

**Goal:** Optimize motor for smooth AND precise 360° revolutions

**Hardware:**
- Seeed XIAO ESP32-S3
- M5Stack RollerCAN BLDC (I2C address 0x64)
- Direct drive (36000 steps = 360°)
- Power: 15V USB-C PD trigger board

---

## Key Findings

### What Works
- **SPEED MODE only** - Position mode is unreliable on this hardware
- **15V power** - Much better than 5V (more torque, less cogging)
- **No Serial** - Serial CDC blocks without USB host, must remove for standalone
- **Position tracking** - Read position register during speed mode for precise stops

### What Doesn't Work
- **Position mode** - Motor doesn't respond, LED blinks blue (error)
- **Pure time-based control** - Can't guarantee exact 360° rotation
- **Very large decel zones (180°) with quint easing** - Speed drops too fast, motor crawls

### Critical Bug Found
With position-based decel using 180° zone + quint easing (t^5):
- At 90° remaining: speed = 0.5^5 = 3% of cruise
- Motor essentially stalls with huge distance still to go
- **Solution:** Use time-based decel OR smaller decel zone OR different easing

---

## Batch 1: Deceleration Zone (degrees)

**Method:** Position-based decel trigger

| Test | Decel Zone | Result | Notes |
|------|------------|--------|-------|
| 1 | 90° | - | |
| 2 | 120° | - | |
| 3 | 150° | - | |
| 4 | 180° | **BEST** | Smoothest stop feel |
| 5 | 210° | - | |

**Winner:** 180° felt smoothest
**Problem discovered later:** 180° + quint = motor takes forever (see bug above)

---

## Batch 2: Acceleration Time (ms)

| Test | Accel Time | Result | Notes |
|------|------------|--------|-------|
| 1 | 400ms | ~ | Not perceptible |
| 2 | 600ms | ~ | Not perceptible |
| 3 | 800ms | ~ | Not perceptible |
| 4 | 1000ms | ~ | Not perceptible |
| 5 | 1200ms | ~ | Not perceptible |

**Winner:** No difference - keep 600ms default

---

## Batch 3: Easing Curves

**SKIPPED** - Code had issues with function pointers. Keep quint (t^5).

---

## Batch 4: Update Rate (Hz)

**Method:** Time-based revolution (to isolate update rate effect)

| Test | Rate | Result | Notes |
|------|------|--------|-------|
| 1 | 100Hz | | |
| 2 | 250Hz | | |
| 3 | 500Hz | | |
| 4 | 750Hz | | |
| 5 | 1000Hz | | |

**Status:** Test framework working, but time-based approach doesn't guarantee exact 360°.
Need hybrid approach: time-based accel/cruise + position-based decel.

---

## Batch 5: Current Limit (Amps)

**Not yet tested**

| Test | Current | Result | Notes |
|------|---------|--------|-------|
| 1 | 1.0A | | |
| 2 | 1.5A | | |
| 3 | 2.0A | | Current default |
| 4 | 2.5A | | |
| 5 | 3.0A | | |

---

## Remaining Tests

| # | Parameter | Status |
|---|-----------|--------|
| 6 | Revolution duration (3s, 4s, 5s) | Not tested |
| 7 | I2C speed (100kHz vs 400kHz) | Not tested |
| 8 | Fine-tune decel zone around winner | Not tested |
| 9 | Motor internal PID tuning | Not tested |

---

## Architecture Decision

**Hybrid approach needed:**
1. **Accel phase:** Time-based (0.5s) with smootherstep easing
2. **Cruise phase:** Time-based until position trigger
3. **Decel phase:** Position-based with appropriate zone size and easing

This gives:
- Consistent timing feel
- Precise 360° stops
- Smooth motion throughout

---

## Current Best Parameters

```cpp
#define REVOLUTION_DURATION  3000   // 3 seconds
#define CURRENT_LIMIT        200000 // 2A
#define UPDATE_RATE_HZ       100    // 100Hz loop (stable)
#define I2C_FREQ             400000 // 400kHz
```

**Motion Profile:** Sine-squared velocity curve `v(t) = v_avg * (1 - cos(2πt))`
**Drift Correction:** Measure error after each revolution, adjust next revolution distance

---

## Final Solution (2026-01-24)

### Sine Curve + Drift Correction

The winning approach:
1. **Pure time-based sine curve** for smooth motion
2. **Drift correction** between revolutions for precision
3. **100Hz update rate** for stable I2C communication

```cpp
// Sine curve velocity profile
float t = (float)elapsed / REVOLUTION_DURATION;
float v_avg = (float)STEPS_PER_REV / (REVOLUTION_DURATION / 1000.0f);
float v_motor = v_avg * (1.0f - cos(TWO_PI * t));
setSpeed((int32_t)v_motor);

// After each revolution, measure error and adjust next
int32_t error = actualPos - targetPos;
nextDistance = STEPS_PER_REV - error;
```

### Encoder Calibration

Ran M5Unit-Roller encoder calibration - stored to motor flash. One-time procedure.

### Mode Comparison Test (2026-01-24 evening)

Tested different approaches using M5UnitRoller library:

| Mode | Description | Result |
|------|-------------|--------|
| 1 | Speed mode + sine curve (2A) | Baseline |
| 2 | Current mode + sine curve | No difference |
| 3 | Speed mode + low current (0.5A) | No difference |
| 4 | Speed mode + reduced PID P (50%) | No difference |
| 5 | Position mode (motor internal) | No difference |

**Conclusion:** No perceivable difference between modes. Stick with Speed mode + sine curve.

---

## Standalone Operation

For standalone (no USB):
- `ARDUINO_USB_CDC_ON_BOOT=0` in platformio.ini
- Add boot delay (1 sec) for power stabilization
- Add I2C retry logic (motor may boot slower)
- Startup wiggle to confirm working

---

*Last updated: 2026-01-24 21:25*
