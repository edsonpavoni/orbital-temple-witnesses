/*
 * M5Unit-Roller I2C Encoder Calibration Example
 * Adapted for XIAO ESP32-S3
 *
 * Calibrates the encoder, then runs motor in speed mode.
 * IMPORTANT: Keep motor shaft free to spin during calibration!
 */

#include <Arduino.h>
#include "unit_rolleri2c.hpp"

// XIAO ESP32-S3 I2C pins
#define I2C_SDA 8
#define I2C_SCL 9

#define ROLLER_CALIBRATION_DELAY 10000  // 10 seconds before calibration starts

UnitRollerI2C RollerI2C;

uint8_t is_roller_valid = 0;
uint8_t is_roller_calibrated = 0;
uint32_t roller_start_delay_counter = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);  // Wait for serial

    Serial.println("M5Unit-Roller Encoder Calibration");
    Serial.println("==================================");
    Serial.println("IMPORTANT: Keep motor shaft free to spin!");
    Serial.println();

    if (RollerI2C.begin(&Wire, 0x64, I2C_SDA, I2C_SCL, 400000)) {
        is_roller_valid = 1;
        roller_start_delay_counter = millis();
        Serial.println("Roller found!");
        Serial.println("Calibration will start in 10 seconds...");
    } else {
        Serial.println("ERROR: Roller not found!");
    }
}

void loop() {
    if (is_roller_valid) {
        if (millis() - roller_start_delay_counter < ROLLER_CALIBRATION_DELAY) {
            int remaining = (ROLLER_CALIBRATION_DELAY - (millis() - roller_start_delay_counter)) / 1000;
            Serial.printf("Calibration starts in %d seconds...\n", remaining);
            delay(1000);
        } else {
            if (!is_roller_calibrated) {
                Serial.println("\n>>> Starting encoder calibration <<<");
                Serial.println("Motor will spin - keep shaft free!");

                RollerI2C.setOutput(0);
                delay(100);

                RollerI2C.startAngleCal();
                delay(100);

                Serial.println("Calibrating...");
                while (RollerI2C.getCalBusyStatus()) {
                    Serial.println("  Still calibrating...");
                    delay(500);
                }

                RollerI2C.updateAngleCal();
                Serial.println("\n>>> Encoder calibration DONE <<<\n");

                delay(500);

                // Test: run in speed mode
                Serial.println("Testing motor in speed mode...");
                RollerI2C.setOutput(0);
                RollerI2C.setMode(ROLLER_MODE_SPEED);
                RollerI2C.setSpeed(240000);  // 2400 RPM
                RollerI2C.setSpeedMaxCurrent(100000);  // 1A max
                RollerI2C.setOutput(1);

                is_roller_calibrated = 1;
                Serial.println("Motor running at 2400 RPM");
            }
        }
    } else {
        Serial.println("No Roller detected - check connections");
        delay(2000);
    }
}
