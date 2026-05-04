// PowerGate.h — voltage interlock for the motor's PWR-485 input.
// Validated 06_mix: at < 15 V the motor doesn't have enough torque
// to break static friction with the unbalanced pointer. We refuse to
// start the boot ritual until Vin clears the engage threshold, and we
// release the motor (without aborting the ritual on transient sags) if
// Vin drops below the drop threshold for several consecutive checks.
#pragma once

#include <Arduino.h>

class Calibrator;

class PowerGate {
 public:
  static constexpr float    ENGAGE_VIN_V    = 15.0f;
  static constexpr float    DROP_VIN_V      = 13.5f;
  static constexpr uint32_t VIN_CHECK_MS    = 500;
  static constexpr uint32_t VIN_PRINT_MS    = 1000;
  static constexpr int      DROP_DEBOUNCE_N = 3;

  explicit PowerGate(Calibrator& cal) : _cal(cal) {}

  void waitForPower();      // blocks until Vin >= ENGAGE_VIN_V
  void monitorPower();      // call every loop iteration

  bool powerOK() const { return _ok; }
  float lastVinV() const { return _lastVinV; }

 private:
  Calibrator& _cal;
  uint32_t    _lastCheckMS = 0;
  int         _dropStreak  = 0;
  float       _lastVinV    = 0.0f;
  bool        _ok          = false;
};
