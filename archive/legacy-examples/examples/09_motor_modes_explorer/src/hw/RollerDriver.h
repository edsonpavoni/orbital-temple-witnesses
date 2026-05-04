// Witness foundation · RollerDriver
// Thin wrapper over UnitRollerI2C. Owns bounded-timeout I/O, online state,
// and the non-blocking I2C health/recovery state machine. Every public method
// guards on online_ so a missing/dead motor never wedges callers.

#pragma once

#include <stdint.h>
#include <Wire.h>
// Pulled in so callers keep the ROLLER_MODE_* enum names available.
#include "unit_rolleri2c.hpp"

class RollerDriver {
 public:
  // Bounded handshake. Returns true if roller responded within retries.
  // Stores wire/addr for later recovery service.
  bool begin(TwoWire& wire, uint8_t addr_7bit, int sda_pin, int scl_pin, uint32_t freq_hz);

  bool isOnline() const { return online_; }

  // Hot-path. All return zero / no-op when offline.
  int32_t getEncoder();
  void    setMode(uint8_t mode);            // raw byte; covers mode 5 cast
  uint8_t getMotorMode();
  void    setOutput(bool on);
  uint8_t getOutputStatus();
  void    setStallProtection(bool on);
  void    setPosTarget(int32_t pos_counts);
  void    setSpeedTarget(int32_t speed_counts);
  void    setCurrentTarget(int32_t current_counts);   // 0.01 mA per count
  void    setPosMaxCurrent(int32_t mA);
  void    setPosPID(uint32_t p, uint32_t i, uint32_t d);
  void    getPosPID(uint32_t& p, uint32_t& i, uint32_t& d);

  // Slow-path / telemetry.
  int32_t getVinRaw();             // 0.01 V per count
  int32_t getTempC();               // C
  int32_t getCurrentReadbackRaw();  // 0.01 mA per count
  uint8_t getFirmwareVersion();

  // Health: probe + non-blocking recovery. Call from loop().
  void serviceHealth();

 private:
  // Bare ACK probe: 0 == ack received.
  bool ackProbe_();
  // Internal handshake helper used by both begin() and recovery.
  bool handshake_();

  UnitRollerI2C roller_;
  TwoWire*      wire_       = nullptr;
  uint8_t       addr_       = 0;
  int           sda_pin_    = -1;
  int           scl_pin_    = -1;
  uint32_t      freq_hz_    = 0;
  bool          online_     = false;

  // Health state.
  int       consecutive_fails_ = 0;
  uint32_t  last_probe_ms_     = 0;
  uint32_t  next_recovery_ms_  = 0;
};
