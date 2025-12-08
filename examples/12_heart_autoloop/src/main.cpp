/*
 * ============================================================================
 * HEART SCULPTURE - AUTO LOOP
 * ============================================================================
 *
 * BEHAVIOR:
 * 1. Wait 3 seconds on startup
 * 2. Automatically start looping
 * 3. Loop continuously with adjustable parameters via serial
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
const int32_t STEPS_PER_REVOLUTION = 16000; // Steps for 360° (calibrated)
const int32_t STEPS_HALF_REVOLUTION = 8000; // Half revolution (180°)

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

void showStatus();
void handleCommand(char cmd);

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
// STATE MANAGEMENT
// =============================================================================

enum State {
  IDLE,
  HOLDING,
  LOOPING
};

State currentState = IDLE;

// =============================================================================
// MOVEMENT FUNCTIONS
// =============================================================================

int32_t holdTargetPosition = 0;
bool emergencyStop = false;
int32_t loopStartPosition = 0;
int32_t loopDistance = STEPS_PER_REVOLUTION * 2;  // Default: 720° (2 full rotations)
int32_t loopPauseTime = 0;  // Default: 0ms (no pause)

void holdPosition() {
  Serial.println("\n🔒 HOLDING: Active holding with feedback");
  motorEnable();
  delay(50);
  motorConfigureSpeedMode();
  holdTargetPosition = motorGetPosition();
  loopStartPosition = 0;
  Serial.printf("  Target position: %d\n", holdTargetPosition);
  Serial.println("  Loop counter reset to 0");
  Serial.println("  Motor will actively resist movement");
}

void gentleRotation(int32_t direction) {
  int32_t degrees = (loopDistance * 360) / STEPS_PER_REVOLUTION;
  Serial.printf("\n⚡ BURST ROTATION: %s (%d°) - MAXIMUM FORCE\n",
                direction > 0 ? "Forward" : "Backward", degrees);

  motorConfigureSpeedMode();

  int32_t currentAbsolutePos = motorGetPosition();
  int32_t startPos = currentAbsolutePos - loopStartPosition;
  int32_t targetPos = startPos + (loopDistance * direction);

  Serial.printf("Start: %d | Target: %d (relative to loop start)\n", startPos, targetPos);

  while (true) {
    // Check for emergency stop
    if (Serial.available() > 0) {
      char cmd = Serial.read();
      if (cmd == 'e' || cmd == 'E') {
        emergencyStop = true;
        motorSetSpeed(0);
        motorDisable();
        currentState = IDLE;
        Serial.println("\n>>> ⚠️  EMERGENCY STOP ⚠️");
        return;
      }
    }

    if (emergencyStop) {
      motorSetSpeed(0);
      return;
    }

    int32_t currentAbsPos = motorGetPosition();
    int32_t relativePos = currentAbsPos - loopStartPosition;

    // Check if reached target
    if ((direction > 0 && relativePos >= targetPos) ||
        (direction < 0 && relativePos <= targetPos)) {
      break;
    }

    // APPLY MAXIMUM BURST FORCE
    motorSetSpeed(800 * direction);
    delay(10);
  }

  motorSetSpeed(0);
  Serial.printf("✓ Rotation complete! Final position: %d\n", motorGetPosition() - loopStartPosition);
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n");
  Serial.println("============================================");
  Serial.println("  HEART SCULPTURE - AUTO LOOP");
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
  showStatus();

  // Wait 3 seconds
  Serial.println("\n⏱️  Waiting 3 seconds before auto-start...");
  for (int i = 3; i > 0; i--) {
    Serial.printf("  %d...\n", i);
    delay(1000);
  }

  // Auto-start loop
  Serial.println("\n🚀 AUTO-STARTING LOOP!");
  emergencyStop = false;
  motorEnable();
  delay(50);
  motorConfigureSpeedMode();
  loopStartPosition = motorGetPosition();
  Serial.printf("  Loop start position set to: %d\n", loopStartPosition);
  currentState = LOOPING;
  Serial.println("  Press 'e' for emergency stop");
  Serial.println("  Press '?' for status and commands\n");
}

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
      Serial.println("LOOPING (Auto Mode)");
      break;
  }

  Serial.printf("Motor Position: %d\n", motorGetPosition());
  int32_t degrees = (loopDistance * 360) / STEPS_PER_REVOLUTION;
  Serial.printf("Loop Distance: %d° (%d steps)\n", degrees, loopDistance);
  Serial.printf("Loop Pause Time: %dms (between movements)\n", loopPauseTime);

  Serial.println("\nCommands:");
  Serial.println("  h - Hold position (maximum power)");
  Serial.println("  l - Start loop");
  Serial.println("  s - Stop / Go to IDLE");
  Serial.println("  d - Disable motor (no power)");
  Serial.println("  e - EMERGENCY STOP (stop all movement immediately)");
  Serial.println("  [ - Decrease loop distance (-45°)");
  Serial.println("  ] - Increase loop distance (+45°)");
  Serial.println("  , - Decrease pause time (-100ms)");
  Serial.println("  . - Increase pause time (+100ms)");
  Serial.println("  ? - Show this status");
  Serial.println("============================================\n");
}

// =============================================================================
// COMMAND HANDLER
// =============================================================================

void handleCommand(char cmd) {
  switch(cmd) {
    case 'h':
    case 'H':
      Serial.println("\n>>> Command: HOLD");
      emergencyStop = false;
      holdPosition();
      currentState = HOLDING;
      showStatus();
      break;

    case 'l':
    case 'L':
      Serial.println("\n>>> Command: START LOOP");
      emergencyStop = false;
      motorEnable();
      delay(50);
      motorConfigureSpeedMode();
      loopStartPosition = motorGetPosition();
      Serial.printf("  Loop start position set to: %d\n", loopStartPosition);
      currentState = LOOPING;
      Serial.println("✓ Starting burst rotation loop");
      showStatus();
      break;

    case 's':
    case 'S':
      Serial.println("\n>>> Command: STOP");
      emergencyStop = false;
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
      emergencyStop = false;
      Serial.println("✓ Motor disabled - no power");
      showStatus();
      break;

    case 'e':
    case 'E':
      Serial.println("\n>>> ⚠️  EMERGENCY STOP ⚠️");
      emergencyStop = true;
      motorSetSpeed(0);
      motorDisable();
      currentState = IDLE;
      Serial.println("✓ Motor stopped and disabled immediately");
      Serial.println("  Press 'h' to hold or 'l' to loop again");
      showStatus();
      break;

    case '?':
      showStatus();
      break;

    case '[':
    case '{':
      {
        int32_t stepChange = STEPS_PER_REVOLUTION / 8; // 45° increments
        loopDistance = constrain(loopDistance - stepChange, stepChange, STEPS_PER_REVOLUTION * 4);
        int32_t degrees = (loopDistance * 360) / STEPS_PER_REVOLUTION;
        Serial.printf("\n>>> Loop distance decreased to: %d° (%d steps)\n", degrees, loopDistance);
        showStatus();
      }
      break;

    case ']':
    case '}':
      {
        int32_t stepChange = STEPS_PER_REVOLUTION / 8; // 45° increments
        loopDistance = constrain(loopDistance + stepChange, stepChange, STEPS_PER_REVOLUTION * 4);
        int32_t degrees = (loopDistance * 360) / STEPS_PER_REVOLUTION;
        Serial.printf("\n>>> Loop distance increased to: %d° (%d steps)\n", degrees, loopDistance);
        showStatus();
      }
      break;

    case ',':
    case '<':
      loopPauseTime = constrain(loopPauseTime - 100, 0, 10000);
      Serial.printf("\n>>> Loop pause time decreased to: %dms\n", loopPauseTime);
      showStatus();
      break;

    case '.':
    case '>':
      loopPauseTime = constrain(loopPauseTime + 100, 0, 10000);
      Serial.printf("\n>>> Loop pause time increased to: %dms\n", loopPauseTime);
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

    if (abs(error) > 10) {
      int32_t holdSpeed = constrain(error * 5, -800, 800);
      motorSetSpeed(holdSpeed);
    } else {
      motorSetSpeed(0);
    }
    delay(15);

  } else if (currentState == LOOPING) {
    // Gentle rotation forward
    gentleRotation(1);

    if (emergencyStop || currentState != LOOPING) {
      return;
    }

    // Hold after rotation (with adjustable pause time)
    if (loopPauseTime > 0) {
      holdPosition();
      unsigned long holdStart = millis();
      while (millis() - holdStart < loopPauseTime && currentState == LOOPING && !emergencyStop) {
        int32_t currentPos = motorGetPosition();
        int32_t error = holdTargetPosition - currentPos;
        if (abs(error) > 10) {
          int32_t holdSpeed = constrain(error * 5, -800, 800);
          motorSetSpeed(holdSpeed);
        } else {
          motorSetSpeed(0);
        }
        delay(15);

        // Check for serial commands during hold
        if (Serial.available() > 0) {
          char cmd = Serial.read();
          handleCommand(cmd);
          if (currentState != LOOPING || emergencyStop) break;
        }
      }
    }

    if (emergencyStop || currentState != LOOPING) {
      return;
    }

    // Gentle rotation backward
    if (currentState == LOOPING && !emergencyStop) {
      gentleRotation(-1);

      if (emergencyStop || currentState != LOOPING) {
        return;
      }

      // Hold after rotation (with adjustable pause time)
      if (loopPauseTime > 0) {
        holdPosition();
        unsigned long holdStart = millis();
        while (millis() - holdStart < loopPauseTime && currentState == LOOPING && !emergencyStop) {
          int32_t currentPos = motorGetPosition();
          int32_t error = holdTargetPosition - currentPos;
          if (abs(error) > 10) {
            int32_t holdSpeed = constrain(error * 5, -800, 800);
            motorSetSpeed(holdSpeed);
          } else {
            motorSetSpeed(0);
          }
          delay(15);

          // Check for serial commands during hold
          if (Serial.available() > 0) {
            char cmd = Serial.read();
            handleCommand(cmd);
            if (currentState != LOOPING || emergencyStop) break;
          }
        }
      }
    }

  } else {
    delay(10);
  }
}
