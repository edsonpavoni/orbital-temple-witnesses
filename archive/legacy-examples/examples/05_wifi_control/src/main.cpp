/*
 * ============================================================================
 * ORBITAL TEMPLE - WIFI MOTOR CONTROL
 * ============================================================================
 *
 * Control RollerCAN motor via WiFi - set velocity and rotation angle.
 *
 * FEATURES:
 * ---------
 * - Web interface for motor control
 * - Set velocity (RPM) with smooth transitions
 * - Set target angle for precise positioning
 * - Real-time position and speed monitoring
 * - API endpoints for programmatic control
 * - WiFi configuration portal
 * - mDNS discovery (witness-ctrl.local)
 *
 * CONTROL MODES:
 * --------------
 * 1. VELOCITY MODE: Set continuous rotation speed in RPM
 * 2. POSITION MODE: Rotate to specific angle (0-360°)
 * 3. RELATIVE MOVE: Rotate by specific degrees from current position
 *
 * WEB INTERFACE:
 * --------------
 * After connecting to WiFi, access the control panel at:
 * - http://witness-ctrl.local (mDNS)
 * - http://[device-ip] (IP address shown in serial monitor)
 *
 * API ENDPOINTS:
 * --------------
 * GET  /status              - Get current motor status
 * POST /velocity?rpm=X      - Set velocity (RPM, can be negative)
 * POST /angle?deg=X         - Rotate to absolute angle (0-360°)
 * POST /rotate?deg=X        - Rotate by relative angle (can be negative)
 * POST /stop                - Stop motor immediately
 * POST /reset               - Reset position to 0°
 * POST /home                - Return to home position (0°)
 *
 * EXAMPLES:
 * ---------
 * curl http://witness-ctrl.local/velocity?rpm=100
 * curl http://witness-ctrl.local/angle?deg=180
 * curl http://witness-ctrl.local/rotate?deg=90
 * curl http://witness-ctrl.local/stop
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

// Device Settings
#define DEVICE_HOSTNAME "witness-ctrl"
#define DEVICE_NAME "Witness Control"

// WiFi AP settings
#define AP_SSID "Witness-Control"
#define AP_PASSWORD "witness123"

// I2C Settings
#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8
#define I2C_SCL_PIN      9
#define I2C_FREQ         100000

// Motor registers
#define REG_OUTPUT       0x00
#define REG_MODE         0x01
#define REG_STALL_PROT   0x0F
#define REG_SPEED        0x40
#define REG_SPEED_MAXCUR 0x50
#define REG_SPEED_READ   0x60
#define REG_POS          0x80
#define REG_POS_MAXCUR   0x88
#define REG_POS_READ     0x90

#define MODE_SPEED       1
#define MODE_POSITION    2

// Motion settings
#define MAX_CURRENT_MA   1000     // Maximum current limit
#define VELOCITY_LIMIT   500      // Maximum RPM limit
#define UPDATE_RATE_MS   50       // Status update rate

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

WebServer server(80);
Preferences preferences;

bool wifiConnected = false;
bool apMode = false;
bool motorConnected = false;

// Motor state
enum ControlMode {
  MODE_IDLE,
  MODE_VELOCITY,
  MODE_POSITION_ABSOLUTE,
  MODE_POSITION_RELATIVE
};

ControlMode controlMode = MODE_IDLE;
float currentRPM = 0;
float targetRPM = 0;
float currentPosition = 0;  // degrees
float targetPosition = 0;   // degrees
int32_t positionOffset = 0; // Motor position offset for zero calibration
unsigned long lastUpdate = 0;

// =============================================================================
// FUNCTION PROTOTYPES
// =============================================================================

void setupWiFi();
void setupWebServer();
void setupMotor();
void updateMotor();
void setVelocity(float rpm);
void setAngle(float degrees);
void rotateBy(float degrees);
void stopMotor();
void resetPosition();
float readPosition();
float readSpeed();

void handleRoot();
void handleStatus();
void handleVelocity();
void handleAngle();
void handleRotate();
void handleStop();
void handleReset();
void handleHome();

void writeReg8(uint8_t reg, uint8_t value);
void writeReg32(uint8_t reg, int32_t value);
int32_t readReg32(uint8_t reg);

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  ORBITAL TEMPLE - WIFI MOTOR CONTROL");
  Serial.println("========================================");
  Serial.println();

  // Setup motor first
  setupMotor();

  // Setup WiFi
  setupWiFi();

  // Setup web server
  setupWebServer();

  Serial.println();
  Serial.println("========================================");
  Serial.println("  READY!");
  Serial.println("========================================");
  if (wifiConnected) {
    Serial.print("Web Interface: http://");
    Serial.print(DEVICE_HOSTNAME);
    Serial.println(".local");
    Serial.print("          or: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.print("AP Mode: http://");
    Serial.println(WiFi.softAPIP());
  }
  Serial.println("========================================");
  Serial.println();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  server.handleClient();

  if (millis() - lastUpdate >= UPDATE_RATE_MS) {
    lastUpdate = millis();
    updateMotor();

    // Periodic status output
    static int counter = 0;
    if (++counter >= 20) {  // Every 1 second (20 * 50ms)
      counter = 0;
      Serial.print("Mode: ");
      switch(controlMode) {
        case MODE_IDLE: Serial.print("IDLE"); break;
        case MODE_VELOCITY: Serial.print("VELOCITY"); break;
        case MODE_POSITION_ABSOLUTE: Serial.print("POSITION"); break;
        case MODE_POSITION_RELATIVE: Serial.print("RELATIVE"); break;
      }
      Serial.print(" | RPM: ");
      Serial.print(currentRPM, 1);
      Serial.print(" | Pos: ");
      Serial.print(currentPosition, 1);
      Serial.println("°");
    }
  }
}

// =============================================================================
// MOTOR CONTROL
// =============================================================================

void setupMotor() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: RollerCAN motor not found!");
    Serial.println("Check wiring and I2C mode configuration.");
    motorConnected = false;
    while (1) delay(1000);
  }

  motorConnected = true;
  Serial.println("Motor connected!");

  // Initialize motor
  writeReg8(REG_OUTPUT, 0);
  delay(50);
  writeReg8(REG_STALL_PROT, 0);
  delay(50);

  // Start in speed mode
  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);
  writeReg32(REG_SPEED_MAXCUR, MAX_CURRENT_MA * 100);
  delay(50);
  writeReg32(REG_POS_MAXCUR, MAX_CURRENT_MA * 100);
  delay(50);
  writeReg32(REG_SPEED, 0);
  delay(50);

  writeReg8(REG_OUTPUT, 1);
  delay(50);

  // Read initial position
  int32_t rawPos = readReg32(REG_POS_READ);
  positionOffset = rawPos;
  currentPosition = 0;

  Serial.println("Motor initialized!");
  Serial.print("Position offset: ");
  Serial.println(positionOffset);
}

void updateMotor() {
  if (!motorConnected) return;

  // Read current values
  currentRPM = readSpeed();
  currentPosition = readPosition();

  // Check if position mode target reached
  if (controlMode == MODE_POSITION_ABSOLUTE || controlMode == MODE_POSITION_RELATIVE) {
    float error = targetPosition - currentPosition;

    // Normalize error to -180 to +180
    while (error > 180) error -= 360;
    while (error < -180) error += 360;

    // Consider arrived if within 2 degrees and speed is low
    if (abs(error) < 2.0 && abs(currentRPM) < 5.0) {
      if (controlMode != MODE_IDLE) {
        Serial.println("Position reached!");
        controlMode = MODE_IDLE;
        setVelocity(0);
      }
    }
  }
}

void setVelocity(float rpm) {
  if (!motorConnected) return;

  // Limit velocity
  if (rpm > VELOCITY_LIMIT) rpm = VELOCITY_LIMIT;
  if (rpm < -VELOCITY_LIMIT) rpm = -VELOCITY_LIMIT;

  targetRPM = rpm;
  controlMode = MODE_VELOCITY;

  // Switch to speed mode
  writeReg8(REG_OUTPUT, 0);
  delay(20);
  writeReg8(REG_MODE, MODE_SPEED);
  delay(20);
  writeReg32(REG_SPEED, (int32_t)(rpm * 100));
  delay(20);
  writeReg8(REG_OUTPUT, 1);

  Serial.print("Velocity set to: ");
  Serial.print(rpm);
  Serial.println(" RPM");
}

void setAngle(float degrees) {
  if (!motorConnected) return;

  // Normalize to 0-360
  while (degrees < 0) degrees += 360;
  while (degrees >= 360) degrees -= 360;

  targetPosition = degrees;
  controlMode = MODE_POSITION_ABSOLUTE;

  // Switch to position mode
  int32_t targetPos = positionOffset + (int32_t)(degrees * 100);

  writeReg8(REG_OUTPUT, 0);
  delay(20);
  writeReg8(REG_MODE, MODE_POSITION);
  delay(20);
  writeReg32(REG_POS, targetPos);
  delay(20);
  writeReg8(REG_OUTPUT, 1);

  Serial.print("Moving to angle: ");
  Serial.print(degrees);
  Serial.println("°");
}

void rotateBy(float degrees) {
  if (!motorConnected) return;

  float newTarget = currentPosition + degrees;

  // Normalize to 0-360
  while (newTarget < 0) newTarget += 360;
  while (newTarget >= 360) newTarget -= 360;

  targetPosition = newTarget;
  controlMode = MODE_POSITION_RELATIVE;

  // Switch to position mode
  int32_t targetPos = positionOffset + (int32_t)(newTarget * 100);

  writeReg8(REG_OUTPUT, 0);
  delay(20);
  writeReg8(REG_MODE, MODE_POSITION);
  delay(20);
  writeReg32(REG_POS, targetPos);
  delay(20);
  writeReg8(REG_OUTPUT, 1);

  Serial.print("Rotating by ");
  Serial.print(degrees);
  Serial.print("° to ");
  Serial.print(newTarget);
  Serial.println("°");
}

void stopMotor() {
  if (!motorConnected) return;

  controlMode = MODE_IDLE;
  writeReg32(REG_SPEED, 0);
  Serial.println("Motor stopped");
}

void resetPosition() {
  if (!motorConnected) return;

  int32_t rawPos = readReg32(REG_POS_READ);
  positionOffset = rawPos;
  currentPosition = 0;
  Serial.println("Position reset to 0°");
}

float readPosition() {
  if (!motorConnected) return 0;

  int32_t rawPos = readReg32(REG_POS_READ);
  int32_t relativePos = rawPos - positionOffset;
  float degrees = relativePos / 100.0f;

  // Normalize to 0-360
  while (degrees < 0) degrees += 360;
  while (degrees >= 360) degrees -= 360;

  return degrees;
}

float readSpeed() {
  if (!motorConnected) return 0;
  return readReg32(REG_SPEED_READ) / 100.0f;
}

// =============================================================================
// WIFI SETUP
// =============================================================================

void setupWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  preferences.begin("witness", false);
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");
  preferences.end();

  if (ssid.length() > 0) {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(DEVICE_HOSTNAME);
    WiFi.begin(ssid.c_str(), password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.println();
      Serial.print("Connected! IP: ");
      Serial.println(WiFi.localIP());

      if (MDNS.begin(DEVICE_HOSTNAME)) {
        Serial.print("mDNS started: http://");
        Serial.print(DEVICE_HOSTNAME);
        Serial.println(".local");
        MDNS.addService("http", "tcp", 80);
      }
      return;
    } else {
      Serial.println();
      Serial.println("WiFi connection failed.");
      WiFi.disconnect(true);
    }
  } else {
    Serial.println("No saved WiFi credentials.");
  }

  // Start AP mode
  Serial.println("Starting AP mode...");
  WiFi.mode(WIFI_AP);
  delay(100);

  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  apMode = true;
  Serial.print("AP: ");
  Serial.print(AP_SSID);
  Serial.print(" / ");
  Serial.println(AP_PASSWORD);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
}

// =============================================================================
// WEB SERVER
// =============================================================================

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/velocity", HTTP_POST, handleVelocity);
  server.on("/angle", HTTP_POST, handleAngle);
  server.on("/rotate", HTTP_POST, handleRotate);
  server.on("/stop", HTTP_POST, handleStop);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/home", HTTP_POST, handleHome);

  server.on("/wifi", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    if (ssid.length() > 0) {
      preferences.begin("witness", false);
      preferences.putString("ssid", ssid);
      preferences.putString("password", password);
      preferences.end();

      server.send(200, "text/html",
        "<html><body style='font-family:Arial;background:#1a1a2e;color:#eee'>"
        "<h1 style='color:#e94560'>WiFi Saved!</h1>"
        "<p>Restarting...</p></body></html>");

      delay(1000);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "SSID required");
    }
  });

  server.begin();
  Serial.println("Web server started");
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>" DEVICE_NAME "</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#1a1a2e;color:#eee}";
  html += "h1{color:#e94560;margin-top:0}h2{color:#f39c12}";
  html += ".card{background:#16213e;padding:20px;border-radius:10px;margin:15px 0;box-shadow:0 4px 6px rgba(0,0,0,0.3)}";
  html += ".status{font-size:20px;line-height:2;font-weight:bold}";
  html += ".controls{display:grid;gap:10px;margin:15px 0}";
  html += "button{background:#e94560;color:white;border:none;padding:15px;font-size:16px;";
  html += "border-radius:5px;cursor:pointer;transition:all 0.3s}";
  html += "button:hover{background:#c73e54;transform:translateY(-2px)}";
  html += "button:active{transform:translateY(0)}";
  html += "button.secondary{background:#0f3460}button.secondary:hover{background:#16213e}";
  html += "button.danger{background:#e74c3c}button.danger:hover{background:#c0392b}";
  html += "input[type=number],input[type=text],input[type=password]{padding:12px;font-size:16px;";
  html += "border:2px solid #0f3460;border-radius:5px;background:#0f3460;color:#eee;width:100%;box-sizing:border-box}";
  html += "input[type=range]{width:100%;height:40px;-webkit-appearance:none;background:transparent}";
  html += "input[type=range]::-webkit-slider-track{background:#0f3460;height:8px;border-radius:4px}";
  html += "input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:24px;height:24px;";
  html += "background:#e94560;border-radius:50%;cursor:pointer;margin-top:-8px}";
  html += ".slider-container{margin:20px 0}.slider-label{display:flex;justify-content:space-between;";
  html += "margin-bottom:10px;font-size:18px}.value{color:#e94560;font-weight:bold}";
  html += ".mode{display:inline-block;padding:5px 15px;border-radius:15px;background:#0f3460;";
  html += "color:#2ecc71;font-weight:bold;font-size:14px}";
  html += "@media(min-width:768px){.controls{grid-template-columns:repeat(2,1fr)}}";
  html += "</style></head><body>";

  html += "<h1>🛰️ " DEVICE_NAME "</h1>";

  html += "<div class='card'><h2>Status</h2><div class='status' id='status'>Loading...</div></div>";

  html += "<div class='card'><h2>Velocity Control</h2>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Speed</span><span class='value' id='velValue'>0 RPM</span></div>";
  html += "<input type='range' id='velocity' min='-200' max='200' value='0' step='5'>";
  html += "</div>";
  html += "<div class='controls'>";
  html += "<button onclick='setVelocity()'>Set Velocity</button>";
  html += "<button onclick='stopMotor()' class='danger'>STOP</button>";
  html += "</div></div>";

  html += "<div class='card'><h2>Position Control</h2>";
  html += "<div class='slider-container'>";
  html += "<div class='slider-label'><span>Target Angle</span><span class='value' id='angleValue'>0°</span></div>";
  html += "<input type='range' id='angle' min='0' max='360' value='0' step='5'>";
  html += "</div>";
  html += "<button onclick='setAngle()' style='width:100%;margin-bottom:15px'>Go to Angle</button>";
  html += "<div class='controls'>";
  html += "<button onclick='rotateBy(90)' class='secondary'>+90°</button>";
  html += "<button onclick='rotateBy(-90)' class='secondary'>-90°</button>";
  html += "<button onclick='rotateBy(180)' class='secondary'>+180°</button>";
  html += "<button onclick='goHome()' class='secondary'>Home (0°)</button>";
  html += "</div></div>";

  html += "<div class='card'><h2>Calibration</h2>";
  html += "<button onclick='resetPosition()' style='width:100%'>Reset Position to 0°</button>";
  html += "</div>";

  if (apMode) {
    html += "<div class='card'><h2>WiFi Setup</h2>";
    html += "<form action='/wifi' method='post' style='display:grid;gap:10px'>";
    html += "<input type='text' name='ssid' placeholder='WiFi Name' required>";
    html += "<input type='password' name='password' placeholder='Password'>";
    html += "<button type='submit'>Connect to WiFi</button>";
    html += "</form></div>";
  }

  html += "<script>";
  html += "const velSlider=document.getElementById('velocity');";
  html += "const velValue=document.getElementById('velValue');";
  html += "const angleSlider=document.getElementById('angle');";
  html += "const angleValue=document.getElementById('angleValue');";
  html += "velSlider.oninput=()=>velValue.textContent=velSlider.value+' RPM';";
  html += "angleSlider.oninput=()=>angleValue.textContent=angleSlider.value+'°';";

  html += "function updateStatus(){";
  html += "fetch('/status').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('status').innerHTML=";
  html += "'<p>Mode: <span class=\"mode\">'+d.mode+'</span></p>'+";
  html += "'<p>Speed: '+d.rpm.toFixed(1)+' RPM</p>'+";
  html += "'<p>Position: '+d.position.toFixed(1)+'°</p>'+";
  html += "'<p>Target: '+d.target.toFixed(1)+'°</p>';";
  html += "}).catch(e=>console.error(e));}";

  html += "function setVelocity(){";
  html += "fetch('/velocity?rpm='+velSlider.value,{method:'POST'})";
  html += ".then(()=>updateStatus());}";

  html += "function setAngle(){";
  html += "fetch('/angle?deg='+angleSlider.value,{method:'POST'})";
  html += ".then(()=>updateStatus());}";

  html += "function rotateBy(deg){";
  html += "fetch('/rotate?deg='+deg,{method:'POST'})";
  html += ".then(()=>updateStatus());}";

  html += "function stopMotor(){";
  html += "fetch('/stop',{method:'POST'})";
  html += ".then(()=>updateStatus());}";

  html += "function goHome(){";
  html += "fetch('/home',{method:'POST'})";
  html += ".then(()=>updateStatus());}";

  html += "function resetPosition(){";
  html += "if(confirm('Reset current position to 0°?')){";
  html += "fetch('/reset',{method:'POST'})";
  html += ".then(()=>updateStatus());}}";

  html += "updateStatus();setInterval(updateStatus,500);";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void handleStatus() {
  StaticJsonDocument<256> doc;

  String mode;
  switch(controlMode) {
    case MODE_IDLE: mode = "IDLE"; break;
    case MODE_VELOCITY: mode = "VELOCITY"; break;
    case MODE_POSITION_ABSOLUTE: mode = "POSITION"; break;
    case MODE_POSITION_RELATIVE: mode = "MOVING"; break;
  }

  doc["mode"] = mode;
  doc["rpm"] = currentRPM;
  doc["position"] = currentPosition;
  doc["target"] = targetPosition;
  doc["connected"] = motorConnected;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleVelocity() {
  if (server.hasArg("rpm")) {
    float rpm = server.arg("rpm").toFloat();
    setVelocity(rpm);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing rpm parameter");
  }
}

void handleAngle() {
  if (server.hasArg("deg")) {
    float deg = server.arg("deg").toFloat();
    setAngle(deg);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing deg parameter");
  }
}

void handleRotate() {
  if (server.hasArg("deg")) {
    float deg = server.arg("deg").toFloat();
    rotateBy(deg);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing deg parameter");
  }
}

void handleStop() {
  stopMotor();
  server.send(200, "text/plain", "OK");
}

void handleReset() {
  resetPosition();
  server.send(200, "text/plain", "OK");
}

void handleHome() {
  setAngle(0);
  server.send(200, "text/plain", "OK");
}

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
