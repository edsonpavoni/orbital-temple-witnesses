/*
 * ============================================================================
 * SMOOTHNESS TEST - Compare different approaches for smooth 360° rotation
 * ============================================================================
 *
 * Serial commands:
 *   1 - Speed mode + sine curve (current approach)
 *   2 - Current mode + sine curve (torque control)
 *   3 - Speed mode + lower max current (gentler)
 *   4 - Speed mode + reduced PID P (less aggressive)
 *   5 - Position mode (motor's internal control)
 *   p - Print current PID values
 *   s - Stop motor
 *   r - Read position/speed/current
 *
 * ============================================================================
 */

#include <Arduino.h>
#include "unit_rolleri2c.hpp"

// XIAO ESP32-S3 I2C pins
#define I2C_SDA 8
#define I2C_SCL 9

// Motor constants
#define STEPS_PER_REV 36000
#define REVOLUTION_DURATION 3000  // 3 seconds

UnitRollerI2C Roller;

// State
bool revRunning = false;
unsigned long revStartTime = 0;
int32_t revStartPos = 0;
int currentMode = 0;  // Which test mode

// Original PID values (saved on startup)
uint32_t origSpeedP, origSpeedI, origSpeedD;
uint32_t origPosP, origPosI, origPosD;

// =============================================================================
// SINE CURVE REVOLUTION
// =============================================================================

void startRevolution(int mode) {
    if (revRunning) {
        Serial.println("Already running!");
        return;
    }

    currentMode = mode;
    revStartPos = Roller.getPosReadback();
    revStartTime = millis();
    revRunning = true;

    // Configure based on mode
    Roller.setOutput(0);
    delay(50);

    switch (mode) {
        case 1:  // Speed mode + sine curve (normal)
            Serial.println("\n>>> Mode 1: Speed + Sine (normal) <<<");
            Roller.setMode(ROLLER_MODE_SPEED);
            Roller.setSpeedMaxCurrent(200000);  // 2A
            break;

        case 2:  // Current mode + sine curve
            Serial.println("\n>>> Mode 2: Current + Sine (torque control) <<<");
            Roller.setMode(ROLLER_MODE_CURRENT);
            break;

        case 3:  // Speed mode + lower current
            Serial.println("\n>>> Mode 3: Speed + Sine (low current 0.5A) <<<");
            Roller.setMode(ROLLER_MODE_SPEED);
            Roller.setSpeedMaxCurrent(50000);  // 0.5A
            break;

        case 4:  // Speed mode + reduced PID
            Serial.println("\n>>> Mode 4: Speed + Sine (reduced PID P) <<<");
            Roller.setMode(ROLLER_MODE_SPEED);
            Roller.setSpeedMaxCurrent(200000);
            // Reduce P to 50% of original
            Roller.setSpeedPID(origSpeedP / 2, origSpeedI, origSpeedD);
            break;

        case 5:  // Position mode (motor's internal)
            Serial.println("\n>>> Mode 5: Position mode (motor internal) <<<");
            Roller.setMode(ROLLER_MODE_POSITION);
            Roller.setPosMaxCurrent(200000);
            Roller.setPos(revStartPos + STEPS_PER_REV);
            break;
    }

    Roller.setOutput(1);
    Serial.printf("Start pos: %d\n", revStartPos);
}

void updateRevolution() {
    if (!revRunning) return;

    unsigned long elapsed = millis() - revStartTime;

    // Position mode handles itself
    if (currentMode == 5) {
        int32_t currentPos = Roller.getPosReadback();
        int32_t target = revStartPos + STEPS_PER_REV;
        if (abs(currentPos - target) < 100 || elapsed > REVOLUTION_DURATION * 2) {
            revRunning = false;
            Serial.printf("\nDone! Final pos: %d (target: %d)\n", currentPos, target);
            Serial.printf("Error: %d steps (%.2f deg)\n", currentPos - target, (currentPos - target) * 360.0f / STEPS_PER_REV);
        }
        return;
    }

    // Time's up
    if (elapsed >= REVOLUTION_DURATION) {
        if (currentMode == 2) {
            Roller.setCurrent(0);
        } else {
            Roller.setSpeed(0);
        }
        revRunning = false;

        delay(100);
        int32_t finalPos = Roller.getPosReadback();
        int32_t target = revStartPos + STEPS_PER_REV;

        Serial.printf("\nDone! Final pos: %d (target: %d)\n", finalPos, target);
        Serial.printf("Error: %d steps (%.2f deg)\n", finalPos - target, (finalPos - target) * 360.0f / STEPS_PER_REV);

        // Restore PID if we changed it
        if (currentMode == 4) {
            Roller.setSpeedPID(origSpeedP, origSpeedI, origSpeedD);
            Serial.println("PID restored");
        }
        return;
    }

    // Sine curve
    float t = (float)elapsed / REVOLUTION_DURATION;
    float v_avg = (float)STEPS_PER_REV / (REVOLUTION_DURATION / 1000.0f);  // steps/sec
    float sineValue = (1.0f - cos(TWO_PI * t));  // 0 to 2 to 0

    if (currentMode == 2) {
        // Current mode: convert to current command
        // Peak current for average torque needed
        float peakCurrent = 150000;  // 1.5A peak
        int32_t current = (int32_t)(peakCurrent * sineValue / 2.0f);
        Roller.setCurrent(current);
    } else {
        // Speed mode
        float v_avg_rpm = (v_avg * 60.0f) / STEPS_PER_REV;
        float v_motor = v_avg_rpm * 100.0f * sineValue;
        Roller.setSpeed((int32_t)v_motor);
    }
}

// =============================================================================
// COMMANDS
// =============================================================================

void printPID() {
    uint32_t p, i, d;

    Roller.getSpeedPID(&p, &i, &d);
    Serial.println("\n--- Speed PID ---");
    Serial.printf("P: %u (%.5f)\n", p, p / 100000.0);
    Serial.printf("I: %u (%.7f)\n", i, i / 10000000.0);
    Serial.printf("D: %u (%.5f)\n", d, d / 100000.0);

    Roller.getPosPID(&p, &i, &d);
    Serial.println("\n--- Position PID ---");
    Serial.printf("P: %u (%.5f)\n", p, p / 100000.0);
    Serial.printf("I: %u (%.7f)\n", i, i / 10000000.0);
    Serial.printf("D: %u (%.5f)\n", d, d / 100000.0);
}

void printStatus() {
    Serial.println("\n--- Status ---");
    Serial.printf("Position: %d (%.1f deg)\n", Roller.getPosReadback(), Roller.getPosReadback() * 360.0f / STEPS_PER_REV);
    Serial.printf("Speed: %.2f RPM\n", Roller.getSpeedReadback() / 100.0f);
    Serial.printf("Current: %.2f A\n", Roller.getCurrentReadback() / 100000.0f);
    Serial.printf("Voltage: %.2f V\n", Roller.getVin() / 100.0f);
    Serial.printf("Temp: %d C\n", Roller.getTemp());
}

void stopMotor() {
    Roller.setOutput(0);
    revRunning = false;
    Serial.println("Motor stopped");
}

void printHelp() {
    Serial.println("\n============================================");
    Serial.println("SMOOTHNESS TEST - Commands:");
    Serial.println("============================================");
    Serial.println("  1 - Speed mode + sine curve (current approach)");
    Serial.println("  2 - Current mode + sine curve (torque control)");
    Serial.println("  3 - Speed mode + low current (0.5A max)");
    Serial.println("  4 - Speed mode + reduced PID P (50%)");
    Serial.println("  5 - Position mode (motor internal control)");
    Serial.println("  p - Print PID values");
    Serial.println("  s - Stop motor");
    Serial.println("  r - Read status (pos/speed/current)");
    Serial.println("  ? - Show this help");
    Serial.println("============================================\n");
}

// =============================================================================
// SETUP & LOOP
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\nSMOOTHNESS TEST");
    Serial.println("===============");

    if (!Roller.begin(&Wire, 0x64, I2C_SDA, I2C_SCL, 400000)) {
        Serial.println("ERROR: Roller not found!");
        while(1) delay(1000);
    }

    Serial.println("Roller found!");

    // Save original PID values
    Roller.getSpeedPID(&origSpeedP, &origSpeedI, &origSpeedD);
    Roller.getPosPID(&origPosP, &origPosI, &origPosD);

    // Set RGB to show ready
    Roller.setRGBMode(ROLLER_RGB_MODE_USER_DEFINED);
    Roller.setRGB(0x00FF00);  // Green

    printHelp();
}

void loop() {
    // Handle serial commands
    if (Serial.available()) {
        char cmd = Serial.read();

        switch (cmd) {
            case '1': startRevolution(1); break;
            case '2': startRevolution(2); break;
            case '3': startRevolution(3); break;
            case '4': startRevolution(4); break;
            case '5': startRevolution(5); break;
            case 'p': case 'P': printPID(); break;
            case 's': case 'S': stopMotor(); break;
            case 'r': case 'R': printStatus(); break;
            case '?': case 'h': case 'H': printHelp(); break;
        }
    }

    // Update revolution
    updateRevolution();

    // Update LED based on state
    if (revRunning) {
        Roller.setRGB(0x0000FF);  // Blue = moving
    } else {
        Roller.setRGB(0x00FF00);  // Green = ready
    }

    delay(10);  // 100Hz
}
