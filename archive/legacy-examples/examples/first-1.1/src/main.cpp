/*
 * ============================================================================
 * FIRST WITNESS v1.1 - SMOOTH 6 REVOLUTIONS
 * ============================================================================
 *
 * Artistic concept:
 * Simple, meditative motion. 6 full revolutions with smooth acceleration
 * and deceleration, returning exactly to the start position.
 *
 * BEHAVIOR:
 * 1. Power ON → wait 5 seconds
 * 2. Ease in (accelerate smoothly)
 * 3. 6 full revolutions
 * 4. Ease out (decelerate smoothly)
 * 5. Stop at position 0 for 1 second
 * 6. Repeat
 *
 * No calibration. Current position at startup = position 0.
 *
 * HARDWARE:
 * - Seeed XIAO ESP32-S3
 * - M5Stack RollerCAN BLDC (I2C)
 * - 15V power via USB-C PD trigger board
 * - Direct drive (36000 steps = 360°)
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
#define TOTAL_STEPS      (STEPS_PER_REV * NUM_REVOLUTIONS)  // 216000 steps

// Easing zones (as fraction of total distance)
#define ACCEL_ZONE       0.10f   // First 10% = acceleration
#define DECEL_ZONE       0.10f   // Last 10% = deceleration

// Timing
#define STARTUP_DELAY    500    // 5 seconds wait after power on
#define PAUSE_DURATION   2000    // 1 second pause at position 0

// =============================================================================
// STATE MACHINE
// =============================================================================

enum SystemState {
  STATE_STARTUP,       // Waiting after power on
  STATE_MOVING,        // In motion (6 revolutions with easing)
  STATE_PAUSED         // Paused at position 0
};

SystemState currentState = STATE_STARTUP;
const char* stateNames[] = { "STARTUP", "MOVING", "PAUSED" };

// Motion tracking
int32_t startPosition = 0;       // Where this cycle started
int32_t targetPosition = 0;      // Where we're going (start + 6 revs)
unsigned long stateStartTime = 0;
int32_t cycleCount = 0;

// Motion parameters
int32_t currentLimit = 150000;   // 1.5A for torque
int32_t maxSpeed = 900;          // 5 RPM (scaled x100) - cruise speed
int32_t minSpeed = 400;           // 0.5 RPM minimum during easing

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

void ledStartup()  { setLED(50, 50, 0); }   // Yellow = waiting
void ledMoving()   { setLED(0, 30, 50); }   // Cyan = in motion
void ledPaused()   { setLED(0, 50, 0); }    // Green = at rest

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
  writeReg32(REG_SPEED, speed);
}

void motorStop() {
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

  // Set current position as 0
  writeReg32(REG_POS, 0);
  delay(50);

  writeReg8(REG_OUTPUT, 1);
  Serial.println("Motor initialized. Position set to 0.");
}

// =============================================================================
// EASING FUNCTION
// =============================================================================

// Attempt 2
// Attempt 3
// Calculate speed based on position within the movement
// Returns speed value with smooth ease-in and ease-out
int32_t calculateEasedSpeed(int32_t currentPos, int32_t startPos, int32_t endPos) {
  int32_t totalDistance = endPos - startPos;
  int32_t traveled = currentPos - startPos;

  // Progress from 0.0 to 1.0
  float progress = (float)traveled / (float)totalDistance;
  progress = constrain(progress, 0.0f, 1.0f);

  float speedMultiplier = 1.0f;

  // Acceleration zone (first 10%)
  if (progress < ACCEL_ZONE) {
    // Smooth ease-in using sine curve
    float accelProgress = progress / ACCEL_ZONE;
    speedMultiplier = sin(accelProgress * PI / 2);  // 0 to 1
  }
  // Deceleration zone (last 10%)
  else if (progress > (1.0f - DECEL_ZONE)) {
    // Smooth ease-out using sine curve
    float decelProgress = (progress - (1.0f - DECEL_ZONE)) / DECEL_ZONE;
    speedMultiplier = cos(decelProgress * PI / 2);  // 1 to 0
  }
  // Cruise zone (middle 80%)
  else {
    speedMultiplier = 1.0f;
  }

  // Calculate final speed
  int32_t speed = minSpeed + (int32_t)((maxSpeed - minSpeed) * speedMultiplier);
  return speed;
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
      ledStartup();
      Serial.printf("Waiting %d seconds before starting...\n", STARTUP_DELAY / 1000);
      break;

    case STATE_MOVING:
      ledMoving();
      startPosition = motorGetPosition();
      targetPosition = startPosition + TOTAL_STEPS;
      Serial.printf("Starting 6 revolutions: %d -> %d\n", startPosition, targetPosition);
      break;

    case STATE_PAUSED:
      ledPaused();
      motorStop();
      cycleCount++;
      Serial.printf("Cycle %d complete. Pausing for %d ms.\n", cycleCount, PAUSE_DURATION);
      break;
  }
}

void updateStateMachine() {
  unsigned long elapsed = millis() - stateStartTime;

  switch (currentState) {
    case STATE_STARTUP:
      // Wait for startup delay
      if (elapsed >= STARTUP_DELAY) {
        enterState(STATE_MOVING);
      }
      break;

    case STATE_MOVING: {
      int32_t currentPos = motorGetPosition();

      // Check if we've reached the target
      if (currentPos >= targetPosition) {
        motorStop();
        enterState(STATE_PAUSED);
      }
      else {
        // Calculate and set eased speed
        int32_t speed = calculateEasedSpeed(currentPos, startPosition, targetPosition);
        motorSetSpeed(speed);
      }
      break;
    }

    case STATE_PAUSED:
      // Wait for pause duration, then start again
      if (elapsed >= PAUSE_DURATION) {
        enterState(STATE_MOVING);
      }
      break;
  }
}

// =============================================================================
// WEB INTERFACE
// =============================================================================

void handleRoot() {
  int32_t pos = motorGetPosition();
  float degrees = fmod((pos * 360.0f) / STEPS_PER_REV, 360.0f);
  if (degrees < 0) degrees += 360;
  int32_t voltage = motorGetVoltage();
  int32_t speed = motorGetSpeed();

  // Calculate progress in current cycle
  float progress = 0;
  if (currentState == STATE_MOVING && (targetPosition - startPosition) != 0) {
    progress = (float)(pos - startPosition) / (float)(targetPosition - startPosition) * 100.0f;
    progress = constrain(progress, 0, 100);
  }

  String html = R"(<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>First Witness v1.1</title>
<style>
body{font-family:Arial;background:#1a1a2e;color:#eee;padding:15px;max-width:500px;margin:0 auto}
.card{background:#16213e;padding:12px;border-radius:10px;margin:8px 0}
.value{font-size:20px;font-weight:bold;color:#e94560}
.state{font-size:24px;color:#4CAF50;text-align:center;padding:10px}
button{background:#e94560;color:#fff;border:none;padding:15px 20px;
font-size:16px;border-radius:5px;margin:5px;cursor:pointer;width:100%}
button:active{background:#c73e54}
.grn{background:#4CAF50}
.org{background:#FF9800}
h1{color:#e94560;text-align:center}
h3{margin:5px 0;color:#e94560}
.status-row{display:flex;justify-content:space-between;margin:5px 0}
.progress-bar{background:#333;border-radius:5px;height:20px;margin:10px 0}
.progress-fill{background:#4CAF50;height:100%;border-radius:5px;transition:width 0.3s}
</style>
<script>
function cmd(c){fetch('/cmd?c='+c).then(()=>location.reload());}
setInterval(()=>location.reload(),1000);
</script></head><body>
)";

  html += "<h1>First Witness v1.1</h1>";
  html += "<div class='state'>" + String(stateNames[currentState]) + "</div>";

  html += "<div class='card'>";
  html += "<div class='status-row'>Position: <span class='value'>" + String(degrees, 1) + "&deg;</span></div>";
  html += "<div class='status-row'>Speed: <span class='value'>" + String(speed/100.0f, 1) + " RPM</span></div>";
  html += "<div class='status-row'>Voltage: <span class='value'>" + String(voltage/100.0f, 1) + " V</span></div>";
  html += "<div class='status-row'>Cycles: <span class='value'>" + String(cycleCount) + "</span></div>";

  if (currentState == STATE_MOVING) {
    html += "<div class='progress-bar'><div class='progress-fill' style='width:" + String(progress, 0) + "%'></div></div>";
    html += "<div style='text-align:center'>" + String(progress, 1) + "% of 6 revolutions</div>";
  }
  html += "</div>";

  html += "<div class='card'><h3>Speed Control</h3>";
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

    if (cmd == "slow") {
      maxSpeed = 300;
      Serial.println("Max speed set to 3 RPM");
    }
    else if (cmd == "med") {
      maxSpeed = 500;
      Serial.println("Max speed set to 5 RPM");
    }
    else if (cmd == "fast") {
      maxSpeed = 800;
      Serial.println("Max speed set to 8 RPM");
    }
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
  Serial.println("  FIRST WITNESS v1.1 - SMOOTH 6 REVOLUTIONS");
  Serial.println("================================================");
  Serial.println();
  Serial.println("BEHAVIOR:");
  Serial.println("  1. Wait 5 seconds");
  Serial.println("  2. Ease in (accelerate)");
  Serial.println("  3. 6 full revolutions");
  Serial.println("  4. Ease out (decelerate)");
  Serial.println("  5. Pause 1 second at position 0");
  Serial.println("  6. Repeat");
  Serial.println();
  Serial.println("COMMANDS:");
  Serial.println("  1 - Slow (3 RPM)");
  Serial.println("  2 - Medium (5 RPM)");
  Serial.println("  3 - Fast (8 RPM)");
  Serial.println("  ? - Status");
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

  // Initialize motor
  motorInit();

  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Witness11", "witness123");
  delay(500);
  Serial.printf("WiFi: Witness11 / witness123\n");
  Serial.printf("URL: http://%s\n", WiFi.softAPIP().toString().c_str());

  // Web server
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.begin();

  // Start the state machine
  enterState(STATE_STARTUP);
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
  server.handleClient();

  // Serial commands
  if (Serial.available()) {
    char cmd = Serial.read();
    switch (cmd) {
      case '1':
        maxSpeed = 300;
        Serial.println("Speed: 3 RPM");
        break;
      case '2':
        maxSpeed = 500;
        Serial.println("Speed: 5 RPM");
        break;
      case '3':
        maxSpeed = 800;
        Serial.println("Speed: 8 RPM");
        break;
      case '?':
        Serial.printf("\n--- STATUS ---\n");
        Serial.printf("State: %s\n", stateNames[currentState]);
        Serial.printf("Position: %d (%.1f deg)\n",
          motorGetPosition(),
          fmod(motorGetPosition() * 360.0f / STEPS_PER_REV, 360.0f));
        Serial.printf("Cycles: %d\n", cycleCount);
        Serial.printf("Max speed: %.1f RPM\n", maxSpeed / 100.0f);
        Serial.printf("--------------\n\n");
        break;
    }
  }

  // Update state machine
  updateStateMachine();

  delay(10);  // 100Hz update rate
}
