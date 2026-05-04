#include "PowerGate.h"

#include "Calibrator.h"
#include "MotorIO.h"

void PowerGate::waitForPower() {
  Serial.print(">> Boot: waiting for Vin >= ");
  Serial.print(ENGAGE_VIN_V, 1);
  Serial.println(" V");
  uint32_t lastPrint = 0;
  while (true) {
    _lastVinV = MotorIO::vinV();
    if (_lastVinV >= ENGAGE_VIN_V) break;
    if (millis() - lastPrint >= VIN_PRINT_MS) {
      lastPrint = millis();
      Serial.print(">> Waiting: Vin=");
      Serial.print(_lastVinV, 2);
      Serial.print(" V (need >= ");
      Serial.print(ENGAGE_VIN_V, 1);
      Serial.println(" V)");
    }
    delay(50);
  }
  Serial.print(">> Power OK (Vin=");
  Serial.print(_lastVinV, 2);
  Serial.println(" V)");
  _ok          = true;
  _dropStreak  = 0;
}

void PowerGate::monitorPower() {
  if (millis() - _lastCheckMS < VIN_CHECK_MS) return;
  _lastCheckMS = millis();
  _lastVinV = MotorIO::vinV();

  if (!_ok) return;

  if (_lastVinV < DROP_VIN_V) {
    _dropStreak++;
    if (_dropStreak >= DROP_DEBOUNCE_N) {
      Serial.print(">> Vin dropped to ");
      Serial.print(_lastVinV, 2);
      Serial.print(" V (< ");
      Serial.print(DROP_VIN_V, 1);
      Serial.print(" V for ");
      Serial.print(DROP_DEBOUNCE_N);
      Serial.println(" checks) — releasing motor.");
      _cal.enterRelease();
      _ok = false;
      waitForPower();
      Serial.println(">> Power restored. Re-running boot ritual.");
      delay(3000);
      _cal.startHomeRitual();
    }
  } else {
    _dropStreak = 0;
  }
}
