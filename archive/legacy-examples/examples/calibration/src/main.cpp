/*
 * ============================================================================
 * FIRST WITNESS - MOTOR CALIBRATION TOOL
 * ============================================================================
 *
 * Interactive tool to calibrate and test the M5Stack RollerCAN BLDC motor.
 *
 * FEATURES:
 * - Manual jog control (forward/reverse)
 * - Adjustable speed
 * - Position readout in steps and degrees
 * - Set current position as zero
 * - Test specific angles (90°, 180°, 360°)
 * - Motor release for gravity calibration
 * - Web interface for wireless control
 *
 * HARDWARE:
 * - Seeed XIAO ESP32-S3
 * - M5Stack RollerCAN BLDC (I2C)
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

// =============================================================================
// STATE
// =============================================================================

WebServer server(80);

int32_t currentSpeed = 0;
int32_t targetSpeed = 0;
int32_t speedSetting = 300;  // Default 3 RPM (scaled x100)
int32_t currentLimit = 150000;  // 1.5A
bool motorEnabled = false;
bool isJogging = false;

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

int32_t motorGetTemp() {
  return readReg32(REG_TEMP);
}

void motorRelease() {
  writeReg8(REG_OUTPUT, 0);
  motorEnabled = false;
  isJogging = false;
  setLED(50, 0, 50);  // Purple = released
  Serial.println("Motor RELEASED (free spin)");
}

void motorEnable() {
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
  setLED(0, 50, 0);  // Green = ready
  Serial.println("Motor ENABLED");
}

void motorSetSpeed(int32_t speed) {
  currentSpeed = speed;
  writeReg32(REG_SPEED, speed);
}

void motorStop() {
  motorSetSpeed(0);
  isJogging = false;
  setLED(0, 50, 0);  // Green = ready
}

void motorSetZero() {
  writeReg32(REG_POS, 0);
  Serial.println("Position set to ZERO");
}

void motorJogForward() {
  if (!motorEnabled) motorEnable();
  motorSetSpeed(speedSetting);
  isJogging = true;
  setLED(0, 0, 50);  // Blue = moving
  Serial.printf("Jogging FORWARD at %.1f RPM\n", speedSetting / 100.0f);
}

void motorJogReverse() {
  if (!motorEnabled) motorEnable();
  motorSetSpeed(-speedSetting);
  isJogging = true;
  setLED(0, 0, 50);  // Blue = moving
  Serial.printf("Jogging REVERSE at %.1f RPM\n", speedSetting / 100.0f);
}

void motorMoveAngle(float degrees) {
  if (!motorEnabled) motorEnable();

  int32_t startPos = motorGetPosition();
  int32_t steps = (int32_t)(degrees * STEPS_PER_REV / 360.0f);
  int32_t targetPos = startPos + steps;

  Serial.printf("Moving %.1f° (%d steps)\n", degrees, steps);
  Serial.printf("Start: %d, Target: %d\n", startPos, targetPos);

  // Simple move with speed mode
  int32_t direction = (steps > 0) ? 1 : -1;
  motorSetSpeed(speedSetting * direction);
  setLED(0, 0, 50);  // Blue = moving

  // Wait until we reach target (with timeout)
  unsigned long startTime = millis();
  unsigned long timeout = 30000;  // 30 second timeout

  while (millis() - startTime < timeout) {
    int32_t pos = motorGetPosition();
    int32_t remaining = targetPos - pos;

    // Check if we've passed the target
    if ((direction > 0 && pos >= targetPos) ||
        (direction < 0 && pos <= targetPos)) {
      break;
    }

    // Slow down when close
    if (abs(remaining) < 3000) {  // Within 30°
      motorSetSpeed((speedSetting / 2) * direction);
    }

    delay(10);
  }

  motorStop();

  int32_t endPos = motorGetPosition();
  int32_t error = endPos - targetPos;
  Serial.printf("End: %d, Error: %d steps (%.2f°)\n",
    endPos, error, error * 360.0f / STEPS_PER_REV);
}

// =============================================================================
// PRINT STATUS
// =============================================================================

void printStatus() {
  int32_t pos = motorGetPosition();
  int32_t speed = motorGetSpeed();
  int32_t voltage = motorGetVoltage();
  int32_t temp = motorGetTemp();

  float degrees = pos * 360.0f / STEPS_PER_REV;
  // Normalize to 0-360
  while (degrees < 0) degrees += 360;
  while (degrees >= 360) degrees -= 360;

  Serial.println("\n=== MOTOR STATUS ===");
  Serial.printf("Position: %d steps (%.1f°)\n", pos, degrees);
  Serial.printf("Raw degrees: %.1f°\n", pos * 360.0f / STEPS_PER_REV);
  Serial.printf("Speed: %.1f RPM\n", speed / 100.0f);
  Serial.printf("Voltage: %.1f V\n", voltage / 100.0f);
  Serial.printf("Temperature: %.1f °C\n", temp / 100.0f);
  Serial.printf("Speed setting: %.1f RPM\n", speedSetting / 100.0f);
  Serial.printf("Motor: %s\n", motorEnabled ? "ENABLED" : "RELEASED");
  Serial.println("====================\n");
}

// =============================================================================
// WEB INTERFACE
// =============================================================================

void handleRoot() {
  int32_t pos = motorGetPosition();
  float degrees = pos * 360.0f / STEPS_PER_REV;
  int32_t voltage = motorGetVoltage();
  int32_t speed = motorGetSpeed();

  String html = R"(<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>Motor Calibration</title>
<style>
body{font-family:Arial;background:#1a1a2e;color:#eee;padding:15px;max-width:500px;margin:0 auto}
.card{background:#16213e;padding:12px;border-radius:10px;margin:8px 0}
.value{font-size:24px;font-weight:bold;color:#e94560}
button{background:#e94560;color:#fff;border:none;padding:15px 20px;
font-size:16px;border-radius:5px;margin:5px;cursor:pointer;width:100%}
button:active{background:#c73e54}
.grn{background:#4CAF50}
.blu{background:#2196F3}
.org{background:#FF9800}
.prp{background:#9C27B0}
.row{display:flex;gap:10px}
.row button{flex:1}
h1{color:#e94560;text-align:center;margin:10px 0}
h3{margin:8px 0;color:#e94560}
.status-row{display:flex;justify-content:space-between;margin:5px 0}
</style>
<script>
function cmd(c){fetch('/cmd?c='+c).then(()=>setTimeout(()=>location.reload(),200));}
function setSpd(s){fetch('/speed?v='+s).then(()=>location.reload());}
</script></head><body>
)";

  html += "<h1>Motor Calibration</h1>";

  // Status
  html += "<div class='card'>";
  html += "<div class='status-row'>Position: <span class='value'>" + String(degrees, 1) + "°</span></div>";
  html += "<div class='status-row'>Steps: <span class='value'>" + String(pos) + "</span></div>";
  html += "<div class='status-row'>Speed: <span class='value'>" + String(speed/100.0f, 1) + " RPM</span></div>";
  html += "<div class='status-row'>Voltage: <span class='value'>" + String(voltage/100.0f, 1) + " V</span></div>";
  html += "<div class='status-row'>Motor: <span class='value'>" + String(motorEnabled ? "ON" : "OFF") + "</span></div>";
  html += "</div>";

  // Jog controls
  html += "<div class='card'><h3>Jog Control</h3>";
  html += "<div class='row'>";
  html += "<button onclick=\"cmd('jf')\" class='blu'>← Forward</button>";
  html += "<button onclick=\"cmd('stop')\" class='org'>STOP</button>";
  html += "<button onclick=\"cmd('jr')\" class='blu'>Reverse →</button>";
  html += "</div></div>";

  // Move angles
  html += "<div class='card'><h3>Move Angle</h3>";
  html += "<div class='row'>";
  html += "<button onclick=\"cmd('m90')\">+90°</button>";
  html += "<button onclick=\"cmd('m180')\">+180°</button>";
  html += "<button onclick=\"cmd('m360')\">+360°</button>";
  html += "</div>";
  html += "<div class='row'>";
  html += "<button onclick=\"cmd('m-90')\">-90°</button>";
  html += "<button onclick=\"cmd('m-180')\">-180°</button>";
  html += "<button onclick=\"cmd('m-360')\">-360°</button>";
  html += "</div></div>";

  // Speed setting
  html += "<div class='card'><h3>Speed: " + String(speedSetting/100.0f, 1) + " RPM</h3>";
  html += "<div class='row'>";
  html += "<button onclick=\"setSpd(100)\">1</button>";
  html += "<button onclick=\"setSpd(200)\">2</button>";
  html += "<button onclick=\"setSpd(300)\">3</button>";
  html += "<button onclick=\"setSpd(500)\">5</button>";
  html += "<button onclick=\"setSpd(1000)\">10</button>";
  html += "</div></div>";

  // Calibration
  html += "<div class='card'><h3>Calibration</h3>";
  html += "<button onclick=\"cmd('zero')\" class='grn'>SET ZERO HERE</button>";
  html += "<button onclick=\"cmd('release')\" class='prp'>RELEASE MOTOR</button>";
  html += "<button onclick=\"cmd('enable')\" class='grn'>ENABLE MOTOR</button>";
  html += "</div>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleCmd() {
  if (server.hasArg("c")) {
    String cmd = server.arg("c");

    if (cmd == "jf") motorJogForward();
    else if (cmd == "jr") motorJogReverse();
    else if (cmd == "stop") motorStop();
    else if (cmd == "zero") motorSetZero();
    else if (cmd == "release") motorRelease();
    else if (cmd == "enable") motorEnable();
    else if (cmd == "m90") motorMoveAngle(90);
    else if (cmd == "m180") motorMoveAngle(180);
    else if (cmd == "m360") motorMoveAngle(360);
    else if (cmd == "m-90") motorMoveAngle(-90);
    else if (cmd == "m-180") motorMoveAngle(-180);
    else if (cmd == "m-360") motorMoveAngle(-360);
  }
  server.send(200, "text/plain", "OK");
}

void handleSpeed() {
  if (server.hasArg("v")) {
    speedSetting = server.arg("v").toInt();
    Serial.printf("Speed set to %.1f RPM\n", speedSetting / 100.0f);
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
  Serial.println("  FIRST WITNESS - MOTOR CALIBRATION TOOL");
  Serial.println("================================================");
  Serial.println();
  Serial.println("SERIAL COMMANDS:");
  Serial.println("  f - Jog forward");
  Serial.println("  r - Jog reverse");
  Serial.println("  s - Stop");
  Serial.println("  z - Set zero here");
  Serial.println("  x - Release motor (free spin)");
  Serial.println("  e - Enable motor");
  Serial.println("  ? - Print status");
  Serial.println("  1-9 - Set speed (RPM)");
  Serial.println("  + - Move +90°");
  Serial.println("  - - Move -90°");
  Serial.println("================================================\n");

  // I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  // Check motor
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: Motor not found!");
    setLED(50, 0, 0);  // Red = error
    while(1) delay(1000);
  }
  Serial.println("Motor found!");

  // Start with motor released
  motorRelease();

  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP("WitnessCalibrate", "calibrate123");
  delay(500);
  Serial.printf("\nWiFi: WitnessCalibrate / calibrate123\n");
  Serial.printf("URL: http://%s\n\n", WiFi.softAPIP().toString().c_str());

  // Web server
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.on("/speed", handleSpeed);
  server.begin();

  printStatus();
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
      case 'f':
      case 'F':
        motorJogForward();
        break;

      case 'r':
      case 'R':
        motorJogReverse();
        break;

      case 's':
      case 'S':
      case ' ':
        motorStop();
        break;

      case 'z':
      case 'Z':
        motorSetZero();
        break;

      case 'x':
      case 'X':
        motorRelease();
        break;

      case 'e':
      case 'E':
        motorEnable();
        break;

      case '?':
        printStatus();
        break;

      case '1': speedSetting = 100; Serial.println("Speed: 1 RPM"); break;
      case '2': speedSetting = 200; Serial.println("Speed: 2 RPM"); break;
      case '3': speedSetting = 300; Serial.println("Speed: 3 RPM"); break;
      case '4': speedSetting = 400; Serial.println("Speed: 4 RPM"); break;
      case '5': speedSetting = 500; Serial.println("Speed: 5 RPM"); break;
      case '6': speedSetting = 600; Serial.println("Speed: 6 RPM"); break;
      case '7': speedSetting = 700; Serial.println("Speed: 7 RPM"); break;
      case '8': speedSetting = 800; Serial.println("Speed: 8 RPM"); break;
      case '9': speedSetting = 900; Serial.println("Speed: 9 RPM"); break;
      case '0': speedSetting = 1000; Serial.println("Speed: 10 RPM"); break;

      case '+':
      case '=':
        motorMoveAngle(90);
        break;

      case '-':
      case '_':
        motorMoveAngle(-90);
        break;
    }
  }

  delay(10);
}
