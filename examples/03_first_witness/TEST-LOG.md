# First Witness - Motor Smoothness Test Log

**Hardware:**
- Seeed XIAO ESP32-S3
- M5Stack RollerCAN BLDC (I2C, 0x64)
- Direct drive (36000 steps = 360°)
- Power: 15V USB-C PD trigger board

**Goal:** Achieve smooth 360° revolution with elegant stop

---

## Baseline Findings

- **5V power:** Jerky, insufficient torque
- **15V power:** Better, but still not smooth enough
- **Serial CDC:** Blocks standalone operation - must remove Serial calls
- **Position mode:** Unreliable on this hardware - use SPEED MODE only

---

## Batch 1: Deceleration Zone (degrees)

**Fixed parameters:**
- Revolution duration: 3000ms
- Accel time: 600ms
- I2C freq: 400kHz
- Update rate: 500Hz
- Current limit: 2A
- Easing: quint ease-out

| Test | Decel Zone | Result | Notes |
|------|------------|--------|-------|
| 1 | 90° | - | |
| 2 | 120° | - | |
| 3 | 150° | - | |
| 4 | 180° | **BEST** | Smoothest stop |
| 5 | 210° | - | |

**Best from Batch 1:** 180° decel zone

---

## Batch 2: Acceleration Time (ms)

**Fixed parameters:**
- Decel zone: 180° (winner from batch 1)
- Revolution duration: 3000ms
- I2C freq: 400kHz
- Update rate: 500Hz
- Current limit: 2A

| Test | Accel Time | Result | Notes |
|------|------------|--------|-------|
| 1 | 400ms | ~ | Not perceptible |
| 2 | 600ms | ~ | Not perceptible |
| 3 | 800ms | ~ | Not perceptible |
| 4 | 1000ms | ~ | Not perceptible |
| 5 | 1200ms | ~ | Not perceptible |

**Best from Batch 2:** No difference - keep 600ms (default)

---

## Batch 3: Deceleration Easing Curve

**SKIPPED** - Code issues with function pointers. Keep quint (t^5).

---

## Batch 4: Update Rate (Hz)

**SKIPPED** - Variable loop delay broke timing. Keep 500Hz.

---

## Batch 5: Current Limit (Amps)

**Fixed parameters:**
- Decel zone: 180°
- Update rate: 500Hz

| Test | Current | Result | Notes |
|------|---------|--------|-------|
| 1 | 1.0A | | Less torque |
| 2 | 1.5A | | |
| 3 | 2.0A | | Current |
| 4 | 2.5A | | More torque |
| 5 | 3.0A | | Max torque |

**Best from Batch 5:** _TBD_

---

## Final Configuration

_To be determined after testing_

```cpp
#define DECEL_ZONE_DEG    ???
#define ACCEL_TIME_MS     ???
#define UPDATE_RATE_HZ    ???
#define CURRENT_LIMIT     ???
```

---

## Conclusions

_To be written after all tests complete_
