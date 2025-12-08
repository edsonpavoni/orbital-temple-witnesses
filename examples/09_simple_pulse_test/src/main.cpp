/*
 * ============================================================================
 * ORBITAL TEMPLE - Simple Pulse Test
 * ============================================================================
 *
 * DESCRIPTION:
 * ------------
 * Very simple test for single pulse movement.
 * Type 'g' to execute one pulse:
 *   1. Motor disable
 *   2. Rotate CCW 180° with ease in/out
 *   3. Rotate CW 180° with ease in/out
 *   4. Motor disable
 *
 * No looping. Just one pulse per 'g' command.
 *
 * SERIAL COMMANDS:
 * ----------------
 * g         - GO! Execute one pulse
 * x         - STOP immediately
 * d<n>      - Set duration for each 180° rotation (d2)
 * s         - Show settings
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

float rotationDuration = 2.0;        // Seconds for rotation
float angle = 180.0;                  // Rotation angle (0-360 degrees)
bool directionCCW = true;             // true = CCW, false = CW
float gearRatio = 1.0;                // Direct drive (1:1)
int32_t minSpeed = 100;               // Minimum speed (prevents stalling)
int32_t maxSpeed = 10000;             // Maximum speed limit (100 RPM)
int32_t maxCurrent = 250000;          // Max current (2500mA for more torque)
float easingIntensity = 10.0;         // Easing (1=gentle, 10=max smooth)
bool useEasing = true;                // Enable/disable easing

// State machine

enum State {
  STATE_IDLE,
  STATE_INIT,          // Moving to angle 1 (no easing)
  STATE_AT_ANGLE1,     // Waiting at angle 1
  STATE_LOOP_TO_2,     // Loop: Moving to angle 2 (with easing)
  STATE_LOOP_TO_3,     // Loop: Moving to angle 3 (with easing)
  STATE_STOPPING       // Returning to 0 and disabling
};

State currentState = STATE_IDLE;
unsigned long stateStartTime = 0;
float currentPosition = 0.0;     // Track current position
float workingZero = 0.0;         // Working zero recorded after init

// Loop angles (adjustable)
float loopAngle1 = 180.0;        // Init angle (absolute from 0)
float loopAngle2 = 90.0;         // Loop angle 2 (offset from workingZero)
float loopAngle3 = 0.0;          // Loop angle 3 (offset from workingZero, usually 0)

// Loop durations
float initDuration = 0.5;        // Duration to move to angle 1
float loopDuration = 2.0;        // Duration for loop movements
float stopDuration = 2.0;        // Duration to return to 0
int32_t holdSpeed = 100;         // Speed to hold position

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

#define MODE_SPEED       1

// =============================================================================
// EASING FUNCTIONS
// =============================================================================

float easeInOutCubicDerivative(float t) {
  if (t < 0.5) {
    return 12 * t * t;
  } else {
    float f = (2 * t - 2);
    return 1.5 * f * f;
  }
}

// =============================================================================
// MOTOR FUNCTIONS
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

void motorInit() {
  writeReg8(REG_OUTPUT, 0);
  delay(50);
  writeReg8(REG_STALL_PROT, 0);
  delay(50);
  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);
  writeReg32(REG_SPEED_MAXCUR, maxCurrent);
  delay(50);
  writeReg32(REG_SPEED, 0);
  delay(50);
  writeReg8(REG_OUTPUT, 1);
  delay(50);
}

void motorSetSpeed(int32_t speed) {
  writeReg32(REG_SPEED, speed);
}

void motorUpdateMaxCurrent() {
  writeReg32(REG_SPEED_MAXCUR, maxCurrent);
}

void motorEnable(bool enable) {
  writeReg8(REG_OUTPUT, enable ? 1 : 0);
}

// =============================================================================
// STATE FUNCTIONS
// =============================================================================

void startInit() {
  Serial.println("\n*** INIT: Moving to angle 1 ***");
  Serial.print("Target: ");
  Serial.print(loopAngle1);
  Serial.println("°");

  motorEnable(false);
  delay(100);
  motorEnable(true);

  currentPosition = 0.0;
  currentState = STATE_INIT;
  stateStartTime = millis();
}

void startLoop() {
  Serial.println("\n*** LOOP: Starting loop between angle 2 and 3 ***");

  if (currentState == STATE_IDLE) {
    Serial.println("ERROR: Must init first (press 'i')");
    return;
  }

  currentState = STATE_LOOP_TO_2;
  stateStartTime = millis();
}

void stopMotor() {
  Serial.println("\n*** STOP: Disabling motor ***");
  motorSetSpeed(0);
  delay(50);
  motorEnable(false);
  currentState = STATE_IDLE;
  currentPosition = 0;
  workingZero = 0;
  Serial.println("Motor disabled");
}

// =============================================================================
// SERIAL COMMANDS
// =============================================================================

void printSettings() {
  Serial.println("\n========================================");
  Serial.println("      MOTOR CONTROL TEST");
  Serial.println("========================================");
  Serial.print("State: ");
  switch(currentState) {
    case STATE_IDLE: Serial.println("IDLE"); break;
    case STATE_INIT: Serial.println("INIT"); break;
    case STATE_AT_ANGLE1: Serial.println("AT ANGLE 1 (ready for loop)"); break;
    case STATE_LOOP_TO_2: Serial.println("LOOP TO 2"); break;
    case STATE_LOOP_TO_3: Serial.println("LOOP TO 3"); break;
  }

  Serial.print("Current Position: ");
  Serial.print(currentPosition);
  Serial.println("°");
  Serial.print("Working Zero: ");
  Serial.print(workingZero);
  Serial.println("°");

  Serial.println("\n--- INIT SETTINGS ---");
  Serial.print("Init Angle: ");
  Serial.print(loopAngle1);
  Serial.println("° (absolute)");
  Serial.print("Init Duration: ");
  Serial.print(initDuration);
  Serial.println("s");

  Serial.println("\n--- LOOP ANGLES ---");
  Serial.print("Angle 2 (offset from WZ): ");
  Serial.print(loopAngle2);
  Serial.print("° (actual: ");
  Serial.print(workingZero + loopAngle2);
  Serial.println("°)");
  Serial.print("Angle 3 (offset from WZ): ");
  Serial.print(loopAngle3);
  Serial.print("° (actual: ");
  Serial.print(workingZero + loopAngle3);
  Serial.println("°)");

  Serial.println("\n--- LOOP SETTINGS ---");
  Serial.print("Loop Duration: ");
  Serial.print(loopDuration);
  Serial.println("s");
  Serial.print("Easing Intensity: ");
  Serial.println(easingIntensity);

  Serial.println("\n--- MOTOR PARAMETERS ---");
  Serial.print("Min Speed: ");
  Serial.print(minSpeed);
  Serial.print(" (");
  Serial.print(minSpeed / 100.0);
  Serial.println(" RPM)");
  Serial.print("Max Speed: ");
  Serial.print(maxSpeed);
  Serial.print(" (");
  Serial.print(maxSpeed / 100.0);
  Serial.println(" RPM)");
  Serial.print("Max Current: ");
  Serial.print(maxCurrent / 100.0);
  Serial.println(" mA");
  Serial.print("Gear Ratio: ");
  Serial.print(gearRatio);
  Serial.println(":1");

  Serial.println("\n--- COMMANDS ---");
  Serial.println("CONTROL:");
  Serial.println("  i          - INIT: Move to angle 1, set working zero");
  Serial.println("  l          - LOOP: Start infinite loop (WZ+L2 <-> WZ+L3)");
  Serial.println("  x          - STOP: Disable motor immediately");
  Serial.println("\nINIT SETTINGS:");
  Serial.println("  ia<n>      - Init angle (absolute, ia180)");
  Serial.println("  id<n>      - Init duration in seconds (id0.5)");
  Serial.println("\nLOOP ANGLES:");
  Serial.println("  l2<n>      - Angle 2: offset from WZ (l290)");
  Serial.println("  l3<n>      - Angle 3: offset from WZ (l30)");
  Serial.println("\nMOTOR TUNING:");
  Serial.println("  c<n>       - Max current mA (c2500)");
  Serial.println("  min<n>     - Min speed (min100)");
  Serial.println("  max<n>     - Max speed (max10000)");
  Serial.println("  e<n>       - Easing intensity 1-10 (e10)");
  Serial.println("\nINFO:");
  Serial.println("  s          - Show settings");
  Serial.println("========================================\n");
}

void processSerialCommand() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();

    if (command == "i") {
      if (currentState == STATE_IDLE) {
        startInit();
      } else {
        Serial.println("Already running!");
      }
    }
    else if (command == "l") {
      if (currentState == STATE_AT_ANGLE1 || currentState == STATE_IDLE) {
        startLoop();
      } else {
        Serial.println("Already looping or not ready!");
      }
    }
    else if (command == "x") {
      stopMotor();
    }
    else if (command.startsWith("c")) {
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
    else if (command.startsWith("min")) {
      int32_t newMinSpeed = command.substring(3).toInt();
      if (newMinSpeed >= 0 && newMinSpeed <= 50000) {
        minSpeed = newMinSpeed;
        Serial.print("MinSpeed: ");
        Serial.print(minSpeed);
        Serial.print(" (");
        Serial.print(minSpeed / 100.0);
        Serial.println(" RPM)");
      } else {
        Serial.println("ERROR: 0-50000");
      }
    }
    else if (command.startsWith("max")) {
      int32_t newMaxSpeed = command.substring(3).toInt();
      if (newMaxSpeed >= 0 && newMaxSpeed <= 50000) {
        maxSpeed = newMaxSpeed;
        Serial.print("MaxSpeed: ");
        Serial.print(maxSpeed);
        Serial.print(" (");
        Serial.print(maxSpeed / 100.0);
        Serial.println(" RPM)");
      } else {
        Serial.println("ERROR: 0-50000");
      }
    }
    else if (command.startsWith("e")) {
      float newEasing = command.substring(1).toFloat();
      if (newEasing >= 1.0 && newEasing <= 10.0) {
        easingIntensity = newEasing;
        Serial.print("Easing: ");
        Serial.println(easingIntensity);
      } else {
        Serial.println("ERROR: 1-10");
      }
    }
    else if (command.startsWith("ia")) {
      float newAngle = command.substring(2).toFloat();
      if (newAngle >= 0 && newAngle <= 3600) {
        loopAngle1 = newAngle;
        Serial.print("Init angle: ");
        Serial.print(loopAngle1);
        Serial.println("°");
      } else {
        Serial.println("ERROR: 0-3600");
      }
    }
    else if (command.startsWith("id")) {
      float newDuration = command.substring(2).toFloat();
      if (newDuration > 0 && newDuration <= 10) {
        initDuration = newDuration;
        Serial.print("Init duration: ");
        Serial.print(initDuration);
        Serial.println("s");
      } else {
        Serial.println("ERROR: 0-10");
      }
    }
    else if (command.startsWith("l2")) {
      float newAngle = command.substring(2).toFloat();
      if (newAngle >= 0 && newAngle <= 3600) {
        loopAngle2 = newAngle;
        Serial.print("Loop Phase 2&4 angle: ");
        Serial.print(loopAngle2);
        Serial.println("°");
      } else {
        Serial.println("ERROR: 0-3600");
      }
    }
    else if (command.startsWith("l3")) {
      float newAngle = command.substring(2).toFloat();
      if (newAngle >= 0 && newAngle <= 3600) {
        loopAngle3 = newAngle;
        Serial.print("Loop Phase 3&5 angle: ");
        Serial.print(loopAngle3);
        Serial.println("°");
      } else {
        Serial.println("ERROR: 0-3600");
      }
    }
    else if (command == "s") {
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

  Serial.println("\nSIMPLE PULSE TEST");
  Serial.println("Type 's' for commands\n");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: Motor not found!");
    while (1) delay(1000);
  }

  motorInit();
  Serial.println("Ready! Type 'g' to test pulse\n");
}

// =============================================================================
// MAIN LOOP
// =============================================================================

unsigned long lastUpdate = 0;

void loop() {
  processSerialCommand();

  if (millis() - lastUpdate >= 50) {
    lastUpdate = millis();

    unsigned long elapsed = millis() - stateStartTime;
    float cycleDuration = rotationDuration * 1000.0;

    switch (currentState) {
      case STATE_INIT: {
        // Move to angle 1 (no easing)
        float cycleDuration = initDuration * 1000.0;

        if (elapsed >= cycleDuration) {
          // Init complete - record working zero and wait at angle 1
          currentPosition = loopAngle1;
          workingZero = loopAngle1;
          currentState = STATE_AT_ANGLE1;
          motorSetSpeed(0);
          Serial.print("Init complete - Working zero set to ");
          Serial.print(workingZero);
          Serial.println("°. Press 'l' to start loop");
        } else {
          float progress = (float)elapsed / cycleDuration;
          float velocity = 1.0;  // No easing

          float angleToMove = loopAngle1;  // From 0 to loopAngle1
          float revolutions = abs(angleToMove) / 360.0;
          float sculptureRPM = (revolutions * 60.0) / initDuration;
          float motorRPM = sculptureRPM * gearRatio;
          int32_t baseSpeed = (int32_t)(motorRPM * 100.0);

          int32_t currentSpeed = (int32_t)(baseSpeed * velocity);

          if (currentSpeed < minSpeed && currentSpeed > 0) {
            currentSpeed = minSpeed;
          }
          if (currentSpeed > maxSpeed) {
            currentSpeed = maxSpeed;
          }

          // Always CCW for init (negative speed)
          currentSpeed = -currentSpeed;

          motorSetSpeed(currentSpeed);
        }
        break;
      }

      case STATE_AT_ANGLE1: {
        // Waiting at angle 1, holding position with minimal CCW speed
        motorSetSpeed(-holdSpeed);
        break;
      }

      case STATE_LOOP_TO_2: {
        // Loop: Move to angle 2 (workingZero + loopAngle2, with easing)
        float cycleDuration = loopDuration * 1000.0;
        float targetAngle = workingZero + loopAngle2;

        if (elapsed >= cycleDuration) {
          // Reached angle 2 - go to angle 3
          currentPosition = targetAngle;
          currentState = STATE_LOOP_TO_3;
          stateStartTime = millis();
          Serial.println("Moving to angle 3...");
        } else {
          float progress = (float)elapsed / cycleDuration;
          float velocity = easeInOutCubicDerivative(progress);
          velocity = pow(velocity, 1.0 / easingIntensity);

          float angleToMove = targetAngle - currentPosition;
          float revolutions = abs(angleToMove) / 360.0;
          float sculptureRPM = (revolutions * 60.0) / loopDuration;
          float motorRPM = sculptureRPM * gearRatio;
          int32_t baseSpeed = (int32_t)(motorRPM * 100.0);

          int32_t currentSpeed = (int32_t)(baseSpeed * velocity);

          if (currentSpeed < minSpeed && currentSpeed > 0) {
            currentSpeed = minSpeed;
          }
          if (currentSpeed > maxSpeed) {
            currentSpeed = maxSpeed;
          }

          // Determine direction
          if (angleToMove < 0) {
            currentSpeed = -currentSpeed;
          }

          motorSetSpeed(currentSpeed);
        }
        break;
      }

      case STATE_LOOP_TO_3: {
        // Loop: Move to angle 3 (workingZero + loopAngle3, with easing)
        float cycleDuration = loopDuration * 1000.0;
        float targetAngle = workingZero + loopAngle3;

        if (elapsed >= cycleDuration) {
          // Reached angle 3 - go back to angle 2
          currentPosition = targetAngle;
          currentState = STATE_LOOP_TO_2;
          stateStartTime = millis();
          Serial.println("Moving to angle 2...");
        } else {
          float progress = (float)elapsed / cycleDuration;
          float velocity = easeInOutCubicDerivative(progress);
          velocity = pow(velocity, 1.0 / easingIntensity);

          float angleToMove = targetAngle - currentPosition;
          float revolutions = abs(angleToMove) / 360.0;
          float sculptureRPM = (revolutions * 60.0) / loopDuration;
          float motorRPM = sculptureRPM * gearRatio;
          int32_t baseSpeed = (int32_t)(motorRPM * 100.0);

          int32_t currentSpeed = (int32_t)(baseSpeed * velocity);

          if (currentSpeed < minSpeed && currentSpeed > 0) {
            currentSpeed = minSpeed;
          }
          if (currentSpeed > maxSpeed) {
            currentSpeed = maxSpeed;
          }

          // Determine direction
          if (angleToMove < 0) {
            currentSpeed = -currentSpeed;
          }

          motorSetSpeed(currentSpeed);
        }
        break;
      }

      case STATE_IDLE:
        // Do nothing
        break;
    }
  }

  delay(10);
}
