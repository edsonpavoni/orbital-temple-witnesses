/*
 * ============================================================================
 * ORBITAL TEMPLE - SCULPTURE 2
 * Heart Pulsation Pattern
 * ============================================================================
 *
 * DESCRIPTION:
 * ------------
 * Creates a heartbeat-like pulsation:
 * 1. Rotate counter-clockwise 180 degrees
 * 2. Release motor (gravity returns piece to rest)
 * 3. Repeat 3 times (3 heartbeats)
 * 4. Wait 30 seconds
 * 5. Repeat cycle
 *
 * SERIAL COMMANDS:
 * ----------------
 * g         - GO! Start pulsation cycle
 * x         - ABORT! Stop everything
 * p<n>      - Set Pulse count (p3)
 * w<n>      - Set wait time between cycles (w30)
 * d<n>      - Set rotation Duration per pulse (d2)
 * r<n>      - Set Release time (gravity fall) (r1)
 * t<n>      - Set gear raTio (t3.2)
 * c<n>      - Set max current mA (c1000)
 * s         - Show settings
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

int pulseCount = 3;                  // Number of pulses per cycle (heartbeats)
float waitTime = 30.0;               // Seconds to wait between cycles
float pulseDuration = 2.0;           // Seconds to rotate 180 degrees per pulse
float releaseTime = 1.0;             // Seconds to let gravity return piece
float gearRatio = 1.0;               // No gear reduction - direct drive (1:1)
int32_t maxCurrent = 100000;         // Max current (in units of 0.01mA)

// State machine
enum State {
  STATE_ROTATING_CCW,   // Rotating counter-clockwise 180°
  STATE_ROTATING_CW,    // Rotating clockwise back to 0°
  STATE_RELEASING,      // Motor off at position 0, gravity settles
  STATE_WAITING         // Waiting between cycles
};

State currentState = STATE_WAITING;
unsigned long stateStartTime = 0;
int currentPulse = 0;                // Current pulse number (0 to pulseCount-1)
int32_t targetPosition = 0;          // Target position for movements

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
#define REG_POS          0x80   // Position target
#define REG_POS_SPEED    0x84   // Position mode speed limit
#define REG_POS_MAXCUR   0x88   // Position mode max current
#define REG_POS_READ     0x90   // Position readback

#define MODE_SPEED       1
#define MODE_POSITION    2

// Position encoder resolution (encoder counts per revolution)
// RollerCAN typically uses 16384 counts per revolution
#define ENCODER_CPR      16384

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
  writeReg8(REG_MODE, MODE_POSITION);  // Use position mode for precise movements
  delay(50);
  writeReg32(REG_POS_MAXCUR, maxCurrent);  // Set max current for position mode
  delay(50);
  writeReg32(REG_POS_SPEED, 5000);  // Set position speed limit (in 0.01 RPM units)
  delay(50);
  writeReg32(REG_POS, 0);  // Set initial position target to 0
  delay(50);
  writeReg8(REG_OUTPUT, 1);
  delay(50);
}

void motorUpdateMaxCurrent() {
  writeReg32(REG_POS_MAXCUR, maxCurrent);
}

void motorSetSpeed(int32_t speed) {
  writeReg32(REG_SPEED, speed);
}

void motorSetPosition(int32_t position) {
  writeReg32(REG_POS, position);
}

int32_t motorGetPosition() {
  return readReg32(REG_POS_READ);
}

void motorResetPosition() {
  // To reset position, we need to disable motor, reset internal counter, and re-enable
  // For RollerCAN, we can just set current position as the new zero by reading and offsetting
  // Or simply assume we're at 0 and set position target to 0
  motorSetPosition(0);
}

void motorEnable(bool enable) {
  writeReg8(REG_OUTPUT, enable ? 1 : 0);
}

// =============================================================================
// STATE MACHINE FUNCTIONS
// =============================================================================

void startPulseCycle() {
  currentPulse = 0;
  motorResetPosition();  // Calibrate position to 0
  currentState = STATE_ROTATING_CCW;
  stateStartTime = millis();
}

void abortAll() {
  currentState = STATE_WAITING;
  currentPulse = 0;
  motorSetPosition(0);  // Return to position 0
  Serial.println("ABORTED - All stopped");
}

String getStateName() {
  switch(currentState) {
    case STATE_ROTATING_CCW: return "ROTATING CCW";
    case STATE_ROTATING_CW: return "ROTATING CW";
    case STATE_RELEASING: return "RELEASING";
    case STATE_WAITING: return "WAITING";
    default: return "UNKNOWN";
  }
}

// =============================================================================
// SERIAL COMMAND PROCESSING
// =============================================================================

void printSettings() {
  Serial.println("\n========================================");
  Serial.println("     SCULPTURE 2 - HEART PULSATION");
  Serial.println("========================================");

  // Current State
  Serial.print("State: ");
  Serial.println(getStateName());

  if (currentState == STATE_ROTATING_CCW || currentState == STATE_ROTATING_CW || currentState == STATE_RELEASING) {
    Serial.print("Pulse: ");
    Serial.print(currentPulse + 1);
    Serial.print(" of ");
    Serial.println(pulseCount);

    Serial.print("Position: ");
    Serial.println(motorGetPosition());
  }

  if (currentState == STATE_WAITING) {
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
  Serial.print("Pulse Count: ");
  Serial.println(pulseCount);

  Serial.print("Pulse Duration: ");
  Serial.print(pulseDuration);
  Serial.println("s (180° rotation)");

  Serial.print("Release Time: ");
  Serial.print(releaseTime);
  Serial.println("s (gravity return)");

  Serial.print("Wait Between Cycles: ");
  Serial.print(waitTime);
  Serial.println("s");

  Serial.print("Gear Ratio: ");
  Serial.print(gearRatio);
  Serial.println(":1");

  Serial.print("Max Current: ");
  Serial.print(maxCurrent / 100.0);
  Serial.println(" mA");

  Serial.println("\n--- COMMANDS ---");
  Serial.println("  g         - GO! Start pulse cycle");
  Serial.println("  x         - ABORT! Stop everything");
  Serial.println("  p<n>      - Set pulse count (p3)");
  Serial.println("  w<n>      - Set wait time (w30)");
  Serial.println("  d<n>      - Set pulse duration (d2)");
  Serial.println("  r<n>      - Set release time (r1)");
  Serial.println("  t<n>      - Set gear ratio (t3.2)");
  Serial.println("  c<n>      - Set max current mA (c1000)");
  Serial.println("  s         - Show this status");
  Serial.println("========================================\n");
}

void processSerialCommand() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();

    if (command == "g") {
      startPulseCycle();
      Serial.println("Starting pulse cycle!");
    }
    else if (command == "x") {
      abortAll();
    }
    else if (command.startsWith("p")) {
      int newCount = command.substring(1).toInt();
      if (newCount >= 1 && newCount <= 20) {
        pulseCount = newCount;
        Serial.print("Pulse count: ");
        Serial.println(pulseCount);
      } else {
        Serial.println("ERROR: 1-20");
      }
    }
    else if (command.startsWith("w")) {
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
    else if (command.startsWith("d")) {
      float newDuration = command.substring(1).toFloat();
      if (newDuration > 0 && newDuration <= 60) {
        pulseDuration = newDuration;
        Serial.print("Pulse duration: ");
        Serial.print(pulseDuration);
        Serial.println("s");
      } else {
        Serial.println("ERROR: 0-60");
      }
    }
    else if (command.startsWith("r")) {
      float newRelease = command.substring(1).toFloat();
      if (newRelease >= 0 && newRelease <= 10) {
        releaseTime = newRelease;
        Serial.print("Release time: ");
        Serial.print(releaseTime);
        Serial.println("s");
      } else {
        Serial.println("ERROR: 0-10");
      }
    }
    else if (command.startsWith("t")) {
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

  Serial.println("\nSCULPTURE 2 - Heart Pulsation");
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
  Serial.println("Ready!");
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
      case STATE_ROTATING_CCW: {
        // Rotate counter-clockwise 180 degrees using position control
        // No gear reduction - direct drive (1:1)
        // 180 degrees = 0.5 revolutions
        // Calculate encoder counts for half revolution
        int32_t targetPos = ENCODER_CPR / 2;  // Half revolution = 180 degrees (8192 counts)

        int32_t currentPos = motorGetPosition();

        // Check if we've reached target (with tolerance of ~1 degree)
        int32_t tolerance = ENCODER_CPR / 360;  // ~1 degree tolerance
        if (abs(currentPos - targetPos) < tolerance) {
          // Reached 180 degrees - now return clockwise
          currentState = STATE_ROTATING_CW;
          stateStartTime = millis();
        } else {
          // Set target position
          motorSetPosition(targetPos);
        }
        break;
      }

      case STATE_ROTATING_CW: {
        // Rotate clockwise back to position 0
        int32_t currentPos = motorGetPosition();

        // Check if we've reached position 0 (with tolerance of ~1 degree)
        int32_t tolerance = ENCODER_CPR / 360;  // ~1 degree tolerance
        if (abs(currentPos) < tolerance) {
          // Reached position 0 - release motor and calibrate
          motorResetPosition();  // Calibrate to 0
          motorEnable(false);    // Disable motor to let gravity settle
          currentState = STATE_RELEASING;
          stateStartTime = millis();
        } else {
          // Return to position 0
          motorSetPosition(0);
        }
        break;
      }

      case STATE_RELEASING: {
        // Motor off at position 0, gravity settles
        float duration = releaseTime * 1000.0;

        if (elapsed >= duration) {
          // Release complete
          motorEnable(true);  // Re-enable motor
          currentPulse++;

          if (currentPulse >= pulseCount) {
            // All pulses done - enter waiting
            currentState = STATE_WAITING;
            stateStartTime = millis();
            currentPulse = 0;
          } else {
            // More pulses to do - reset position and start next pulse
            motorResetPosition();
            currentState = STATE_ROTATING_CCW;
            stateStartTime = millis();
          }
        }
        break;
      }

      case STATE_WAITING: {
        // Wait between cycles
        float duration = waitTime * 1000.0;
        if (elapsed >= duration) {
          // Start new cycle
          startPulseCycle();
        }
        break;
      }
    }
  }

  delay(10);
}
