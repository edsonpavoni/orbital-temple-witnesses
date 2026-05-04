// pointer_v1 — First Witness Series, fresh start.
//
// Xiao ESP32-S3 + M5Stack Unit-Roller485 Lite over I2C (Grove).
// Motor powered by PD board at 15 V on PWR-485.
//
// Behavior:
//   - At boot, current pointer angle becomes 0 deg (top of clock).
//   - Type an angle in the Serial Monitor (e.g. "0", "90", "154", "180")
//     and the brass pointer ramps slowly to that angle and holds.
//   - Speed: 30 deg/sec (so 180 deg takes 6 s).

#include <Arduino.h>
#include <Wire.h>
#include "unit_rolleri2c.hpp"

// ---- pins / address ----
#define I2C_SDA      8
#define I2C_SCL      9
#define ROLLER_ADDR  0x64

// ---- motion config ----
const float    SPEED_DEG_PER_S  = 30.0f;    // 180 deg in 6 s
const uint32_t STEP_INTERVAL_MS = 10;       // setpoint update cadence
const int32_t  POS_MAX_CURRENT  = 20000;    // centi-mA -> 200 mA.
                                            // LOW current = smooth, low torque.
                                            // raise if it stalls (try 30000, 50000).
const int32_t  COUNTS_PER_DEG   = 100;      // motor: 36000 counts = 360 deg

const uint32_t STATUS_INTERVAL_MS = 250;    // how often to print actual pos for debugging

// ---- state ----
UnitRollerI2C roller;
int32_t  zeroOffsetCounts = 0;
float    currentTargetDeg = 0;
float    finalTargetDeg   = 0;
uint32_t lastStepMs       = 0;
uint32_t lastStatusMs     = 0;
String   serialBuf;

void sendTargetDeg(float deg) {
    int32_t counts = zeroOffsetCounts + (int32_t)lroundf(deg * COUNTS_PER_DEG);
    roller.setPos(counts);
}

float readActualDeg() {
    int32_t counts = roller.getPosReadback() - zeroOffsetCounts;
    return counts / (float)COUNTS_PER_DEG;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("pointer_v1 booting...");

    Wire.begin(I2C_SDA, I2C_SCL);
    if (!roller.begin(&Wire, ROLLER_ADDR, I2C_SDA, I2C_SCL, 400000)) {
        Serial.println("ERROR: roller.begin() failed.");
    }

    roller.setOutput(0);
    delay(50);
    roller.setMode(ROLLER_MODE_POSITION);
    roller.setPosMaxCurrent(POS_MAX_CURRENT);

    delay(100);
    zeroOffsetCounts = roller.getPosReadback();
    Serial.print("Zero offset captured: ");
    Serial.println(zeroOffsetCounts);

    sendTargetDeg(0);
    roller.setOutput(1);

    Serial.print("Max current: ");
    Serial.print(POS_MAX_CURRENT / 100.0f);
    Serial.println(" mA");
    Serial.println("Ready. Type an angle (0-360) and press Enter.");
}

void loop() {
    // --- serial input ---
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            serialBuf.trim();
            if (serialBuf.length() > 0) {
                float val = serialBuf.toFloat();
                if (val < 0)   val = 0;
                if (val > 360) val = 360;
                finalTargetDeg = val;
                Serial.print("-> moving to ");
                Serial.print(finalTargetDeg, 1);
                Serial.println(" deg");
            }
            serialBuf = "";
        } else {
            serialBuf += c;
        }
    }

    // --- ramp the setpoint ---
    uint32_t now = millis();
    if (now - lastStepMs >= STEP_INTERVAL_MS) {
        lastStepMs = now;

        float maxStep = SPEED_DEG_PER_S * (STEP_INTERVAL_MS / 1000.0f);
        float diff    = finalTargetDeg - currentTargetDeg;
        if (fabsf(diff) <= maxStep) {
            currentTargetDeg = finalTargetDeg;
        } else {
            currentTargetDeg += (diff > 0 ? maxStep : -maxStep);
        }
        sendTargetDeg(currentTargetDeg);
    }

    // --- periodic status (so we can SEE what's happening) ---
    if (now - lastStatusMs >= STATUS_INTERVAL_MS) {
        lastStatusMs = now;
        float actualDeg = readActualDeg();
        Serial.printf("  setpoint=%.1f  actual=%.1f  final=%.1f\n",
                      currentTargetDeg, actualDeg, finalTargetDeg);
    }
}
