/*
 * ============================================================================
 * FIRST WITNESS - PRECISE REVOLUTION
 * ============================================================================
 *
 * Artistic concept: Every 15 seconds, sculpture makes one full revolution
 * and stops pointing at the satellite.
 *
 * Motion profile: S-Curve (smoothstep)
 * - Smooth acceleration (ease-in)
 * - Cruise at target speed
 * - Smooth deceleration (ease-out) - gentle approach to zero
 * - Stop precisely at 360°
 *
 * Uses SPEED MODE with position tracking for precise stops.
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
#define REG_POS_READ     0x90

#define MODE_SPEED       1

// Motor resolution
#define STEPS_PER_REV    36000   // 36000 steps = 360°

// =============================================================================
// EASING FUNCTIONS (S-Curve for smooth motion)
// =============================================================================

// Attempt #1: Attempt#1
// Ken Perlin's improved smoothstep - very smooth, no sudden changes
float smootherstep(float t) {
  t = constrain(t, 0.0f, 1.0f);
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// Attempt #2: ease-in (slow start, accelerates)
float easeInCubic(float t) {
  return t * t * t;
}

// Attempt #3: ease-out (fast start, decelerates smoothly to zero)
float easeOutCubic(float t) {
  float f = 1.0f - t;
  return 1.0f - (f * f * f);
}

// Attempt #4: ease-out quint (even smoother approach to zero)
float easeOutQuint(float t) {
  float f = 1.0f - t;
  return 1.0f - (f * f * f * f * f);
}

// =============================================================================
// STATE
// =============================================================================

WebServer server(80);

int32_t currentSpeed = 0;
int32_t currentLimit = 150000;  // 1.5A for more torque
bool motorEnabled = false;

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
unsigned long revAccelTime = 800;   // 0.8s acceleration (smoother start)
unsigned long revDecelTime = 800;   // 0.8s deceleration
int32_t revDecelStartPos = 0;

// Easing mode (changeable via serial)
int easingMode = 0;  // 0=smootherstep, 1=cubic, 2=quint

// Stats for precision testing
int revolutionCount = 0;
int32_t lastEndPos = 0;

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
  motorEnabled = true;
  Serial.println("Motor ready");
}

void motorSetSpeed(int32_t speed) {
  currentSpeed = speed;
  writeReg32(REG_SPEED, speed);
}

void motorStop() {
  writeReg32(REG_SPEED, 0);
  currentSpeed = 0;
}

int32_t motorGetSpeed() {
  return readReg32(REG_SPEED_READ);
}

int32_t motorGetPosition() {
  return readReg32(REG_POS_READ);
}

int32_t motorGetVoltage() {
  return readReg32(REG_VIN);
}

// =============================================================================
// PRECISE REVOLUTION
// =============================================================================

// Start a precise 360° revolution
// duration: total time in milliseconds
void startRevolution(unsigned long duration) {
  if (revState != REV_IDLE) {
    Serial.println("Revolution already in progress!");
    return;
  }

  // Calculate speeds
  // For trapezoidal profile: accel + cruise + decel
  // Total distance = 36000 steps
  // accel distance + cruise distance + decel distance = 36000

  // With 0.5s accel and 0.5s decel, cruise time = duration - 1000ms
  unsigned long cruiseTime = duration - revAccelTime - revDecelTime;

  // Average speed during accel/decel = cruiseSpeed/2
  // Distance during accel = (cruiseSpeed/2) * accelTime
  // Distance during cruise = cruiseSpeed * cruiseTime
  // Distance during decel = (cruiseSpeed/2) * decelTime

  // Total = cruiseSpeed * (accelTime/2 + cruiseTime + decelTime/2)
  // 36000 = cruiseSpeed * (0.25 + cruiseTime/1000 + 0.25)
  // 36000 = cruiseSpeed * (0.5 + cruiseTime/1000)

  // Speed is in RPM * 100, need to convert
  // cruiseSpeed (steps/sec) = 36000 / (0.5 + cruiseTime/1000)
  // cruiseSpeed (RPM) = (steps/sec) * 60 / 36000
  // cruiseSpeed (scaled) = RPM * 100

  float effectiveTime = (revAccelTime + revDecelTime) / 2000.0f + cruiseTime / 1000.0f;
  float stepsPerSec = STEPS_PER_REV / effectiveTime;
  float rpm = (stepsPerSec * 60.0f) / STEPS_PER_REV;
  revCruiseSpeed = (int32_t)(rpm * 100);

  // Get starting position
  revStartPos = motorGetPosition();
  revTargetPos = revStartPos + STEPS_PER_REV;
  revDecelStartPos = revTargetPos - (STEPS_PER_REV / 4);  // Start decel at ~270° (90° to slow down)

  revStartTime = millis();
  revState = REV_ACCELERATING;

  Serial.println("\n=== REVOLUTION START ===");
  Serial.printf("Duration: %lu ms\n", duration);
  Serial.printf("Cruise speed: %.1f RPM\n", revCruiseSpeed / 100.0f);
  Serial.printf("Start pos: %d (%.1f°)\n", revStartPos, revStartPos * 360.0f / STEPS_PER_REV);
  Serial.printf("Target pos: %d (%.1f°)\n", revTargetPos, revTargetPos * 360.0f / STEPS_PER_REV);

  setLED(0, 0, 50);  // Blue = moving
}

void updateRevolution() {
  if (revState == REV_IDLE) return;

  unsigned long elapsed = millis() - revStartTime;
  int32_t currentPos = motorGetPosition();
  int32_t traveled = currentPos - revStartPos;

  switch (revState) {
    case REV_ACCELERATING: {
      // S-curve acceleration (smooth start)
      float t = min(1.0f, (float)elapsed / revAccelTime);
      float eased = smootherstep(t);  // Smooth S-curve
      int32_t speed = (int32_t)(revCruiseSpeed * eased);
      motorSetSpeed(speed);

      if (elapsed >= revAccelTime) {
        revState = REV_CRUISING;
        motorSetSpeed(revCruiseSpeed);
        Serial.printf("Cruising at %.1f RPM\n", revCruiseSpeed / 100.0f);
      }
      break;
    }

    case REV_CRUISING: {
      // Check if we should start decelerating
      // Start decel when we have ~90° left (9000 steps) for smoother stop
      int32_t remaining = revTargetPos - currentPos;

      if (remaining <= STEPS_PER_REV / 4) {  // 90° = 9000 steps
        revState = REV_DECELERATING;
        revDecelStartPos = currentPos;
        Serial.printf("Decelerating... (%.1f° remaining)\n", remaining * 360.0f / STEPS_PER_REV);
      }
      break;
    }

    case REV_DECELERATING: {
      // S-curve deceleration (smooth approach to zero)
      int32_t remaining = revTargetPos - currentPos;
      int32_t decelDistance = STEPS_PER_REV / 4;  // 90° decel zone

      if (remaining <= 100) {  // Within ~1° of target
        // Reached target
        revState = REV_STOPPING;
        motorStop();
      } else {
        // t goes from 1.0 (start of decel) to 0.0 (at target)
        float t = (float)remaining / decelDistance;
        t = constrain(t, 0.0f, 1.0f);

        // Select easing function based on mode
        float speedFactor;
        switch (easingMode) {
          case 1:  // Cubic ease-out
            speedFactor = easeOutCubic(t);
            break;
          case 2:  // Quint ease-out (smoothest)
            speedFactor = easeOutQuint(t);
            break;
          default:  // Smootherstep (balanced)
            speedFactor = smootherstep(t);
            break;
        }

        // Minimum speed to keep moving (but very slow near end)
        int32_t minSpeed = 30;  // 0.3 RPM minimum
        int32_t speed = max(minSpeed, (int32_t)(revCruiseSpeed * speedFactor));
        motorSetSpeed(speed);
      }
      break;
    }

    case REV_STOPPING: {
      // Final stop
      motorStop();

      int32_t endPos = motorGetPosition();
      int32_t error = endPos - revTargetPos;
      float errorDeg = error * 360.0f / STEPS_PER_REV;

      revolutionCount++;
      lastEndPos = endPos;

      Serial.println("\n=== REVOLUTION COMPLETE ===");
      Serial.printf("End pos: %d (%.1f°)\n", endPos, endPos * 360.0f / STEPS_PER_REV);
      Serial.printf("Target was: %d (%.1f°)\n", revTargetPos, revTargetPos * 360.0f / STEPS_PER_REV);
      Serial.printf("Error: %d steps (%.2f°)\n", error, errorDeg);
      Serial.printf("Revolution #%d\n", revolutionCount);
      Serial.println("===========================\n");

      revState = REV_IDLE;
      setLED(0, 50, 0);  // Green = ready
      break;
    }

    default:
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

  String html = R"(<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>First Witness</title>
<style>
body{font-family:Arial;background:#1a1a2e;color:#eee;padding:15px;max-width:500px;margin:0 auto}
.card{background:#16213e;padding:12px;border-radius:10px;margin:8px 0}
.value{font-size:20px;font-weight:bold;color:#e94560}
button{background:#e94560;color:#fff;border:none;padding:15px 20px;
font-size:16px;border-radius:5px;margin:5px;cursor:pointer}
button:active{background:#c73e54}
.grn{background:#4CAF50}
.blu{background:#2196F3}
h1{color:#e94560;text-align:center}
h3{margin:5px 0;color:#e94560}
.status-row{display:flex;justify-content:space-between;margin:5px 0}
.row{display:flex;justify-content:center;flex-wrap:wrap}
</style>
<script>
function cmd(c){fetch('/cmd?c='+c);}
setInterval(()=>location.reload(),2000);
</script></head><body>
)";

  html += "<h1>First Witness</h1>";

  html += "<div class='card'>";
  html += "<div class='status-row'>Position: <span class='value'>" + String(degrees, 1) + " deg</span></div>";
  html += "<div class='status-row'>Voltage: <span class='value'>" + String(voltage/100.0f, 1) + " V</span></div>";
  html += "<div class='status-row'>Revolutions: <span class='value'>" + String(revolutionCount) + "</span></div>";
  html += "</div>";

  html += "<div class='card'><h3>Revolution Test</h3><div class='row'>";
  html += "<button onclick=\"cmd('1')\" class='grn'>360° in 5s</button>";
  html += "<button onclick=\"cmd('2')\" class='blu'>360° in 3s</button>";
  html += "<button onclick=\"cmd('3')\">360° in 2s</button>";
  html += "</div></div>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleCmd() {
  if (server.hasArg("c")) {
    char cmd = server.arg("c").charAt(0);
    switch (cmd) {
      case '1': startRevolution(5000); break;  // 5 seconds
      case '2': startRevolution(3000); break;  // 3 seconds
      case '3': startRevolution(2000); break;  // 2 seconds
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

  Serial.println("\n========================================");
  Serial.println("  FIRST WITNESS - SMOOTH REVOLUTION");
  Serial.println("========================================");
  Serial.println("Revolution commands:");
  Serial.println("  1 - 360° in 5 seconds");
  Serial.println("  2 - 360° in 3 seconds");
  Serial.println("  3 - 360° in 2 seconds");
  Serial.println("Easing modes (for deceleration):");
  Serial.println("  a - Smootherstep (default, balanced)");
  Serial.println("  b - Cubic ease-out");
  Serial.println("  c - Quint ease-out (smoothest)");
  Serial.println("Other:");
  Serial.println("  ? - Show status");
  Serial.println("========================================\n");

  // I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  // Check motor
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: Motor not found!");
    while(1) delay(1000);
  }

  // Init motor
  motorInit();
  setLED(0, 50, 0);  // Green = ready

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

  Serial.println("\nReady! Press 1, 2, or 3 to test.\n");
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
      case '1': startRevolution(5000); break;  // 5 seconds
      case '2': startRevolution(3000); break;  // 3 seconds
      case '3': startRevolution(2000); break;  // 2 seconds
      case 'a':
        easingMode = 0;
        Serial.println("Easing: Smootherstep (balanced)");
        break;
      case 'b':
        easingMode = 1;
        Serial.println("Easing: Cubic ease-out");
        break;
      case 'c':
        easingMode = 2;
        Serial.println("Easing: Quint ease-out (smoothest)");
        break;
      case '?':
        Serial.printf("Position: %d (%.1f°)\n",
          motorGetPosition(),
          motorGetPosition() * 360.0f / STEPS_PER_REV);
        Serial.printf("Revolutions completed: %d\n", revolutionCount);
        Serial.printf("Easing mode: %d (%s)\n", easingMode,
          easingMode == 0 ? "smootherstep" :
          easingMode == 1 ? "cubic" : "quint");
        break;
    }
  }

  // Update revolution state machine
  updateRevolution();

  delay(10);  // 100Hz update rate
}
