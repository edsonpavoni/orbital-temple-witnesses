/*
 * ============================================================================
 * HEART SCULPTURE - SIMPLE ROTATION
 * ============================================================================
 *
 * BEHAVIOR:
 * 1. INIT: Rotates 2 full turns on startup
 * 2. HOLD: Maximum holding power to keep motor fixed
 * 3. GENTLE ROTATION: Slowly rotates 360° in one direction, then 360° back
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>

// =============================================================================
// HARDWARE CONFIGURATION
// =============================================================================

#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8
#define I2C_SCL_PIN      9
#define I2C_FREQ         100000

// =============================================================================
// ROLLERCAN REGISTERS
// =============================================================================

#define REG_OUTPUT       0x00
#define REG_MODE         0x01
#define REG_STALL_PROT   0x0F
#define REG_SPEED        0x40
#define REG_SPEED_MAXCUR 0x50
#define REG_POS_READ     0x90
#define REG_POS_TARGET   0xA0
#define REG_POS_MAXCUR   0xB0
#define REG_POS_MAXSPD   0xC0

#define MODE_SPEED       1
#define MODE_POSITION    2

// =============================================================================
// MOTOR CONFIGURATION
// =============================================================================

const int32_t MAX_CURRENT = 500000;     // Maximum holding current (5A)
const int32_t INIT_SPEED = 500;         // Init rotation speed
const int32_t GENTLE_SPEED = 50;        // Gentle rotation speed
const int32_t STEPS_PER_REVOLUTION = 4000; // Approximate steps for 360°

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

void showStatus();

// =============================================================================
// MOTOR I2C FUNCTIONS
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

void motorInit() {
  writeReg8(REG_OUTPUT, 0);
  delay(50);
  writeReg8(REG_STALL_PROT, 0);
  delay(50);
  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);
  writeReg32(REG_SPEED_MAXCUR, MAX_CURRENT);
  delay(50);
  writeReg32(REG_SPEED, 0);
  delay(50);
  writeReg8(REG_OUTPUT, 1);
  delay(50);
}

void motorSetSpeed(int32_t speed) {
  writeReg32(REG_SPEED, speed);
}

void motorSetMode(uint8_t mode) {
  writeReg8(REG_MODE, mode);
  delay(50);
}

void motorSetPositionTarget(int32_t target) {
  writeReg32(REG_POS_TARGET, target);
}

void motorConfigurePositionMode() {
  motorSetMode(MODE_POSITION);
  writeReg32(REG_POS_MAXCUR, MAX_CURRENT);  // Max holding current
  writeReg32(REG_POS_MAXSPD, 200);          // Max speed for position moves
  delay(50);
}

void motorConfigureSpeedMode() {
  motorSetMode(MODE_SPEED);
  writeReg32(REG_SPEED_MAXCUR, MAX_CURRENT);
  delay(50);
}

void motorEnable() {
  writeReg8(REG_OUTPUT, 1);
}

void motorDisable() {
  writeReg8(REG_OUTPUT, 0);
}

int32_t motorGetPosition() {
  return readReg32(REG_POS_READ);
}

// =============================================================================
// MOVEMENT FUNCTIONS
// =============================================================================

int32_t holdTargetPosition = 0;  // Global variable for hold position

void initRotation() {
  Serial.println("\n🔄 INITIALIZATION: 2 full rotations (CCW)");

  int32_t startPos = motorGetPosition();
  int32_t targetPos = startPos - (STEPS_PER_REVOLUTION * 2); // CCW = negative

  motorSetSpeed(-INIT_SPEED); // CCW = negative speed

  while (motorGetPosition() > targetPos) {
    delay(100);
  }

  motorSetSpeed(0);
  Serial.println("✓ Initialization complete");
}

void holdPosition() {
  Serial.println("\n🔒 HOLDING: Active holding with feedback");

  // Make sure motor is enabled
  motorEnable();
  delay(50);

  // Switch to speed mode with max current
  motorConfigureSpeedMode();

  // Store target position
  holdTargetPosition = motorGetPosition();
  Serial.printf("  Target position: %d\n", holdTargetPosition);

  Serial.println("  Motor will actively resist movement");
}

void gentleRotation(int32_t direction) {
  Serial.printf("\n↻ Gentle rotation: %s (360°)\n", direction > 0 ? "Forward" : "Backward");

  // Switch to speed mode for smooth rotation
  motorConfigureSpeedMode();

  int32_t startPos = motorGetPosition();
  int32_t targetPos = startPos + (STEPS_PER_REVOLUTION * direction);

  motorSetSpeed(GENTLE_SPEED * direction);

  if (direction > 0) {
    while (motorGetPosition() < targetPos) {
      delay(100);
    }
  } else {
    while (motorGetPosition() > targetPos) {
      delay(100);
    }
  }

  motorSetSpeed(0);
  Serial.println("✓ Rotation complete");
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n");
  Serial.println("============================================");
  Serial.println("  HEART SCULPTURE - SIMPLE ROTATION");
  Serial.println("============================================\n");

  // Initialize I2C
  Serial.print("Initializing I2C... ");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);
  delay(100);
  Serial.println("✓");

  // Initialize motor
  Serial.print("Initializing motor... ");
  motorInit();
  Serial.println("✓");

  Serial.println("\n✓ System ready!\n");

  // Show initial status and commands
  showStatus();
}

// =============================================================================
// STATE MANAGEMENT
// =============================================================================

enum State {
  IDLE,
  HOLDING,
  LOOPING
};

State currentState = IDLE;

// =============================================================================
// STATUS DISPLAY
// =============================================================================

void showStatus() {
  Serial.println("\n============================================");
  Serial.println("  HEART SCULPTURE STATUS");
  Serial.println("============================================");

  Serial.print("State: ");
  switch(currentState) {
    case IDLE:
      Serial.println("IDLE");
      break;
    case HOLDING:
      Serial.println("HOLDING (Maximum Power)");
      break;
    case LOOPING:
      Serial.println("LOOPING (Gentle Rotation)");
      break;
  }

  Serial.printf("Motor Position: %d\n", motorGetPosition());
  Serial.println("\nCommands:");
  Serial.println("  i - Initialize (2 rotations)");
  Serial.println("  h - Hold position (maximum power)");
  Serial.println("  l - Start gentle rotation loop");
  Serial.println("  s - Stop / Go to IDLE");
  Serial.println("  d - Disable motor (no power)");
  Serial.println("  ? - Show this status");
  Serial.println("============================================\n");
}

// =============================================================================
// COMMAND HANDLER
// =============================================================================

void handleCommand(char cmd) {
  switch(cmd) {
    case 'i':
    case 'I':
      Serial.println("\n>>> Command: INITIALIZE");
      currentState = IDLE;
      initRotation();
      holdPosition();
      currentState = HOLDING;
      showStatus();
      break;

    case 'h':
    case 'H':
      Serial.println("\n>>> Command: HOLD");
      holdPosition();
      currentState = HOLDING;
      showStatus();
      break;

    case 'l':
    case 'L':
      Serial.println("\n>>> Command: START LOOP");
      motorEnable();
      delay(50);
      motorConfigureSpeedMode();
      currentState = LOOPING;
      Serial.println("✓ Starting gentle rotation loop");
      showStatus();
      break;

    case 's':
    case 'S':
      Serial.println("\n>>> Command: STOP");
      motorSetSpeed(0);
      currentState = IDLE;
      Serial.println("✓ Stopped - Motor idle");
      showStatus();
      break;

    case 'd':
    case 'D':
      Serial.println("\n>>> Command: DISABLE MOTOR");
      motorSetSpeed(0);
      motorDisable();
      currentState = IDLE;
      Serial.println("✓ Motor disabled - no power");
      showStatus();
      break;

    case '?':
      showStatus();
      break;

    default:
      // Ignore unknown commands
      break;
  }
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  // Check for serial commands
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    handleCommand(cmd);
  }

  // Execute behavior based on state
  if (currentState == HOLDING) {
    // Active holding - constantly correct position
    int32_t currentPos = motorGetPosition();
    int32_t error = holdTargetPosition - currentPos;

    // Apply corrective force proportional to error with larger deadband
    if (abs(error) > 50) { // Larger deadband to reduce trembling
      int32_t holdSpeed = constrain(error * 2, -500, 500); // Lower gain for smoother control
      motorSetSpeed(holdSpeed);
    } else {
      motorSetSpeed(0);
    }
    delay(20); // Slower update rate

  } else if (currentState == LOOPING) {
    // Gentle rotation forward
    gentleRotation(1);
    delay(1000);

    // Gentle rotation backward
    gentleRotation(-1);
    delay(1000);

  } else {
    delay(10);
  }
}
