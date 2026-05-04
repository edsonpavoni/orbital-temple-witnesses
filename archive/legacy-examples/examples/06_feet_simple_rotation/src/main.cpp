/*
 * ============================================================================
 * ORBITAL TEMPLE - FEET SCULPTURE
 * Revolution-Wait-Revolution Pattern
 * ============================================================================
 *
 * DESCRIPTION:
 * ------------
 * Performs one complete revolution with smooth ease in/out motion,
 * then waits for a specified time before the next revolution.
 *
 * SERIAL COMMANDS:
 * ----------------
 * r=<seconds>   - Set rotation duration (e.g., r=30)
 * w=<seconds>   - Set wait time between revolutions (e.g., w=30)
 * g             - GO! Trigger a new revolution immediately
 * s             - Show current settings
 * p             - Pause/Resume rotation
 * minspeed=<n>  - Set minimum speed (default 50, prevents clunky motion)
 * maxcur=<n>    - Set max current in mA (default 1000)
 * easing=<n>    - Set easing intensity 1-10 (default 5)
 *
 * HARDWARE:
 * ---------
 * - Seeed XIAO ESP32-S3
 * - M5Stack Unit RollerCAN (I2C)
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>

// =============================================================================
// CONFIGURATION - ADJUSTABLE VIA SERIAL MONITOR
// =============================================================================

float rotationDuration = 10.0;       // Seconds for one complete SCULPTURE revolution
float waitTime = 30.0;               // Seconds to wait between revolutions
float gearRatio = 3.2;               // Gear reduction ratio (motor revs : sculpture revs)
                                     // e.g., 3.2 means motor does 3.2 revs for 1 sculpture rev
int32_t minSpeed = 10;               // Minimum speed (prevents clunky motion at very low speeds)
int32_t maxCurrent = 100000;         // Max current (in units of 0.01mA, so 100000 = 1000mA)
float easingIntensity = 10.0;        // Easing intensity (1=gentle, 10=aggressive)
bool isPaused = false;               // Pause/resume flag

// State machine
enum State {
  STATE_ROTATING,
  STATE_WAITING,
  STATE_STOPPED
};

State currentState = STATE_WAITING;
unsigned long stateStartTime = 0;

// =============================================================================
// HARDWARE CONFIGURATION
// =============================================================================

#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8          // XIAO D9
#define I2C_SCL_PIN      9          // XIAO D10
#define I2C_FREQ         100000

// =============================================================================
// ROLLERCAN REGISTER ADDRESSES
// =============================================================================

#define REG_OUTPUT       0x00
#define REG_MODE         0x01
#define REG_STALL_PROT   0x0F
#define REG_SPEED        0x40
#define REG_SPEED_MAXCUR 0x50
#define REG_SPEED_READ   0x60
#define REG_POS_READ     0x90

#define MODE_SPEED       1
#define MODE_POSITION    2

// =============================================================================
// EASING FUNCTIONS
// =============================================================================

// Ease in-out cubic function (smooth acceleration and deceleration)
float easeInOutCubic(float t) {
  if (t < 0.5) {
    return 4 * t * t * t;
  } else {
    float f = (2 * t - 2);
    return 1 + f * f * f / 2;
  }
}

// Get derivative of easing function (velocity)
float easeInOutCubicDerivative(float t) {
  if (t < 0.5) {
    return 12 * t * t;
  } else {
    float f = (2 * t - 2);
    return 1.5 * f * f;
  }
}

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
  // Disable motor first
  writeReg8(REG_OUTPUT, 0);
  delay(50);

  // Disable stall protection
  writeReg8(REG_STALL_PROT, 0);
  delay(50);

  // Set to speed control mode
  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);

  // Set max current (adjustable via serial)
  writeReg32(REG_SPEED_MAXCUR, maxCurrent);
  delay(50);

  // Set initial speed to 0
  writeReg32(REG_SPEED, 0);
  delay(50);

  // Enable motor
  writeReg8(REG_OUTPUT, 1);
  delay(50);
}

void motorUpdateMaxCurrent() {
  writeReg32(REG_SPEED_MAXCUR, maxCurrent);
}

void motorSetSpeed(int32_t speed) {
  writeReg32(REG_SPEED, speed);
}

void motorEnable(bool enable) {
  writeReg8(REG_OUTPUT, enable ? 1 : 0);
}

int32_t motorGetSpeed() {
  return readReg32(REG_SPEED_READ);
}

int32_t motorGetPosition() {
  return readReg32(REG_POS_READ);
}

// =============================================================================
// SERIAL COMMAND PROCESSING
// =============================================================================

void printSettings() {
  Serial.println("\n========================================");
  Serial.println("        FEET SCULPTURE STATUS");
  Serial.println("========================================");

  // Current State
  Serial.print("State: ");
  switch(currentState) {
    case STATE_ROTATING: Serial.println("ROTATING"); break;
    case STATE_WAITING: Serial.println("WAITING"); break;
    case STATE_STOPPED: Serial.println("STOPPED"); break;
  }

  // Progress info
  if (currentState == STATE_ROTATING) {
    unsigned long elapsed = millis() - stateStartTime;
    float cycleDuration = rotationDuration * 1000.0;
    float progress = (float)elapsed / cycleDuration * 100.0;
    Serial.print("Progress: ");
    Serial.print(progress, 1);
    Serial.println("%");
  } else if (currentState == STATE_WAITING) {
    unsigned long elapsed = millis() - stateStartTime;
    float waitDuration = waitTime * 1000.0;
    float remaining = (waitDuration - elapsed) / 1000.0;
    if (remaining > 0) {
      Serial.print("Time Until Next: ");
      Serial.print(remaining, 1);
      Serial.println("s");
    }
  }

  Serial.println("\n--- CURRENT SETTINGS ---");
  Serial.print("Rotation Duration: ");
  Serial.print(rotationDuration);
  Serial.println(" seconds");

  Serial.print("Wait Time: ");
  Serial.print(waitTime);
  Serial.println(" seconds");

  Serial.print("Gear Ratio: ");
  Serial.print(gearRatio);
  Serial.println(":1");

  Serial.print("Min Speed: ");
  Serial.print(minSpeed);
  Serial.print(" (");
  Serial.print(minSpeed / 100.0);
  Serial.println(" RPM)");

  Serial.print("Max Current: ");
  Serial.print(maxCurrent / 100.0);
  Serial.println(" mA");

  Serial.print("Easing: ");
  Serial.println(easingIntensity);

  Serial.println("\n--- COMMANDS ---");
  Serial.println("  g         - GO! Start revolution");
  Serial.println("  x         - ABORT! Stop everything");
  Serial.println("  r<n>      - Set rotation duration (r30)");
  Serial.println("  w<n>      - Set wait time (w30)");
  Serial.println("  t<n>      - Set gear raTio (t3.2)");
  Serial.println("  m<n>      - Set min speed (m50)");
  Serial.println("  c<n>      - Set max current mA (c1000)");
  Serial.println("  e<n>      - Set easing 1-10 (e5)");
  Serial.println("  s         - Show this status");
  Serial.println("========================================\n");
}

void startRotation() {
  currentState = STATE_ROTATING;
  stateStartTime = millis();
}

void abortAll() {
  currentState = STATE_STOPPED;
  isPaused = true;
  motorSetSpeed(0);
  Serial.println("ABORTED - All stopped");
}

void processSerialCommand() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();

    if (command == "g") {
      // GO! Start revolution immediately
      isPaused = false;
      startRotation();
    }
    else if (command == "x") {
      // ABORT! Stop everything
      abortAll();
    }
    else if (command.startsWith("r")) {
      // Set rotation duration (r30)
      float newDuration = command.substring(1).toFloat();
      if (newDuration > 0 && newDuration <= 300) {
        rotationDuration = newDuration;
        Serial.print("Rotation: ");
        Serial.print(rotationDuration);
        Serial.println("s");
      } else {
        Serial.println("ERROR: 0-300");
      }
    }
    else if (command.startsWith("w")) {
      // Set wait time (w30)
      float newWait = command.substring(1).toFloat();
      if (newWait >= 0 && newWait <= 600) {
        waitTime = newWait;
        Serial.print("Wait: ");
        Serial.print(waitTime);
        Serial.println("s");
      } else {
        Serial.println("ERROR: 0-600");
      }
    }
    else if (command.startsWith("t")) {
      // Set gear ratio (t3.2)
      float newRatio = command.substring(1).toFloat();
      if (newRatio >= 0.1 && newRatio <= 20.0) {
        gearRatio = newRatio;
        Serial.print("Gear Ratio: ");
        Serial.print(gearRatio);
        Serial.println(":1");
      } else {
        Serial.println("ERROR: 0.1-20");
      }
    }
    else if (command.startsWith("m")) {
      // Set minimum speed (m50)
      int32_t newMinSpeed = command.substring(1).toInt();
      if (newMinSpeed >= 0 && newMinSpeed <= 1000) {
        minSpeed = newMinSpeed;
        Serial.print("MinSpeed: ");
        Serial.print(minSpeed);
        Serial.print(" (");
        Serial.print(minSpeed / 100.0);
        Serial.println(" RPM)");
      } else {
        Serial.println("ERROR: 0-1000");
      }
    }
    else if (command.startsWith("c")) {
      // Set max current (c1000)
      int32_t newMaxCurrent = command.substring(1).toInt();
      if (newMaxCurrent >= 100 && newMaxCurrent <= 3000) {
        maxCurrent = newMaxCurrent * 100;
        motorUpdateMaxCurrent();
        Serial.print("MaxCurrent: ");
        Serial.print(newMaxCurrent);
        Serial.println("mA");
      } else {
        Serial.println("ERROR: 100-3000");
      }
    }
    else if (command.startsWith("e")) {
      // Set easing intensity (e5)
      float newEasing = command.substring(1).toFloat();
      if (newEasing >= 1.0 && newEasing <= 10.0) {
        easingIntensity = newEasing;
        Serial.print("Easing: ");
        Serial.println(easingIntensity);
      } else {
        Serial.println("ERROR: 1-10");
      }
    }
    else if (command == "s") {
      // Show settings
      printSettings();
    }
    else if (command.length() > 0) {
      Serial.println("Unknown. Type 's' for help");
    }
  }
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\nFEET SCULPTURE READY");
  Serial.println("Type 's' for commands\n");

  // Initialize I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  // Check motor connection
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: Motor not found!");
    while (1) delay(1000);
  }

  // Initialize motor
  motorInit();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

unsigned long lastUpdate = 0;

void loop() {
  // Process serial commands
  processSerialCommand();

  // Update state machine every 50ms
  if (millis() - lastUpdate >= 50) {
    lastUpdate = millis();

    unsigned long elapsed = millis() - stateStartTime;

    switch (currentState) {
      case STATE_ROTATING: {
        // Calculate position in current rotation (0.0 to 1.0)
        float cycleDuration = rotationDuration * 1000.0; // Convert to milliseconds

        if (elapsed >= cycleDuration) {
          // Revolution complete! Move to waiting state
          motorSetSpeed(0);
          currentState = STATE_WAITING;
          stateStartTime = millis();
        } else {
          float progress = (float)elapsed / cycleDuration;

          // Get velocity from easing derivative
          float velocity = easeInOutCubicDerivative(progress);

          // Apply easing intensity multiplier
          velocity = pow(velocity, 1.0 / easingIntensity);

          // Calculate RPM needed for one SCULPTURE revolution in rotationDuration seconds
          // Must multiply by gear ratio to get motor RPM
          float sculptureRPM = 60.0 / rotationDuration;  // RPM of sculpture
          float motorRPM = sculptureRPM * gearRatio;     // RPM of motor (accounts for reduction)
          int32_t baseSpeed = (int32_t)(motorRPM * 100.0);

          // Apply velocity scaling
          int32_t currentSpeed = (int32_t)(baseSpeed * velocity);

          // Apply minimum speed threshold to prevent clunky motion
          if (currentSpeed < minSpeed && currentSpeed > 0) {
            currentSpeed = minSpeed;
          }

          // Set motor speed
          motorSetSpeed(currentSpeed);
        }
        break;
      }

      case STATE_WAITING: {
        // Wait for specified time
        motorSetSpeed(0);

        float waitDuration = waitTime * 1000.0;
        if (elapsed >= waitDuration) {
          // Wait complete! Start new revolution
          startRotation();
        }
        break;
      }

      case STATE_STOPPED: {
        // Stopped/paused - do nothing
        motorSetSpeed(0);
        break;
      }
    }
  }

  delay(10);
}
