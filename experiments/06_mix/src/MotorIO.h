// MotorIO.h — small header-only helper for the M5Stack Roller485 Lite.
// Both PreciseOperator and SmoothOperator use these primitives directly so
// neither has to duplicate the I²C plumbing. main.cpp also uses them for
// the one-time Wire.begin() and any direct register reads.
#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace MotorIO {

// ─── Registers (Roller485 I²C protocol) ───────────────────────────────
constexpr uint8_t I2C_ADDR         = 0x64;
constexpr int     I2C_SDA_PIN      = 8;
constexpr int     I2C_SCL_PIN      = 9;
constexpr uint32_t I2C_FREQ        = 100000;

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

constexpr uint8_t MODE_SPEED    = 1;
constexpr uint8_t MODE_POSITION = 2;

// Both classes use 100 counts/deg (36000 counts per revolution).
constexpr int COUNTS_PER_DEG = 100;

// ─── I²C primitives ───────────────────────────────────────────────────

inline void writeReg8(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

inline void writeReg32(uint8_t reg, int32_t value) {
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(reg);
  Wire.write(static_cast<uint8_t>(value & 0xFF));
  Wire.write(static_cast<uint8_t>((value >> 8) & 0xFF));
  Wire.write(static_cast<uint8_t>((value >> 16) & 0xFF));
  Wire.write(static_cast<uint8_t>((value >> 24) & 0xFF));
  Wire.endTransmission();
}

inline int32_t readReg32(uint8_t reg) {
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(I2C_ADDR, static_cast<uint8_t>(4));
  int32_t v = 0;
  if (Wire.available() >= 4) {
    v  = Wire.read();
    v |= static_cast<int32_t>(Wire.read()) << 8;
    v |= static_cast<int32_t>(Wire.read()) << 16;
    v |= static_cast<int32_t>(Wire.read()) << 24;
  }
  return v;
}

inline void writeReg96(uint8_t reg, int32_t v1, int32_t v2, int32_t v3) {
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(reg);
  auto put = [](int32_t v) {
    Wire.write(static_cast<uint8_t>(v & 0xFF));
    Wire.write(static_cast<uint8_t>((v >> 8) & 0xFF));
    Wire.write(static_cast<uint8_t>((v >> 16) & 0xFF));
    Wire.write(static_cast<uint8_t>((v >> 24) & 0xFF));
  };
  put(v1); put(v2); put(v3);
  Wire.endTransmission();
}

// ─── Convenience readers ──────────────────────────────────────────────

inline int32_t encoderRaw()   { return readReg32(REG_POS_READ); }
inline float   actualRPM()    { return readReg32(REG_SPEED_READ) / 100.0f; }
inline float   currentMA()    { return readReg32(REG_CURRENT_READ) / 100.0f; }
inline float   vinV()         { return readReg32(REG_VIN) / 100.0f; }
inline int32_t tempC()        { return readReg32(REG_TEMP); }

// Cosine S-curve. t in [0, 1] -> output in [0, 1] with zero slope at endpoints.
inline float easeInOut(float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  return 0.5f * (1.0f - cosf(t * PI));
}

}  // namespace MotorIO
