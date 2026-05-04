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
#define ACCEL_TIME_MS        500    // 0.5s accel (time-based)
#define DECEL_ZONE_DEG       90     // 90° decel zone (position-based) - reduced from 180
#define CURRENT_LIMIT        200000 // 2A
#define UPDATE_RATE_HZ       500    // 500Hz loop
#define I2C_FREQ             400000 // 400kHz
```

**Easing:** smootherstep for both accel and decel (balanced S-curve)

---

## Next Steps

1. Implement hybrid time+position approach
2. Re-run update rate test (Batch 4)
3. Run current limit test (Batch 5)
4. Fine-tune based on results

---

*Last updated: 2026-01-24*
