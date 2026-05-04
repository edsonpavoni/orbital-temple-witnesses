/*
 * ============================================================================
 * HEART SCULPTURE - BURST ROTATION WITH WEB INTERFACE
 * ============================================================================
 *
 * BEHAVIOR:
 * 1. INIT: Rotates 2 full turns on startup
 * 2. HOLD: Maximum holding power to keep motor fixed
 * 3. LOOP: Burst rotation 360° in each direction with position monitoring
 * 4. WEB INTERFACE: Control via phone/browser
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include "webpage.h"

// =============================================================================
// HARDWARE CONFIGURATION
// =============================================================================

#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8
#define I2C_SCL_PIN      9
#define I2C_FREQ         100000

// =============================================================================
// WIFI CONFIGURATION - ACCESS POINT MODE
// =============================================================================

const char* ap_ssid = "Heart-Sculpture";
const char* ap_password = "12345678";  // Must be at least 8 characters
const IPAddress local_ip(192, 168, 4, 1);
const IPAddress gateway(192, 168, 4, 1);
const IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

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
const int32_t STEPS_PER_REVOLUTION = 16000; // Steps for 360° (4x because 4000 = 90°)
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

int32_t holdTargetPosition = 0;  // Global variable for hold position
int32_t loopMaxSpeed = 150;      // Adjustable loop rotation speed (default: 150)
bool emergencyStop = false;      // Emergency stop flag
int32_t loopStartPosition = 0;   // Position when loop starts
int32_t loopDistance = STEPS_PER_REVOLUTION; // How far to rotate in each direction (default: 360°)
int32_t loopPauseTime = 0;       // Pause time between movements in milliseconds (default: 0ms)

void powerPull() {
  Serial.println("\n⚡ POWER PULL: Burst + 3 rotations");

  // Switch to speed mode
  motorConfigureSpeedMode();

  // BURST: Apply maximum force for 500ms to break initial resistance
  Serial.println("  💥 Applying burst force...");
  motorSetSpeed(800);  // Maximum speed
  delay(500);  // Hold max power for half a second

  // Check for emergency stop
  if (emergencyStop) {
    motorSetSpeed(0);
    return;
  }

  // Now continue with 3 full rotations at max speed
  Serial.println("  ↻ Continuing 3 rotations...");
  int32_t startPos = motorGetPosition();
  int32_t targetPos = startPos + (STEPS_PER_REVOLUTION * 3);  // 3 full rotations forward

  while (motorGetPosition() < targetPos && !emergencyStop) {
    // Check for emergency stop command
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

    // Maintain max speed
    motorSetSpeed(800);
    delay(50);
  }

  motorSetSpeed(0);
  Serial.println("✓ Power pull complete");
}

void initRotation() {
  Serial.println("\n🔄 INITIALIZATION: 2 full rotations (CCW)");

  int32_t startPos = motorGetPosition();
  int32_t targetPos = startPos - (STEPS_PER_REVOLUTION * 2); // CCW = negative

  motorSetSpeed(-INIT_SPEED); // CCW = negative speed

  while (motorGetPosition() > targetPos && !emergencyStop) {
    delay(100);
  }

  motorSetSpeed(0);
  if (emergencyStop) {
    Serial.println("✗ Initialization interrupted");
  } else {
    Serial.println("✓ Initialization complete");
  }
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

  // Reset loop start position to zero
  loopStartPosition = 0;
  Serial.println("  Loop counter reset to 0");

  Serial.println("  Motor will actively resist movement");
}

void gentleRotation(int32_t direction) {
  int32_t degrees = (loopDistance * 360) / STEPS_PER_REVOLUTION;
  Serial.printf("\n⚡ BURST ROTATION: %s (%d°) - MAXIMUM FORCE\n",
                direction > 0 ? "Forward" : "Backward", degrees);

  // Switch to speed mode
  motorConfigureSpeedMode();

  // Get starting position (relative to loop start)
  int32_t currentAbsolutePos = motorGetPosition();
  int32_t startPos = currentAbsolutePos - loopStartPosition;
  int32_t targetPos = startPos + (loopDistance * direction);  // Use adjustable distance

  Serial.printf("Start: %d | Target: %d (relative to loop start)\n", startPos, targetPos);
  Serial.println("Position readings every 100ms:");

  unsigned long lastPrint = millis();
  int printCount = 0;

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

    // Get current position
    int32_t currentAbsPos = motorGetPosition();
    int32_t relativePos = currentAbsPos - loopStartPosition;
    int32_t distanceTraveled = abs(relativePos - startPos);
    int32_t remaining = abs(targetPos - relativePos);

    // Print position every 100ms
    if (millis() - lastPrint >= 100) {
      Serial.printf("[%d] Pos: %d | Moved: %d | Remaining: %d\n",
                    printCount++, relativePos, distanceTraveled, remaining);
      lastPrint = millis();
    }

    // Check if reached target
    if ((direction > 0 && relativePos >= targetPos) ||
        (direction < 0 && relativePos <= targetPos)) {
      break;
    }

    // APPLY MAXIMUM BURST FORCE - NO RAMPING
    motorSetSpeed(800 * direction);  // Full speed immediately

    delay(10);
  }

  motorSetSpeed(0);
  Serial.printf("✓ Rotation complete! Final position: %d\n", motorGetPosition() - loopStartPosition);
}

// =============================================================================
// WEB INTERFACE HANDLERS
// =============================================================================

void handleRoot() {
  server.send(200, "text/html", webPage);
}

void handleWebCommand() {
  if (server.hasArg("c")) {
    String cmd = server.arg("c");
    if (cmd.length() > 0) {
      handleCommand(cmd.charAt(0));
      server.send(200, "text/plain", "OK");
      return;
    }
  }
  server.send(400, "text/plain", "Bad Request");
}

void handleStatus() {
  String stateStr = "IDLE";
  if (currentState == HOLDING) stateStr = "HOLDING";
  else if (currentState == LOOPING) stateStr = "LOOPING";

  int32_t degrees = (loopDistance * 360) / STEPS_PER_REVOLUTION;

  String json = "{";
  json += "\"state\":\"" + stateStr + "\",";
  json += "\"position\":" + String(motorGetPosition()) + ",";
  json += "\"distance\":" + String(degrees) + ",";
  json += "\"pause\":" + String(loopPauseTime);
  json += "}";

  server.send(200, "application/json", json);
}

void handleSetDistance() {
  if (server.hasArg("v")) {
    int32_t degrees = server.arg("v").toInt();
    loopDistance = (degrees * STEPS_PER_REVOLUTION) / 360;
    loopDistance = constrain(loopDistance, STEPS_PER_REVOLUTION / 8, STEPS_PER_REVOLUTION * 4);
    server.send(200, "text/plain", "OK");
    return;
  }
  server.send(400, "text/plain", "Bad Request");
}

void handleSetPause() {
  if (server.hasArg("v")) {
    loopPauseTime = server.arg("v").toInt();
    loopPauseTime = constrain(loopPauseTime, 0, 10000);
    server.send(200, "text/plain", "OK");
    return;
  }
  server.send(400, "text/plain", "Bad Request");
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n");
  Serial.println("============================================");
  Serial.println("  HEART SCULPTURE - WEB INTERFACE");
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

  // Initialize WiFi Access Point
  Serial.println("Starting WiFi Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password);

  delay(100);

  IPAddress IP = WiFi.softAPIP();
  Serial.println("  ✓ Access Point started");
  Serial.printf("  Network Name: %s\n", ap_ssid);
  Serial.printf("  Password: %s\n", ap_password);
  Serial.printf("  IP address: %s\n", IP.toString().c_str());
  Serial.println("\n  >>> Connect your phone to this WiFi network");
  Serial.println("  >>> Then open http://192.168.4.1 in your browser\n");

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/cmd", handleWebCommand);
  server.on("/status", handleStatus);
  server.on("/setdist", handleSetDistance);
  server.on("/setpause", handleSetPause);

  server.begin();
  Serial.println("  Web server started ✓");

  Serial.println("\n✓ System ready!\n");

  // Show initial status and commands
  showStatus();
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
      Serial.println("LOOPING (Gentle Rotation)");
      break;
  }

  Serial.printf("Motor Position: %d\n", motorGetPosition());
  Serial.printf("Loop Speed: %d (min: 20, max: 500)\n", loopMaxSpeed);
  int32_t degrees = (loopDistance * 360) / STEPS_PER_REVOLUTION;
  Serial.printf("Loop Distance: %d° (%d steps)\n", degrees, loopDistance);
  Serial.printf("Loop Pause Time: %dms (between movements)\n", loopPauseTime);
  Serial.println("\nCommands:");
  Serial.println("  i - Initialize (2 rotations)");
  Serial.println("  h - Hold position (maximum power)");
  Serial.println("  l - Start gentle rotation loop");
  Serial.println("  p - POWER PULL (burst + 3 rotations)");
  Serial.println("  s - Stop / Go to IDLE");
  Serial.println("  d - Disable motor (no power)");
  Serial.println("  e - EMERGENCY STOP (stop all movement immediately)");
  Serial.println("  + - Increase loop speed (+10)");
  Serial.println("  - - Decrease loop speed (-10)");
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
    case 'i':
    case 'I':
      Serial.println("\n>>> Command: INITIALIZE");
      emergencyStop = false;
      currentState = IDLE;
      initRotation();
      if (!emergencyStop) {
        holdPosition();
        currentState = HOLDING;
      }
      showStatus();
      break;

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
      // Set loop start position to current position
      loopStartPosition = motorGetPosition();
      Serial.printf("  Loop start position set to: %d\n", loopStartPosition);
      currentState = LOOPING;
      Serial.println("✓ Starting burst rotation loop");
      showStatus();
      break;

    case 'p':
    case 'P':
      Serial.println("\n>>> Command: POWER PULL");
      emergencyStop = false;
      motorEnable();
      delay(50);
      powerPull();
      currentState = IDLE;
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

    case '+':
    case '=':
      loopMaxSpeed = constrain(loopMaxSpeed + 10, 20, 500);
      Serial.printf("\n>>> Loop speed increased to: %d\n", loopMaxSpeed);
      showStatus();
      break;

    case '-':
    case '_':
      loopMaxSpeed = constrain(loopMaxSpeed - 10, 20, 500);
      Serial.printf("\n>>> Loop speed decreased to: %d\n", loopMaxSpeed);
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
  // Handle web server requests
  server.handleClient();

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

    // Apply corrective force proportional to error
    if (abs(error) > 10) { // Small deadband for responsive holding
      int32_t holdSpeed = constrain(error * 5, -800, 800); // Moderate gain
      motorSetSpeed(holdSpeed);
    } else {
      motorSetSpeed(0);
    }
    delay(15); // Moderate update rate

  } else if (currentState == LOOPING) {
    // Gentle rotation forward
    gentleRotation(1);

    // Check if emergency stop was triggered during rotation
    if (emergencyStop || currentState != LOOPING) {
      return;
    }

    // Hold after rotation (with adjustable pause time)
    if (loopPauseTime > 0) {
      holdPosition();
      unsigned long holdStart = millis();
      while (millis() - holdStart < loopPauseTime && currentState == LOOPING && !emergencyStop) {
        // Active holding for the configured pause time
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

    // Check if emergency stop was triggered during hold
    if (emergencyStop || currentState != LOOPING) {
      return;
    }

    // Gentle rotation backward
    if (currentState == LOOPING && !emergencyStop) {
      gentleRotation(-1);

      // Check if emergency stop was triggered during rotation
      if (emergencyStop || currentState != LOOPING) {
        return;
      }

      // Hold after rotation (with adjustable pause time)
      if (loopPauseTime > 0) {
        holdPosition();
        unsigned long holdStart = millis();
        while (millis() - holdStart < loopPauseTime && currentState == LOOPING && !emergencyStop) {
          // Active holding for the configured pause time
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
