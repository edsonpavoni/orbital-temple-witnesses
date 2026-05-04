// MotorIO.h — header-only namespace for everything that talks to the
// M5Stack Unit-Roller485 Lite. I2C primitives, register addresses, and
// convenience readers/writers (encoder, current, vin, temp, position
// target, PID gains, output enable, mode select). Both Calibrator and
// the motion operators use these.
//
// Validated against 04_position_lab + 05_calibration. Don't change without
// re-validating against the motor's documented register map (see
// documentation/motor-roller485-lite/CLAUDE-REFERENCE.md).
#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "Recipes.h"

namespace MotorIO {

// ─── Bus configuration ──────────────────────────────────────────────────
constexpr uint8_t  I2C_ADDR    = 0x64;
constexpr int      I2C_SDA_PIN = 8;
constexpr int      I2C_SCL_PIN = 9;
constexpr uint32_t I2C_FREQ    = 100000;

// ─── Register map ───────────────────────────────────────────────────────
constexpr uint8_t REG_OUTPUT       = 0x00;
constexpr uint8_t REG_MODE         = 0x01;
constexpr uint8_t REG_STALL_PROT   = 0x0F;
constexpr uint8_t REG_POS_MAXCUR   = 0x20;
constexpr uint8_t REG_VIN          = 0x34;
constexpr uint8_t REG_TEMP         = 0x38;
constexpr uint8_t REG_SPEED        = 0x40;
constexpr uint8_t REG_SPEED_MAXCUR = 0x50;
constexpr uint8_t REG_SPEED_READ   = 0x60;
constexpr uint8_t REG_SPEED_PID    = 0x70;
constexpr uint8_t REG_POS          = 0x80;
constexpr uint8_t REG_POS_READ     = 0x90;
constexpr uint8_t REG_POS_PID      = 0xA0;
constexpr uint8_t REG_CURRENT_READ = 0xC0;

constexpr uint8_t MODE_SPEED       = 1;
constexpr uint8_t MODE_POSITION    = 2;

constexpr int COUNTS_PER_DEG = 100;     // 36 000 counts per 360°

// ─── I2C primitives ─────────────────────────────────────────────────────

inline void writeReg8(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

inline void writeReg32(uint8_t reg, int32_t value) {
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(reg);
  Wire.write(static_cast<uint8_t>( value        & 0xFF));
  Wire.write(static_cast<uint8_t>((value >>  8) & 0xFF));
  Wire.write(static_cast<uint8_t>((value >> 16) & 0xFF));
  Wire.write(static_cast<uint8_t>((value >> 24) & 0xFF));
  Wire.endTransmission();
}

inline int32_t readReg32(uint8_t reg) {
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(static_cast<uint8_t>(I2C_ADDR), static_cast<uint8_t>(4));
  int32_t v = 0;
  if (Wire.available() >= 4) {
    v  = Wire.read();
    v |= static_cast<int32_t>(Wire.read()) <<  8;
    v |= static_cast<int32_t>(Wire.read()) << 16;
    v |= static_cast<int32_t>(Wire.read()) << 24;
  }
  return v;
}

inline void writeReg96(uint8_t reg, int32_t v1, int32_t v2, int32_t v3) {
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(reg);
  auto put = [](int32_t v) {
    Wire.write(static_cast<uint8_t>( v        & 0xFF));
    Wire.write(static_cast<uint8_t>((v >>  8) & 0xFF));
    Wire.write(static_cast<uint8_t>((v >> 16) & 0xFF));
    Wire.write(static_cast<uint8_t>((v >> 24) & 0xFF));
  };
  put(v1); put(v2); put(v3);
  Wire.endTransmission();
}

// ─── Convenience readers ────────────────────────────────────────────────
inline int32_t encoderCounts()    { return readReg32(REG_POS_READ);              }
inline float   encoderRawDeg()    { return readReg32(REG_POS_READ) / 100.0f;     }
inline float   actualRPM()        { return readReg32(REG_SPEED_READ) / 100.0f;   }
inline float   currentMA()        { return readReg32(REG_CURRENT_READ) / 100.0f; }
inline float   vinV()             { return readReg32(REG_VIN) / 100.0f;          }
inline int32_t tempC()            { return readReg32(REG_TEMP);                  }

// ─── Convenience writers ────────────────────────────────────────────────
inline void setOutput(bool on) { writeReg8(REG_OUTPUT, on ? 1 : 0); }
inline void setMode(uint8_t mode) { writeReg8(REG_MODE, mode); }
inline void setStallProtection(bool on) { writeReg8(REG_STALL_PROT, on ? 1 : 0); }
inline void setPosTarget(int32_t enc) { writeReg32(REG_POS, enc); }
inline void setPosMaxCurrentMA(int32_t mA) { writeReg32(REG_POS_MAXCUR, mA * 100); }
inline void setPosPID(int32_t kp, int32_t ki, int32_t kd) {
  writeReg96(REG_POS_PID, kp, ki, kd);
}

// Speed-mode primitives — used by ChoreoSpin to replicate 07_smooth1080's
// loose-PID speed-mode rotation (gravity-affected swing aesthetic).
inline void setSpeedTarget(float rpm) { writeReg32(REG_SPEED, static_cast<int32_t>(rpm * 100.0f)); }
inline void setSpeedMaxCurrentMA(int32_t mA) { writeReg32(REG_SPEED_MAXCUR, mA * 100); }
inline void setSpeedPID(int32_t kp, int32_t ki, int32_t kd) {
  writeReg96(REG_SPEED_PID, kp, ki, kd);
}

// ─── PID switching (jolt-safe pattern) ──────────────────────────────────
// When the motor is energised, swapping PID gains directly carries the
// internal integrator through the new (often much larger) gains and
// produces a massive instantaneous output, oscillation, and supply droop.
// Sequence:
//   1. Disable output (this resets the motor's internal integrator).
//   2. Pin the position target to the current encoder reading, so when we
//      re-enable, the motor sees zero error.
//   3. Write new PID + current cap.
//   4. Re-enable.
// Caller is responsible for tracking which PID is currently active and
// for updating any user-frame target after the swap (the encoder is
// physically unchanged but the integrator was reset).

inline void applyProductionPID() {
  // Boot-time helper: motor output is already off.
  setPosPID(Recipes::PROD_KP, Recipes::PROD_KI, Recipes::PROD_KD);
  setPosMaxCurrentMA(Recipes::PROD_MC_MA);
  delay(20);
}

inline int32_t safeSwitchToDiagPID() {
  // Returns the encoder counts at the moment the PID was swapped — caller
  // can use this to recompute its user-frame target.
  setOutput(false);
  delay(20);
  int32_t cur = encoderCounts();
  setPosTarget(cur);
  delay(10);
  setPosPID(Recipes::DIAG_KP, Recipes::DIAG_KI, Recipes::DIAG_KD);
  setPosMaxCurrentMA(Recipes::DIAG_MC_MA);
  delay(20);
  setOutput(true);
  return cur;
}

inline int32_t safeSwitchToProductionPID() {
  setOutput(false);
  delay(20);
  int32_t cur = encoderCounts();
  setPosTarget(cur);
  delay(10);
  setPosPID(Recipes::PROD_KP, Recipes::PROD_KI, Recipes::PROD_KD);
  setPosMaxCurrentMA(Recipes::PROD_MC_MA);
  delay(20);
  setOutput(true);
  return cur;
}

// ─── Boot-time motor init ───────────────────────────────────────────────
inline bool begin() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  // Long settle: motor's I2C interface can take >1 s to come up after the
  // 15 V rail rises, especially when the XIAO and the motor power on at
  // the same instant. 100 ms wasn't enough — observed false-negative halts
  // on cold boot 2026-05-03.
  delay(1500);
  Wire.beginTransmission(I2C_ADDR);
  if (Wire.endTransmission() != 0) return false;
  setOutput(false);
  delay(20);
  setStallProtection(false);          // disable jam protection (false alarms during slow diag)
  delay(20);
  setPosMaxCurrentMA(Recipes::PROD_MC_MA);
  delay(20);
  return true;
}

// ─── Cosine S-curve ease ────────────────────────────────────────────────
inline float easeInOut(float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  return 0.5f * (1.0f - cosf(t * PI));
}

}  // namespace MotorIO
