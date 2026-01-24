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
| 1 | 400ms | | Snappy |
| 2 | 600ms | | Current |
| 3 | 800ms | | Smoother |
| 4 | 1000ms | | Very smooth |
| 5 | 1200ms | | Gradual |

**Best from Batch 2:** _TBD_

---

## Batch 3: _TBD_

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
