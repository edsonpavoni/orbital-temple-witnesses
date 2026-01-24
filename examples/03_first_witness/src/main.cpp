/*
 * ============================================================================
 * FIRST WITNESS - AUTO REVOLUTION TEST
 * ============================================================================
 *
 * Automatic test: 360° revolution every 10 seconds
 * Duration: 3 seconds per revolution
 * Motion: S-curve (quint ease-out for smooth stop)
 *
 * NO SERIAL - runs standalone without USB host.
 *
 * LED FEEDBACK:
 * - RED blink on startup = code is running
 * - YELLOW = initializing motor
 * - GREEN = ready/idle
 * - BLUE = moving
 * - RED solid = error (motor not found)
 *
 * HARDWARE:
 * - Seeed XIAO ESP32-S3
 * - M5Stack RollerCAN BLDC (I2C)
 * - 15V power via USB-C PD trigger board
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8
#define I2C_SCL_PIN      9
#define I2C_FREQ         400000  // 400kHz (was 100kHz) - faster I2C

// Registers
#define REG_OUTPUT       0x00
#define REG_MODE         0x01
#define REG_STALL_PROT   0x0F
#define REG_RGB          0x30
#define REG_SPEED        0x40
#define REG_SPEED_MAXCUR 0x50
#define REG_POS_READ     0x90

#define MODE_SPEED       1

// Motor resolution
#define STEPS_PER_REV    36000

// Timing
#define REVOLUTION_DURATION  3000   // 3 seconds per revolution
#define REVOLUTION_INTERVAL  10000  // 10 seconds between revolutions

// =============================================================================
// TUNING PARAMETERS (adjust these to improve smoothness)
// =============================================================================
#define ACCEL_TIME_MS     600     // Acceleration duration (ms)
#define DECEL_ZONE_DEG    120     // Deceleration zone (degrees) - was 90
#define MIN_SPEED         0       // Minimum speed during decel (0 = ramp to zero)
#define UPDATE_RATE_HZ    500     // Loop update rate (was 200)
#define CURRENT_LIMIT     200000  // Max current (×100000) - 2A (was 1.5A)

// =============================================================================
// EASING FUNCTION
// =============================================================================

// Attempt #4: ease-out quint (very smooth approach to zero)
float easeOutQuint(float t) {
  float f = 1.0f - t;
  return 1.0f - (f * f * f * f * f);
}

// Ken Perlin's smootherstep for acceleration
float smootherstep(float t) {
  t = constrain(t, 0.0f, 1.0f);
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// =============================================================================
// STATE
// =============================================================================

int32_t currentLimit = CURRENT_LIMIT;

// Revolution state machine
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
unsigned long lastRevolutionTime = 0;
int revolutionCount = 0;

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

void setLED(uint8_t r, uint8_t g, uint8_t b) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(REG_RGB);
  Wire.write(r);
  Wire.write(g);
  Wire.write(b);
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
  writeReg32(REG_SPEED_MAXCUR, currentLimit);
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

  // Calculate cruise speed for 3 second revolution
  // With S-curve, effective time is slightly different
  // Approximate: cruise covers most of the distance
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

void updateRevolution() {
  if (revState == REV_IDLE) return;

  unsigned long elapsed = millis() - revStartTime;
  int32_t currentPos = motorGetPosition();

  switch (revState) {
    case REV_ACCELERATING: {
      float t = min(1.0f, (float)elapsed / revAccelTime);
      float eased = smootherstep(t);
      int32_t speed = (int32_t)(revCruiseSpeed * eased);
      motorSetSpeed(speed);

      if (elapsed >= revAccelTime) {
        revState = REV_CRUISING;
        motorSetSpeed(revCruiseSpeed);
      }
      break;
    }

    case REV_CRUISING: {
      int32_t remaining = revTargetPos - currentPos;
      int32_t decelZoneSteps = (STEPS_PER_REV * DECEL_ZONE_DEG) / 360;
      if (remaining <= decelZoneSteps) {
        revState = REV_DECELERATING;
      }
      break;
    }

    case REV_DECELERATING: {
      int32_t remaining = revTargetPos - currentPos;
      int32_t decelDistance = (STEPS_PER_REV * DECEL_ZONE_DEG) / 360;

      if (remaining <= 30) {  // ~0.3° from target
        revState = REV_STOPPING;
        motorStop();
      } else {
        float t = (float)remaining / decelDistance;
        t = constrain(t, 0.0f, 1.0f);
        float speedFactor = easeOutQuint(t);

        int32_t speed = (int32_t)(revCruiseSpeed * speedFactor);
        if (speed < MIN_SPEED) speed = MIN_SPEED;
        motorSetSpeed(speed);
      }
      break;
    }

    case REV_STOPPING: {
      motorStop();
      revolutionCount++;
      revState = REV_IDLE;
      setLED(0, 50, 0);  // Green = ready
      break;
    }

    default:
      break;
  }
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  // NO SERIAL - runs without USB host

  // I2C init
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  // Check motor
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    // Motor not found - stuck here
    while(1) {
      delay(1000);
    }
  }

  // Init motor
  motorInit();
  delay(500);

  // STARTUP WIGGLE - shows code is running
  // Small back-and-forth movement
  motorSetSpeed(300);   // Slow forward
  delay(300);
  motorSetSpeed(-300);  // Slow backward
  delay(300);
  motorSetSpeed(300);   // Forward again
  delay(300);
  motorStop();
  delay(1000);

  // Start first revolution immediately
  lastRevolutionTime = millis() - REVOLUTION_INTERVAL;
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
  // Auto-trigger revolution every 10 seconds
  if (revState == REV_IDLE) {
    if (millis() - lastRevolutionTime >= REVOLUTION_INTERVAL) {
      lastRevolutionTime = millis();
      startRevolution();
    }
  }

  // Update revolution state machine
  updateRevolution();

  delay(1000 / UPDATE_RATE_HZ);  // Configurable update rate
}
