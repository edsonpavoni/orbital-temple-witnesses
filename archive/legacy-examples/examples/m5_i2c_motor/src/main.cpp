/*
 * M5Unit-Roller I2C Motor Example
 * Adapted for XIAO ESP32-S3
 *
 * Tests all motor modes: Current, Position, Speed, Encoder
 * Also tests RGB LED and various status reads
 */

#include <Arduino.h>
#include "unit_rolleri2c.hpp"

// XIAO ESP32-S3 I2C pins
#define I2C_SDA 8
#define I2C_SCL 9

UnitRollerI2C RollerI2C;
uint32_t p, i, d;
uint8_t r, g, b;

void setup() {
    Serial.begin(115200);
    delay(2000);  // Wait for serial

    Serial.println("M5Unit-Roller I2C Motor Test");
    Serial.println("============================");

    if (!RollerI2C.begin(&Wire, 0x64, I2C_SDA, I2C_SCL, 400000)) {
        Serial.println("ERROR: Roller not found!");
        while(1) delay(1000);
    }

    Serial.println("Roller found!");
}

void loop() {
    // Current mode
    Serial.println("\n--- CURRENT MODE ---");
    RollerI2C.setMode(ROLLER_MODE_CURRENT);
    RollerI2C.setCurrent(120000);
    RollerI2C.setOutput(1);
    Serial.printf("current: %d\n", RollerI2C.getCurrent());
    delay(100);
    Serial.printf("actualCurrent: %.2f\n", RollerI2C.getCurrentReadback() / 100.0f);
    delay(5000);

    // Position mode
    Serial.println("\n--- POSITION MODE ---");
    RollerI2C.setOutput(0);
    RollerI2C.setMode(ROLLER_MODE_POSITION);
    RollerI2C.setPos(2000000);
    RollerI2C.setPosMaxCurrent(100000);
    RollerI2C.setOutput(1);
    RollerI2C.getPosPID(&p, &i, &d);
    Serial.printf("PosPID  P: %3.8f  I: %3.8f  D: %3.8f\n", p / 100000.0, i / 10000000.0, d / 100000.0);
    delay(100);
    Serial.printf("pos: %d\n", RollerI2C.getPos());
    delay(100);
    Serial.printf("posMaxCurrent: %.2f\n", RollerI2C.getPosMaxCurrent() / 100.0f);
    delay(100);
    Serial.printf("actualPos: %.2f\n", RollerI2C.getPosReadback() / 100.0f);
    delay(5000);

    // Speed mode
    Serial.println("\n--- SPEED MODE ---");
    RollerI2C.setOutput(0);
    RollerI2C.setMode(ROLLER_MODE_SPEED);
    RollerI2C.setSpeed(240000);
    RollerI2C.setSpeedMaxCurrent(100000);
    RollerI2C.setOutput(1);
    RollerI2C.getSpeedPID(&p, &i, &d);
    Serial.printf("SpeedPID  P: %3.8f  I: %3.8f  D: %3.8f\n", p / 100000.0, i / 10000000.0, d / 100000.0);
    delay(100);
    Serial.printf("speed: %d\n", RollerI2C.getSpeed());
    delay(100);
    Serial.printf("speedMaxCurrent: %.2f\n", RollerI2C.getSpeedMaxCurrent() / 100.0f);
    delay(100);
    Serial.printf("actualSpeed: %.2f\n", RollerI2C.getSpeedReadback() / 100.0f);
    delay(5000);

    // Encoder mode
    Serial.println("\n--- ENCODER MODE ---");
    RollerI2C.setOutput(0);
    RollerI2C.setMode(ROLLER_MODE_ENCODER);
    RollerI2C.setDialCounter(240000);
    RollerI2C.setOutput(1);
    Serial.printf("DialCounter: %d\n", RollerI2C.getDialCounter());
    delay(5000);

    // Status reads
    Serial.println("\n--- STATUS ---");
    Serial.printf("temp: %d C\n", RollerI2C.getTemp());
    delay(100);
    Serial.printf("Vin: %.2f V\n", RollerI2C.getVin() / 100.0);
    delay(100);
    Serial.printf("RGBBrightness: %d\n", RollerI2C.getRGBBrightness());
    delay(1000);

    // RGB test
    Serial.println("\n--- RGB TEST ---");
    RollerI2C.setRGBBrightness(100);
    delay(100);
    RollerI2C.setRGBMode(ROLLER_RGB_MODE_USER_DEFINED);
    delay(1000);

    Serial.println("White");
    RollerI2C.setRGB(0xFFFFFF);
    delay(1000);

    Serial.println("Blue");
    RollerI2C.setRGB(0x0000FF);
    delay(1000);

    Serial.println("Yellow");
    RollerI2C.setRGB(0xFFFF00);
    delay(1000);

    Serial.println("Red");
    RollerI2C.setRGB(0xFF0000);
    delay(1000);

    Serial.println("Green");
    RollerI2C.setRGB(0x00FF00);
    delay(1000);

    RollerI2C.setRGBMode(ROLLER_RGB_MODE_DEFAULT);
    delay(100);

    // More status
    Serial.println("\n--- MORE STATUS ---");
    RollerI2C.setKeySwitchMode(1);
    delay(100);
    Serial.printf("I2CAddress: 0x%02X\n", RollerI2C.getI2CAddress());
    delay(100);
    Serial.printf("485 BPS: %d\n", RollerI2C.getBPS());
    delay(100);
    Serial.printf("485 motor id: %d\n", RollerI2C.getMotorID());
    delay(100);
    Serial.printf("motor output: %d\n", RollerI2C.getOutputStatus());
    delay(100);
    Serial.printf("SysStatus: %d\n", RollerI2C.getSysStatus());
    delay(100);
    Serial.printf("ErrorCode: %d\n", RollerI2C.getErrorCode());
    delay(100);
    Serial.printf("Button switching mode enable: %d\n", RollerI2C.getKeySwitchMode());
    delay(100);
    RollerI2C.getRGB(&r, &g, &b);
    Serial.printf("RGB - R: 0x%02X  G: 0x%02X  B: 0x%02X\n", r, g, b);

    Serial.println("\n\n========== LOOP COMPLETE ==========\n");
    delay(5000);
}
