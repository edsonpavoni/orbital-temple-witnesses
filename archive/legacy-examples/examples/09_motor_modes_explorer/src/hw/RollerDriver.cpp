// Witness foundation · RollerDriver implementation
// Behavior preserved verbatim from the prior monolithic main.cpp.

#include "RollerDriver.h"

#include <Arduino.h>

#include "../foundation/Watchdog.h"

namespace {
// 10 s WDT covers worst-case bounded-handshake (5 retries * ~250 ms ~= 1.3 s).
// These constants are local to the foundation; main owns the user-visible knobs.
constexpr uint16_t WIRE_TIMEOUT_MS         = 50;
constexpr int      HANDSHAKE_RETRIES       = 5;
constexpr uint32_t HANDSHAKE_RETRY_GAP_MS  = 200;
constexpr int      I2C_FAIL_THRESHOLD      = 5;
constexpr uint32_t I2C_RECOVERY_PERIOD_MS  = 2000;
constexpr uint32_t I2C_PROBE_PERIOD_MS     = 100;
}  // namespace

bool RollerDriver::ackProbe_() {
  if (!wire_) return false;
  wire_->beginTransmission(addr_);
  return wire_->endTransmission() == 0;
}

bool RollerDriver::handshake_() {
  // First attempt uses library begin(); retries use bare Wire ACK probe to
  // avoid reconfiguring pins repeatedly.
  if (roller_.begin(wire_, addr_, sda_pin_, scl_pin_, freq_hz_)) {
    online_ = true;
    return true;
  }
  for (int i = 1; i < HANDSHAKE_RETRIES; i++) {
    delay(HANDSHAKE_RETRY_GAP_MS);
    if (ackProbe_()) { online_ = true; return true; }
  }
  online_ = false;
  return false;
}

bool RollerDriver::begin(TwoWire& wire, uint8_t addr_7bit, int sda_pin, int scl_pin, uint32_t freq_hz) {
  wire_    = &wire;
  addr_    = addr_7bit;
  sda_pin_ = sda_pin;
  scl_pin_ = scl_pin;
  freq_hz_ = freq_hz;
  return handshake_();
}

int32_t RollerDriver::getEncoder() {
  // hpp:714 - getDialCounter returns encoder counts.
  return online_ ? roller_.getDialCounter() : 0;
}

void RollerDriver::setMode(uint8_t mode) {
  // hpp:56 - setMode(roller_mode_t). Mode 5 (POS_SPEED cascade) not in enum
  // but firmware accepts it; cast through the parameter.
  if (online_) roller_.setMode((roller_mode_t)mode);
}

uint8_t RollerDriver::getMotorMode() {
  // hpp:887 - getMotorMode returns the active mode byte.
  return online_ ? roller_.getMotorMode() : 0;
}

void RollerDriver::setOutput(bool on) {
  // hpp:74 - setOutput(uint8_t en). 1=on, 0=off.
  if (online_) roller_.setOutput(on ? 1 : 0);
}

uint8_t RollerDriver::getOutputStatus() {
  // hpp:866 - getOutputStatus, used by wiggle save/restore.
  return online_ ? roller_.getOutputStatus() : 0;
}

void RollerDriver::setStallProtection(bool on) {
  // hpp:431 - setStallProtection(uint8_t en).
  if (online_) roller_.setStallProtection(on ? 1 : 0);
}

void RollerDriver::setPosTarget(int32_t pos_counts) {
  // hpp:158 - setPos(int32_t). Raw counts.
  if (online_) roller_.setPos(pos_counts);
}

void RollerDriver::setSpeedTarget(int32_t speed_counts) {
  // hpp:92 - setSpeed(int32_t). Raw counts (0.01 RPM/count).
  if (online_) roller_.setSpeed(speed_counts);
}

void RollerDriver::setCurrentTarget(int32_t current_counts) {
  // hpp:224 - setCurrent(int32_t). Raw counts (0.01 mA/count).
  if (online_) roller_.setCurrent(current_counts);
}

void RollerDriver::setPosMaxCurrent(int32_t mA) {
  // hpp:176 - setPosMaxCurrent(int32_t). Raw mA, no scaling.
  if (online_) roller_.setPosMaxCurrent(mA);
}

void RollerDriver::setPosPID(uint32_t p, uint32_t i, uint32_t d) {
  // hpp:206 - setPosPID(uint32_t, uint32_t, uint32_t).
  if (online_) roller_.setPosPID(p, i, d);
}

void RollerDriver::getPosPID(uint32_t& p, uint32_t& i, uint32_t& d) {
  // hpp:527 - getPosPID(uint32_t*, uint32_t*, uint32_t*).
  p = i = d = 0;
  if (online_) roller_.getPosPID(&p, &i, &d);
}

int32_t RollerDriver::getVinRaw() {
  // hpp:733 - getVin(). 0.01 V/count.
  return online_ ? roller_.getVin() : 0;
}

int32_t RollerDriver::getTempC() {
  // hpp:752 - getTemp(). Degrees Celsius (signed integer).
  return online_ ? roller_.getTemp() : 0;
}

int32_t RollerDriver::getCurrentReadbackRaw() {
  // hpp:695 - getCurrentReadback(). 0.01 mA/count.
  return online_ ? roller_.getCurrentReadback() : 0;
}

uint8_t RollerDriver::getFirmwareVersion() {
  // hpp:981 - getFirmwareVersion().
  return online_ ? roller_.getFirmwareVersion() : 0;
}

void RollerDriver::serviceHealth() {
  uint32_t now = millis();

  // Light ACK probe at fixed cadence; trips offline at threshold.
  if (online_) {
    if (now - last_probe_ms_ >= I2C_PROBE_PERIOD_MS) {
      last_probe_ms_ = now;
      if (ackProbe_()) {
        consecutive_fails_ = 0;
      } else {
        consecutive_fails_++;
        if (consecutive_fails_ >= I2C_FAIL_THRESHOLD) {
          Serial.println("\nI2C FAULT: roller stopped acking. Driver OFFLINE.");
          online_ = false;
          next_recovery_ms_ = now + I2C_RECOVERY_PERIOD_MS;
        }
      }
    }
    return;
  }

  // Offline: try one Wire reset + handshake every recovery period.
  if (now < next_recovery_ms_) return;
  next_recovery_ms_ = now + I2C_RECOVERY_PERIOD_MS;
  // Non-blocking reset: end, brief delay, reopen, re-apply timeout, pet WDT,
  // attempt bounded handshake. Returns either way.
  wire_->end();
  delay(100);
  wire_->begin(sda_pin_, scl_pin_, freq_hz_);
  wire_->setTimeOut(WIRE_TIMEOUT_MS);
  Watchdog::pet();
  if (handshake_()) {
    // Library begin() reconfigures Wire and resets timeout - re-apply ours.
    wire_->setTimeOut(WIRE_TIMEOUT_MS);
    Serial.println("\nI2C RECOVERED: driver back online.");
    consecutive_fails_ = 0;
  }
}
