/*
 * I2C scanner v2 for First Witness — verbose, with stuck-bus detection.
 *
 * v1 hung silently on the first endTransmission(). That's a stuck-bus
 * signature: SDA or SCL is held low (wiring short, bad ground, or a slave
 * misbehaving). v2 reads the raw line states BEFORE configuring I2C, so
 * we can see whether the bus is even healthy.
 *
 * Tries TWO pin assignments:
 *   Pass A: SDA = GPIO8, SCL = GPIO9
 *   Pass B: SDA = GPIO9, SCL = GPIO8
 */

#include <Arduino.h>
#include <Wire.h>

static void readLineStates(int sdaPin, int sclPin) {
  pinMode(sdaPin, INPUT_PULLUP);
  pinMode(sclPin, INPUT_PULLUP);
  delay(5);
  int sda = digitalRead(sdaPin);
  int scl = digitalRead(sclPin);
  Serial.print("  Idle line states: SDA=");
  Serial.print(sda ? "HIGH" : "LOW ");
  Serial.print("  SCL=");
  Serial.println(scl ? "HIGH" : "LOW ");
  if (sda == LOW || scl == LOW) {
    Serial.println("  *** BUS STUCK *** A line is held low. This is the");
    Serial.println("      reason endTransmission() hangs. Likely cause:");
    Serial.println("      shorted wire, missing ground, or device fault.");
  } else {
    Serial.println("  Bus idle looks healthy (both lines pulled high).");
  }
}

static void scanBus(const char* tag, int sdaPin, int sclPin) {
  Serial.println();
  Serial.print("=== Pass [");
  Serial.print(tag);
  Serial.print("]  SDA=GPIO");
  Serial.print(sdaPin);
  Serial.print("  SCL=GPIO");
  Serial.print(sclPin);
  Serial.println(" ===");

  // Step 1: read the lines BEFORE Wire takes them over.
  readLineStates(sdaPin, sclPin);

  // Step 2: bring up I2C with a short timeout so we never hang.
  Wire.end();
  delay(50);
  Wire.begin(sdaPin, sclPin);
  Wire.setClock(100000);
  Wire.setTimeOut(50);  // ms per transaction
  delay(100);

  Serial.println("  Scanning 0x03..0x77 (250 ms progress dots)...");

  int found = 0;
  uint32_t lastDot = millis();
  for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("  >> ACK at 0x");
      if (addr < 0x10) Serial.print('0');
      Serial.println(addr, HEX);
      found++;
    }
    if (millis() - lastDot > 250) {
      lastDot = millis();
      Serial.print(".");
    }
    delay(2);
  }
  Serial.println();

  if (found == 0) {
    Serial.println("  Result: NO devices responded.");
  } else {
    Serial.print("  Result: ");
    Serial.print(found);
    Serial.println(" device(s) found.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(2500);

  Serial.println();
  Serial.println("============================================");
  Serial.println("  I2C Scanner v2 — verbose diagnostic");
  Serial.println("============================================");
  Serial.println("Production firmware expects motor at 0x64");
  Serial.println("on SDA=GPIO8, SCL=GPIO9.");
}

void loop() {
  scanBus("A", 8, 9);
  scanBus("B", 9, 8);

  Serial.println();
  Serial.println("--- Done. Repeating in 8 s. ---");
  delay(8000);
}
