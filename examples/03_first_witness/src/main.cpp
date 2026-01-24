/*
 * ============================================================================
 * FIRST WITNESS - PARAMETER TEST (HYBRID APPROACH)
 * ============================================================================
 *
 * HYBRID: Time-based accel + Position-based decel = smooth AND precise
 *
 * BATCH 4: Update Rate (Hz)
 * Test 1: 100Hz
 * Test 2: 250Hz
 * Test 3: 500Hz
 * Test 4: 750Hz
 * Test 5: 1000Hz
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
// TEST PARAMETERS
// =============================================================================

const int TEST_VALUES[] = {100, 250, 500, 750, 1000};
const int NUM_TESTS = 5;

// Fixed parameters (best from previous tests)
#define ACCEL_TIME_MS    500     // Time-based accel
#define DECEL_ZONE_DEG   90      // Position-based decel (reduced from 180)
#define CURRENT_LIMIT    200000  // 2A
#define MIN_SPEED        100     // 1 RPM minimum during decel

// =============================================================================
// EASING
// =============================================================================

float smootherstep(float t) {
  t = constrain(t, 0.0f, 1.0f);
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// =============================================================================
// STATE
// =============================================================================

int currentTest = 0;
int currentUpdateRate = TEST_VALUES[0];

enum TestState { TEST_WAITING, TEST_REVOLUTION, TEST_PAUSE };
TestState testState = TEST_WAITING;
unsigned long stateStartTime = 0;

enum RevState { REV_IDLE, REV_ACCEL, REV_CRUISE, REV_DECEL, REV_DONE };
RevState revState = REV_IDLE;
bool revComplete = false;

int32_t revStartPos = 0;
int32_t revTargetPos = 0;
int32_t revCruiseSpeed = 2000;  // ~20 RPM default
unsigned long revStartTime = 0;

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

void showTestNum(int n) {
  for (int i = 0; i < n; i++) {
    Wire.beginTransmission(ROLLER_I2C_ADDR);
    Wire.write(REG_RGB);
    Wire.write(50); Wire.write(50); Wire.write(0);
    Wire.endTransmission();
    delay(200);
    Wire.beginTransmission(ROLLER_I2C_ADDR);
    Wire.write(REG_RGB);
    Wire.write(0); Wire.write(0); Wire.write(0);
    Wire.endTransmission();
    delay(200);
  }
}

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
// REVOLUTION (HYBRID: time accel + position decel)
// =============================================================================

void startRevolution() {
  if (revState != REV_IDLE) return;

  revComplete = false;
  revStartPos = getPos();
  revTargetPos = revStartPos + STEPS_PER_REV;
  revStartTime = millis();
  revCruiseSpeed = 2000;  // 20 RPM
  revState = REV_ACCEL;
  setLED(0, 0, 50);
}

void updateRevolution() {
  if (revState == REV_IDLE) return;

  unsigned long elapsed = millis() - revStartTime;
  int32_t pos = getPos();
  int32_t remaining = revTargetPos - pos;
  int32_t decelZone = (STEPS_PER_REV * DECEL_ZONE_DEG) / 360;

  switch (revState) {
    case REV_ACCEL: {
      // Time-based acceleration
      float t = (float)elapsed / ACCEL_TIME_MS;
      t = constrain(t, 0.0f, 1.0f);
      int32_t spd = (int32_t)(revCruiseSpeed * smootherstep(t));
      setSpeed(spd);

      if (elapsed >= ACCEL_TIME_MS) {
        revState = REV_CRUISE;
        setSpeed(revCruiseSpeed);
      }
      break;
    }

    case REV_CRUISE: {
      // Cruise until position trigger
      if (remaining <= decelZone) {
        revState = REV_DECEL;
      }
      break;
    }

    case REV_DECEL: {
      // Position-based deceleration
      if (remaining <= 50) {
        revState = REV_DONE;
        setSpeed(0);
      } else {
        float t = (float)remaining / decelZone;
        t = constrain(t, 0.0f, 1.0f);
        int32_t spd = (int32_t)(revCruiseSpeed * smootherstep(t));
        if (spd < MIN_SPEED) spd = MIN_SPEED;
        setSpeed(spd);
      }
      break;
    }

    case REV_DONE: {
      setSpeed(0);
      revState = REV_IDLE;
      revComplete = true;
      setLED(0, 50, 0);
      break;
    }

    default:
      break;
  }
}

// =============================================================================
// TEST SEQUENCE
// =============================================================================

void startTest() {
  if (currentTest >= NUM_TESTS) currentTest = 0;
  currentUpdateRate = TEST_VALUES[currentTest];
  showTestNum(currentTest + 1);
  delay(500);
  testState = TEST_REVOLUTION;
  startRevolution();
}

void updateTest() {
  switch (testState) {
    case TEST_WAITING:
      startTest();
      break;

    case TEST_REVOLUTION:
      updateRevolution();
      if (revComplete) {
        revComplete = false;
        testState = TEST_PAUSE;
        stateStartTime = millis();
      }
      break;

    case TEST_PAUSE:
      if (millis() - stateStartTime >= 2000) {
        currentTest++;
        startTest();
      }
      break;
  }
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
  setLED(0, 50, 0);
  delay(2000);
  testState = TEST_WAITING;
}

void loop() {
  updateTest();
  if (currentUpdateRate > 0) {
    delay(1000 / currentUpdateRate);
  }
}
