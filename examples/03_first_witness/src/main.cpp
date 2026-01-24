/*
 * ============================================================================
 * FIRST WITNESS - PARAMETER TEST SEQUENCE
 * ============================================================================
 *
 * Automated testing: cycles through different parameters
 * Each test: 1 revolution (3s), then 2s pause
 *
 * BATCH 4: Testing update rate (Hz)
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

const int TEST_VALUES[] = {100, 250, 500, 750, 1000};  // Update rates in Hz
const int NUM_TESTS = 5;

// Fixed parameters
#define REVOLUTION_DURATION  3000   // 3 seconds total
#define ACCEL_TIME_MS        500    // 0.5s accel
#define DECEL_TIME_MS        1000   // 1s decel (longer for smoothness)
#define CURRENT_LIMIT        200000 // 2A

// =============================================================================
// EASING FUNCTIONS
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

enum TestState {
  TEST_WAITING,
  TEST_REVOLUTION,
  TEST_PAUSE_AFTER
};

TestState testState = TEST_WAITING;
unsigned long stateStartTime = 0;

// Revolution state - TIME BASED (not position based)
enum RevState {
  REV_IDLE,
  REV_ACCELERATING,
  REV_CRUISING,
  REV_DECELERATING,
  REV_COMPLETE
};

RevState revState = REV_IDLE;
bool revJustCompleted = false;
unsigned long revStartTime = 0;
int32_t revCruiseSpeed = 0;

// Timing
unsigned long accelEndTime = 0;
unsigned long cruiseEndTime = 0;
unsigned long decelEndTime = 0;

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
// LED
// =============================================================================

void showTestNumber(int num) {
  for (int i = 0; i < num; i++) {
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

// =============================================================================
// REVOLUTION - TIME BASED
// =============================================================================

void startRevolution() {
  if (revState != REV_IDLE) return;

  revJustCompleted = false;
  revStartTime = millis();

  // Calculate timing
  accelEndTime = revStartTime + ACCEL_TIME_MS;
  cruiseEndTime = revStartTime + REVOLUTION_DURATION - DECEL_TIME_MS;
  decelEndTime = revStartTime + REVOLUTION_DURATION;

  // Calculate cruise speed for 1 revolution in REVOLUTION_DURATION
  // Total distance = 36000 steps = 360°
  // Average speed during accel = cruise/2, during decel = cruise/2
  // Distance = (accel_time * cruise/2) + (cruise_time * cruise) + (decel_time * cruise/2)
  // 36000 = cruise * (accel/2 + cruise_time + decel/2) / 1000 * (steps/sec conversion)

  float accelSec = ACCEL_TIME_MS / 1000.0f;
  float cruiseSec = (REVOLUTION_DURATION - ACCEL_TIME_MS - DECEL_TIME_MS) / 1000.0f;
  float decelSec = DECEL_TIME_MS / 1000.0f;

  // effectiveTime = time at cruise speed equivalent
  // With smootherstep easing, average is ~0.5 of peak during accel/decel
  float effectiveTime = (accelSec / 2.0f) + cruiseSec + (decelSec / 2.0f);

  float stepsPerSec = STEPS_PER_REV / effectiveTime;
  float rpm = (stepsPerSec * 60.0f) / STEPS_PER_REV;

  // Add 5% to compensate for motor response lag
  rpm = rpm * 1.05f;

  revCruiseSpeed = (int32_t)(rpm * 100);  // Register value is RPM * 100

  revState = REV_ACCELERATING;
  setLED(0, 0, 50);
}

void updateRevolution() {
  if (revState == REV_IDLE) return;

  unsigned long now = millis();

  switch (revState) {
    case REV_ACCELERATING: {
      float t = (float)(now - revStartTime) / ACCEL_TIME_MS;
      t = constrain(t, 0.0f, 1.0f);
      float eased = smootherstep(t);
      motorSetSpeed((int32_t)(revCruiseSpeed * eased));

      if (now >= accelEndTime) {
        revState = REV_CRUISING;
        motorSetSpeed(revCruiseSpeed);
      }
      break;
    }

    case REV_CRUISING: {
      if (now >= cruiseEndTime) {
        revState = REV_DECELERATING;
      }
      break;
    }

    case REV_DECELERATING: {
      float t = (float)(decelEndTime - now) / DECEL_TIME_MS;
      t = constrain(t, 0.0f, 1.0f);
      float eased = smootherstep(t);
      motorSetSpeed((int32_t)(revCruiseSpeed * eased));

      if (now >= decelEndTime) {
        revState = REV_COMPLETE;
        motorStop();
      }
      break;
    }

    case REV_COMPLETE: {
      motorStop();
      revState = REV_IDLE;
      revJustCompleted = true;
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

void startNextTest() {
  if (currentTest >= NUM_TESTS) {
    currentTest = 0;
  }

  currentUpdateRate = TEST_VALUES[currentTest];

  showTestNumber(currentTest + 1);
  delay(500);

  testState = TEST_REVOLUTION;
  startRevolution();
}

void updateTestSequence() {
  switch (testState) {
    case TEST_WAITING:
      startNextTest();
      break;

    case TEST_REVOLUTION:
      updateRevolution();
      if (revJustCompleted) {
        revJustCompleted = false;
        testState = TEST_PAUSE_AFTER;
        stateStartTime = millis();
      }
      break;

    case TEST_PAUSE_AFTER:
      if (millis() - stateStartTime >= 2000) {
        currentTest++;
        startNextTest();
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
  updateTestSequence();

  // Variable delay based on current test's update rate
  // But timing is based on millis(), so this only affects smoothness
  if (currentUpdateRate > 0) {
    delay(1000 / currentUpdateRate);
  } else {
    delay(2);
  }
}
