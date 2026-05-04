/*
 * ============================================================================
 * FIRST WITNESS v1.1 - SMOOTH 2 REVOLUTIONS
 * ============================================================================
 *
 * BEHAVIOR:
 * 1. Power ON - wait 5 seconds
 * 2. Ease in, 2 revolutions, ease out
 * 3. Stop for 1 second
 * 4. Repeat
 *
 * No calibration - starts from current position.
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8
#define I2C_SCL_PIN      9
#define I2C_FREQ         100000

// Registers
#define REG_OUTPUT       0x00
#define REG_MODE         0x01
#define REG_STALL_PROT   0x0F
#define REG_RGB          0x30
#define REG_VIN          0x34
#define REG_TEMP         0x38
#define REG_SPEED        0x40
#define REG_SPEED_MAXCUR 0x50
#define REG_SPEED_READ   0x60
#define REG_POS          0x80
#define REG_POS_READ     0x90

#define MODE_SPEED       1

// Motor resolution
#define STEPS_PER_REV    36000   // 36000 steps = 360°

// Motion parameters
#define NUM_REVOLUTIONS  2
#define TOTAL_STEPS      (STEPS_PER_REV * NUM_REVOLUTIONS)  // 72000 steps = 2 revolutions

// =============================================================================
// STATE MACHINE
// =============================================================================

enum SystemState {
  STATE_STARTUP,    // Wait 5 seconds
  STATE_ACCEL,      // Accelerating
  STATE_CRUISE,     // Constant speed
  STATE_DECEL,      // Decelerating
  STATE_PAUSED      // Stopped for 1 second
};

SystemState currentState = STATE_STARTUP;
const char* stateNames[] = {
  "STARTUP", "ACCEL", "CRUISE", "DECEL", "PAUSED"
};

// Motion tracking
int32_t startPosition = 0;
int32_t targetPosition = 0;
unsigned long stateStartTime = 0;
int32_t cycleCount = 0;

// Easing parameters (10% accel, 80% cruise, 10% decel)
int32_t accelSteps = TOTAL_STEPS / 10;   // 10% to accelerate
int32_t decelSteps = TOTAL_STEPS / 10;   // 10% to decelerate
int32_t cruiseSteps = TOTAL_STEPS - accelSteps - decelSteps;  // 80% at cruise

// Motor parameters
int32_t currentLimit = 150000;      // 1.5A for torque
int32_t cruiseSpeed = 900;          // 9 RPM (scaled x100)
int32_t currentSpeed = 0;

// =============================================================================
// WEB SERVER
// =============================================================================

WebServer server(80);

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

int32_t motorGetPosition() {
  return readReg32(REG_POS_READ);
}

int32_t motorGetSpeed() {
  return readReg32(REG_SPEED_READ);
}

int32_t motorGetVoltage() {
  return readReg32(REG_VIN);
}

void motorSetSpeed(int32_t speed) {
  currentSpeed = speed;
  writeReg32(REG_SPEED, speed);
}

void motorStop() {
  currentSpeed = 0;
  writeReg32(REG_SPEED, 0);
}

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
  writeReg32(REG_POS, 0);  // Set current position as 0
  delay(50);
  writeReg8(REG_OUTPUT, 1);
  Serial.println("Motor initialized. Position = 0");
}

// =============================================================================
// STATE MACHINE
// =============================================================================

void enterState(SystemState newState) {
  currentState = newState;
  stateStartTime = millis();

  Serial.printf("\n>>> STATE: %s\n", stateNames[newState]);

  switch (newState) {
    case STATE_STARTUP:
      setLED(50, 50, 0);  // Yellow
      Serial.println("Waiting 5 seconds...");
      break;

    case STATE_ACCEL:
      setLED(0, 0, 50);   // Blue
      startPosition = motorGetPosition();
      targetPosition = startPosition + TOTAL_STEPS;
      Serial.printf("=== CYCLE %d START ===\n", cycleCount + 1);
      Serial.printf("Start: %d  Target: %d  Total: %d steps (%d revs)\n",
        startPosition, targetPosition, TOTAL_STEPS, NUM_REVOLUTIONS);
      Serial.printf("Accel: %d steps  Cruise: %d steps  Decel: %d steps\n",
        accelSteps, cruiseSteps, decelSteps);
      break;

    case STATE_CRUISE:
      setLED(0, 30, 50);  // Cyan
      motorSetSpeed(cruiseSpeed);
      Serial.printf("Cruising at speed %d (%.1f RPM)\n", cruiseSpeed, cruiseSpeed/100.0f);
      break;

    case STATE_DECEL:
      setLED(0, 50, 50);  // Teal
      Serial.printf("Decelerating to target %d\n", targetPosition);
      break;

    case STATE_PAUSED: {
      setLED(0, 50, 0);   // Green
      motorStop();
      cycleCount++;
      int32_t finalPos = motorGetPosition();
      Serial.printf("=== CYCLE %d COMPLETE ===\n", cycleCount);
      Serial.printf("Final position: %d (target was %d, diff: %d)\n",
        finalPos, targetPosition, finalPos - targetPosition);
      Serial.println("Pausing 1 second...");
      break;
    }
  }
}

// Debug counter for periodic output
unsigned long lastDebugTime = 0;

void updateStateMachine() {
  unsigned long now = millis();
  unsigned long elapsed = now - stateStartTime;
  int32_t pos = motorGetPosition();
  int32_t traveled = pos - startPosition;
  int32_t remaining = targetPosition - pos;

  // Debug output every 500ms during motion
  if (now - lastDebugTime >= 500 && currentState != STATE_STARTUP && currentState != STATE_PAUSED) {
    Serial.printf("[%s] pos=%d traveled=%d remaining=%d target=%d\n",
      stateNames[currentState], pos, traveled, remaining, targetPosition);
    lastDebugTime = now;
  }

  switch (currentState) {
    case STATE_STARTUP:
      if (elapsed >= 5000) {
        enterState(STATE_ACCEL);
      }
      break;

    case STATE_ACCEL: {
      // Accelerate until we've traveled accelSteps
      if (traveled >= accelSteps) {
        Serial.printf("ACCEL complete at pos=%d, entering CRUISE\n", pos);
        enterState(STATE_CRUISE);
      } else {
        // Linear ramp from 0 to cruiseSpeed
        float progress = (float)traveled / (float)accelSteps;
        int32_t speed = (int32_t)(cruiseSpeed * progress);
        if (speed < 100) speed = 100;  // Minimum 1 RPM to overcome friction
        motorSetSpeed(speed);
      }
      break;
    }

    case STATE_CRUISE: {
      // Check if we should start decelerating
      if (remaining <= decelSteps) {
        Serial.printf("CRUISE complete at pos=%d, remaining=%d, entering DECEL\n", pos, remaining);
        enterState(STATE_DECEL);
      }
      break;
    }

    case STATE_DECEL: {
      // Check if we reached or passed target
      if (pos >= targetPosition || remaining <= 0) {
        Serial.printf("DECEL complete at pos=%d, target was %d\n", pos, targetPosition);
        motorStop();
        enterState(STATE_PAUSED);
      } else {
        // Linear ramp from cruiseSpeed to minimum
        float progress = (float)remaining / (float)decelSteps;
        if (progress > 1.0f) progress = 1.0f;  // Clamp in case we overshot entry point
        int32_t speed = (int32_t)(cruiseSpeed * progress);
        if (speed < 100) speed = 100;  // Minimum 1 RPM to reach target
        motorSetSpeed(speed);
      }
      break;
    }

    case STATE_PAUSED:
      if (elapsed >= 1000) {
        Serial.println("PAUSE complete, starting next cycle");
        enterState(STATE_ACCEL);  // Start next cycle
      }
      break;
  }
}

// =============================================================================
// WEB INTERFACE
// =============================================================================

void handleRoot() {
  int32_t pos = motorGetPosition();
  float degrees = (pos * 360.0f) / STEPS_PER_REV;
  int32_t voltage = motorGetVoltage();
  int32_t speed = motorGetSpeed();

  String html = R"(<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>First Witness v1.1 - 2 Rev</title>
<style>
body{font-family:Arial;background:#1a1a2e;color:#eee;padding:15px;max-width:500px;margin:0 auto}
.card{background:#16213e;padding:12px;border-radius:10px;margin:8px 0}
.value{font-size:20px;font-weight:bold;color:#e94560}
.state{font-size:24px;color:#4CAF50;text-align:center;padding:10px}
button{background:#e94560;color:#fff;border:none;padding:15px 20px;
font-size:16px;border-radius:5px;margin:5px;cursor:pointer;width:100%}
button:active{background:#c73e54}
h1{color:#e94560;text-align:center}
h3{margin:5px 0;color:#e94560}
.status-row{display:flex;justify-content:space-between;margin:5px 0}
</style>
<script>
function cmd(c){fetch('/cmd?c='+c).then(()=>location.reload());}
setInterval(()=>location.reload(),1000);
</script></head><body>
)";

  html += "<h1>First Witness v1.1</h1>";
  html += "<div class='state'>" + String(stateNames[currentState]) + "</div>";

  html += "<div class='card'>";
  html += "<div class='status-row'>Position: <span class='value'>" + String(degrees, 1) + " deg</span></div>";
  html += "<div class='status-row'>Speed: <span class='value'>" + String(speed/100.0f, 1) + " RPM</span></div>";
  html += "<div class='status-row'>Voltage: <span class='value'>" + String(voltage/100.0f, 1) + " V</span></div>";
  html += "<div class='status-row'>Cycles: <span class='value'>" + String(cycleCount) + "</span></div>";
  html += "</div>";

  html += "<div class='card'><h3>Speed</h3>";
  html += "<button onclick=\"cmd('slow')\">SLOW (3 RPM)</button>";
  html += "<button onclick=\"cmd('med')\">MEDIUM (5 RPM)</button>";
  html += "<button onclick=\"cmd('fast')\">FAST (8 RPM)</button>";
  html += "</div>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleCmd() {
  if (server.hasArg("c")) {
    String cmd = server.arg("c");
    if (cmd == "slow") cruiseSpeed = 300;
    else if (cmd == "med") cruiseSpeed = 500;
    else if (cmd == "fast") cruiseSpeed = 800;
    Serial.printf("Speed set to %.1f RPM\n", cruiseSpeed / 100.0f);
  }
  server.send(200, "text/plain", "OK");
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n================================================");
  Serial.println("  FIRST WITNESS v1.1 - SMOOTH 2 REVOLUTIONS");
  Serial.println("================================================");
  Serial.println("  1. Wait 5 seconds");
  Serial.println("  2. Ease in, 2 revolutions, ease out");
  Serial.println("  3. Stop for 1 second");
  Serial.println("  4. Repeat");
  Serial.println("================================================\n");

  // I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  // Check motor
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: Motor not found!");
    setLED(50, 0, 0);
    while(1) delay(1000);
  }
  Serial.println("Motor found!");

  // Init motor
  motorInit();

  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP("FirstWitness", "witness123");
  delay(500);
  Serial.printf("WiFi: FirstWitness / witness123\n");
  Serial.printf("URL: http://%s\n", WiFi.softAPIP().toString().c_str());

  // Web server
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.begin();

  // Start
  enterState(STATE_STARTUP);
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
  server.handleClient();

  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == '1') { cruiseSpeed = 300; Serial.println("3 RPM"); }
    if (cmd == '2') { cruiseSpeed = 500; Serial.println("5 RPM"); }
    if (cmd == '3') { cruiseSpeed = 800; Serial.println("8 RPM"); }
    if (cmd == '?') {
      Serial.printf("State: %s, Pos: %d, Cycles: %d\n",
        stateNames[currentState], motorGetPosition(), cycleCount);
    }
  }

  updateStateMachine();
  delay(10);
}
