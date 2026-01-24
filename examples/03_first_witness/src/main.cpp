/*
 * ============================================================================
 * FIRST WITNESS - PARAMETER TEST SEQUENCE
 * ============================================================================
 *
 * Automated testing: cycles through different parameters
 * Each test: 2 revolutions (3s each), 4s pause between
 * Then 10s pause before next test
 *
 * BATCH 1: Testing deceleration zone (degrees)
 * Test 1: 90°  (original)
 * Test 2: 120° (current)
 * Test 3: 150° (longer)
 * Test 4: 180° (half rotation)
 * Test 5: 210° (very gradual)
 *
 * Watch and note which test number feels smoothest!
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>

// =============================================================================
// HARDWARE CONFIG
// =============================================================================

#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8
#define I2C_SCL_PIN      9
#define I2C_FREQ         400000

// Registers
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

// Values to test (deceleration zone in degrees)
const int TEST_VALUES[] = {90, 120, 150, 180, 210};
const int NUM_TESTS = 5;

// Fixed parameters for this batch
#define REVOLUTION_DURATION  3000
#define ACCEL_TIME_MS        600
#define CURRENT_LIMIT        200000
#define UPDATE_RATE_HZ       500

// =============================================================================
// EASING FUNCTIONS
// =============================================================================

float easeOutQuint(float t) {
  float f = 1.0f - t;
  return 1.0f - (f * f * f * f * f);
}

float smootherstep(float t) {
  t = constrain(t, 0.0f, 1.0f);
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// =============================================================================
// STATE
// =============================================================================

int currentTest = 0;
int currentRev = 0;  // 0 or 1 (two revs per test)
int decelZoneDeg = TEST_VALUES[0];

enum TestState {
  TEST_WAITING,
  TEST_REVOLUTION,
  TEST_PAUSE_BETWEEN,
  TEST_PAUSE_AFTER
};

TestState testState = TEST_WAITING;
unsigned long stateStartTime = 0;

// Revolution state
enum RevState {
  REV_IDLE,
  REV_ACCELERATING,
  REV_CRUISING,
  REV_DECELERATING,
  REV_STOPPING
};

RevState revState = REV_IDLE;
int32_t revStartPos = 0;
int32_t revTargetPos = 0;
int32_t revCruiseSpeed = 0;
unsigned long revStartTime = 0;
unsigned long revAccelTime = ACCEL_TIME_MS;

// =============================================================================
// I2C FUNCTIONS
// =============================================================================

void writeReg8(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void writeReg32(uint8_t reg, int32_t value) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)(value & 0xFF));
  Wire.write((uint8_t)((value >> 8) & 0xFF));
  Wire.write((uint8_t)((value >> 16) & 0xFF));
  Wire.write((uint8_t)((value >> 24) & 0xFF));
  Wire.endTransmission();
}

int32_t readReg32(uint8_t reg) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)ROLLER_I2C_ADDR, (uint8_t)4);
  int32_t value = 0;
  if (Wire.available() >= 4) {
    value = Wire.read();
    value |= (int32_t)Wire.read() << 8;
    value |= (int32_t)Wire.read() << 16;
    value |= (int32_t)Wire.read() << 24;
  }
  return value;
}

// =============================================================================
// LED - Shows test number
// =============================================================================

void showTestNumber(int num) {
  // Blink LED to show test number (1-5)
  for (int i = 0; i < num; i++) {
    Wire.beginTransmission(ROLLER_I2C_ADDR);
    Wire.write(REG_RGB);
    Wire.write(50); Wire.write(50); Wire.write(0);  // Yellow
    Wire.endTransmission();
    delay(200);
    Wire.beginTransmission(ROLLER_I2C_ADDR);
    Wire.write(REG_RGB);
    Wire.write(0); Wire.write(0); Wire.write(0);  // Off
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
// MOTOR FUNCTIONS
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

void motorSetSpeed(int32_t speed) {
  writeReg32(REG_SPEED, speed);
}

void motorStop() {
  writeReg32(REG_SPEED, 0);
}

int32_t motorGetPosition() {
  return readReg32(REG_POS_READ);
}

// =============================================================================
// REVOLUTION
// =============================================================================

void startRevolution() {
  if (revState != REV_IDLE) return;

  unsigned long cruiseTime = REVOLUTION_DURATION - (2 * revAccelTime);
  float effectiveTime = revAccelTime / 1000.0f + cruiseTime / 1000.0f;
  float stepsPerSec = STEPS_PER_REV / effectiveTime;
  float rpm = (stepsPerSec * 60.0f) / STEPS_PER_REV;
  revCruiseSpeed = (int32_t)(rpm * 100);

  revStartPos = motorGetPosition();
  revTargetPos = revStartPos + STEPS_PER_REV;
  revStartTime = millis();
  revState = REV_ACCELERATING;

  setLED(0, 0, 50);  // Blue = moving
}

bool updateRevolution() {
  if (revState == REV_IDLE) return true;  // Done

  unsigned long elapsed = millis() - revStartTime;
  int32_t currentPos = motorGetPosition();

  switch (revState) {
    case REV_ACCELERATING: {
      float t = min(1.0f, (float)elapsed / revAccelTime);
      float eased = smootherstep(t);
      motorSetSpeed((int32_t)(revCruiseSpeed * eased));
      if (elapsed >= revAccelTime) {
        revState = REV_CRUISING;
        motorSetSpeed(revCruiseSpeed);
      }
      break;
    }

    case REV_CRUISING: {
      int32_t remaining = revTargetPos - currentPos;
      int32_t decelZoneSteps = (STEPS_PER_REV * decelZoneDeg) / 360;
      if (remaining <= decelZoneSteps) {
        revState = REV_DECELERATING;
      }
      break;
    }

    case REV_DECELERATING: {
      int32_t remaining = revTargetPos - currentPos;
      int32_t decelDistance = (STEPS_PER_REV * decelZoneDeg) / 360;

      if (remaining <= 30) {
        revState = REV_STOPPING;
        motorStop();
      } else {
        float t = (float)remaining / decelDistance;
        t = constrain(t, 0.0f, 1.0f);
        float speedFactor = easeOutQuint(t);
        motorSetSpeed((int32_t)(revCruiseSpeed * speedFactor));
      }
      break;
    }

    case REV_STOPPING: {
      motorStop();
      revState = REV_IDLE;
      setLED(0, 50, 0);  // Green
      return true;  // Done
    }

    default:
      break;
  }
  return false;  // Not done yet
}

// =============================================================================
// TEST SEQUENCE
// =============================================================================

void startNextTest() {
  if (currentTest >= NUM_TESTS) {
    // All tests complete - restart from beginning
    currentTest = 0;
  }

  decelZoneDeg = TEST_VALUES[currentTest];
  currentRev = 0;

  // Show test number with LED blinks
  showTestNumber(currentTest + 1);
  delay(500);

  testState = TEST_REVOLUTION;
  startRevolution();
}

void updateTestSequence() {
  switch (testState) {
    case TEST_WAITING:
      // Start first test
      startNextTest();
      break;

    case TEST_REVOLUTION:
      if (updateRevolution()) {
        // Revolution complete
        currentRev++;
        if (currentRev < 2) {
          // Do second revolution after 4s pause
          testState = TEST_PAUSE_BETWEEN;
          stateStartTime = millis();
        } else {
          // Both revolutions done, pause before next test
          testState = TEST_PAUSE_AFTER;
          stateStartTime = millis();
        }
      }
      break;

    case TEST_PAUSE_BETWEEN:
      if (millis() - stateStartTime >= 4000) {
        testState = TEST_REVOLUTION;
        startRevolution();
      }
      break;

    case TEST_PAUSE_AFTER:
      if (millis() - stateStartTime >= 10000) {
        currentTest++;
        startNextTest();
      }
      break;
  }
}

// =============================================================================
// SETUP
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

// =============================================================================
// LOOP
// =============================================================================

void loop() {
  updateTestSequence();
  delay(1000 / UPDATE_RATE_HZ);
}
