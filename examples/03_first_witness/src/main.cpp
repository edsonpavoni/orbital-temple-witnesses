/*
 * ============================================================================
 * FIRST WITNESS - SINE CURVE WITH DRIFT CORRECTION
 * ============================================================================
 *
 * Hybrid approach:
 * - Smooth sine curve for motion (pure time-based)
 * - After each revolution, measure position error
 * - Adjust next revolution to compensate
 *
 * Result: Smooth motion + precise position over time
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>

// =============================================================================
// HARDWARE
// =============================================================================

#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8
#define I2C_SCL_PIN      9
#define I2C_FREQ         400000

#define REG_OUTPUT       0x00
#define REG_MODE         0x01
#define REG_STALL_PROT   0x0F
#define REG_RGB          0x30
#define REG_SPEED        0x40
#define REG_SPEED_MAXCUR 0x50
#define REG_POS_READ     0x90

#define MODE_SPEED       1
#define STEPS_PER_REV    36000

// =============================================================================
// MOTION PARAMETERS
// =============================================================================

#define REVOLUTION_DURATION  3000    // 3 seconds
#define CURRENT_LIMIT        200000  // 2A
#define UPDATE_RATE_HZ       100     // 100Hz

// =============================================================================
// STATE
// =============================================================================

bool revRunning = false;
unsigned long revStartTime = 0;
bool revComplete = false;

// Position tracking for drift correction
int32_t revStartPos = 0;
int32_t revTargetPos = 0;
int32_t revDistance = STEPS_PER_REV;  // Adjusted each revolution

// Test sequence
unsigned long pauseStart = 0;
bool inPause = false;
int revCount = 0;

// =============================================================================
// I2C
// =============================================================================

void writeReg8(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void writeReg32(uint8_t reg, int32_t val) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)(val & 0xFF));
  Wire.write((uint8_t)((val >> 8) & 0xFF));
  Wire.write((uint8_t)((val >> 16) & 0xFF));
  Wire.write((uint8_t)((val >> 24) & 0xFF));
  Wire.endTransmission();
}

int32_t readReg32(uint8_t reg) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)ROLLER_I2C_ADDR, (uint8_t)4);
  int32_t val = 0;
  if (Wire.available() >= 4) {
    val = Wire.read();
    val |= (int32_t)Wire.read() << 8;
    val |= (int32_t)Wire.read() << 16;
    val |= (int32_t)Wire.read() << 24;
  }
  return val;
}

// =============================================================================
// LED
// =============================================================================

void setLED(uint8_t r, uint8_t g, uint8_t b) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(REG_RGB);
  Wire.write(r); Wire.write(g); Wire.write(b);
  Wire.endTransmission();
}

// =============================================================================
// MOTOR
// =============================================================================

void motorInit() {
  writeReg8(REG_OUTPUT, 0);
  delay(50);
  writeReg8(REG_STALL_PROT, 0);
  delay(50);
  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);
  writeReg32(REG_SPEED_MAXCUR, CURRENT_LIMIT);
  delay(50);
  writeReg32(REG_SPEED, 0);
  delay(50);
  writeReg8(REG_OUTPUT, 1);
}

void setSpeed(int32_t spd) {
  writeReg32(REG_SPEED, spd);
}

int32_t getPos() {
  return readReg32(REG_POS_READ);
}

// =============================================================================
// REVOLUTION - SINE CURVE WITH DRIFT CORRECTION
// =============================================================================

void startRevolution() {
  if (revRunning) return;

  revComplete = false;
  revStartPos = getPos();
  revTargetPos = revStartPos + STEPS_PER_REV;  // Ideal target
  revStartTime = millis();
  revRunning = true;
  setLED(0, 0, 50);  // Blue = moving
}

void updateRevolution() {
  if (!revRunning) return;

  unsigned long elapsed = millis() - revStartTime;

  // Time's up - stop and measure error
  if (elapsed >= REVOLUTION_DURATION) {
    setSpeed(0);
    revRunning = false;
    revComplete = true;

    // Measure position error
    int32_t actualPos = getPos();
    int32_t error = actualPos - revTargetPos;  // + = overshoot, - = undershoot

    // Adjust next revolution distance to compensate
    // If we overshot by 100 steps, next rev should be 35900 steps
    revDistance = STEPS_PER_REV - error;

    // Clamp to reasonable range (avoid wild corrections)
    if (revDistance < STEPS_PER_REV - 1000) revDistance = STEPS_PER_REV - 1000;
    if (revDistance > STEPS_PER_REV + 1000) revDistance = STEPS_PER_REV + 1000;

    revCount++;
    setLED(0, 50, 0);  // Green = done
    return;
  }

  // Sine curve velocity profile
  // v(t) = v_avg * (1 - cos(2πt))

  float t = (float)elapsed / REVOLUTION_DURATION;

  // Use adjusted distance for v_avg calculation
  float v_avg = (float)revDistance / (REVOLUTION_DURATION / 1000.0f);

  // Convert to motor units (RPM * 100)
  float v_avg_motor = (v_avg * 60.0f / STEPS_PER_REV) * 100.0f;

  // Sine curve
  float currentSpeed = v_avg_motor * (1.0f - cos(TWO_PI * t));

  setSpeed((int32_t)currentSpeed);
}

// =============================================================================
// SETUP & LOOP
// =============================================================================

void setup() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    while(1) delay(1000);
  }

  motorInit();
  setLED(0, 50, 0);  // Green = ready
  delay(2000);

  // Start first revolution
  startRevolution();
}

void loop() {
  if (inPause) {
    // Waiting between revolutions
    if (millis() - pauseStart >= 3000) {
      inPause = false;
      startRevolution();
    }
  } else {
    // Running revolution
    updateRevolution();

    if (revComplete) {
      revComplete = false;
      inPause = true;
      pauseStart = millis();
    }
  }

  delay(1000 / UPDATE_RATE_HZ);  // 100Hz = 10ms
}
