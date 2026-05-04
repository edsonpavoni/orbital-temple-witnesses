/*
 * ============================================================================
 * ORBITAL TEMPLE - FEET SCULPTURE
 * WiFi Control Version
 * ============================================================================
 *
 * DESCRIPTION:
 * ------------
 * Same as simple rotation, but with WiFi control via phone/browser.
 * Creates WiFi Access Point "Feet-XXX" that you can connect to.
 * Then open browser to 192.168.4.1 to control the sculpture.
 *
 * SERIAL COMMANDS (same as simple version):
 * -----------------------------------------
 * g         - GO! Start revolution
 * x         - ABORT! Stop everything
 * r<n>      - Set rotation duration (r30)
 * w<n>      - Set wait time (w30)
 * t<n>      - Set gear raTio (t3.2)
 * m<n>      - Set min speed (m50)
 * c<n>      - Set max current mA (c1000)
 * e<n>      - Set easing 1-10 (e5)
 * s         - Show settings
 *
 * WIFI SETUP:
 * -----------
 * - Change WIFI_SSID and WIFI_PASSWORD below if you want to connect to your WiFi
 * - Or leave empty to create an Access Point instead
 * - AP Mode: Connect phone to "Feet-001", password "sculpture123"
 * - Then browse to 192.168.4.1
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>

// =============================================================================
// WIFI CONFIGURATION
// =============================================================================

// Option 1: Connect to your existing WiFi (set both values)
#define WIFI_SSID ""        // Your WiFi name (leave empty for AP mode)
#define WIFI_PASSWORD ""    // Your WiFi password

// Option 2: Create Access Point (if WIFI_SSID is empty)
#define AP_SSID "Feet-001"
#define AP_PASSWORD "sculpture123"

// =============================================================================
// CONFIGURATION - ADJUSTABLE VIA SERIAL MONITOR OR WEB
// =============================================================================

float rotationDuration = 10.0;       // Seconds for one complete SCULPTURE revolution
float waitTime = 30.0;               // Seconds to wait between revolutions
float gearRatio = 3.2;               // Gear reduction ratio (motor revs : sculpture revs)
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
// WEB SERVER
// =============================================================================

WebServer server(80);
bool apMode = false;

// =============================================================================
// EASING FUNCTIONS
// =============================================================================

float easeInOutCubic(float t) {
  if (t < 0.5) {
    return 4 * t * t * t;
  } else {
    float f = (2 * t - 2);
    return 1 + f * f * f / 2;
  }
}

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

void motorUpdateMaxCurrent() {
  writeReg32(REG_SPEED_MAXCUR, maxCurrent);
}

void motorSetSpeed(int32_t speed) {
  writeReg32(REG_SPEED, speed);
}

void motorEnable(bool enable) {
  writeReg8(REG_OUTPUT, enable ? 1 : 0);
}

// =============================================================================
// STATE MACHINE FUNCTIONS
// =============================================================================

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

String getStateName() {
  switch(currentState) {
    case STATE_ROTATING: return "ROTATING";
    case STATE_WAITING: return "WAITING";
    case STATE_STOPPED: return "STOPPED";
    default: return "UNKNOWN";
  }
}

// =============================================================================
// WEB SERVER HANDLERS
// =============================================================================

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Feet Sculpture</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
      background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
      color: #eee;
      padding: 20px;
      min-height: 100vh;
    }
    .container { max-width: 600px; margin: 0 auto; }
    h1 {
      color: #e94560;
      font-size: 24px;
      margin-bottom: 20px;
      text-align: center;
    }
    .card {
      background: rgba(22, 33, 62, 0.8);
      border-radius: 15px;
      padding: 20px;
      margin-bottom: 15px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.3);
    }
    .status {
      font-size: 18px;
      margin-bottom: 10px;
      padding: 10px;
      background: rgba(255,255,255,0.05);
      border-radius: 8px;
    }
    .status-label { color: #888; }
    .status-value { color: #e94560; font-weight: bold; }
    button {
      width: 100%;
      padding: 18px;
      font-size: 16px;
      font-weight: bold;
      border: none;
      border-radius: 10px;
      cursor: pointer;
      margin-bottom: 10px;
      transition: all 0.3s;
      text-transform: uppercase;
      letter-spacing: 1px;
    }
    .btn-go {
      background: linear-gradient(135deg, #2ecc71 0%, #27ae60 100%);
      color: white;
    }
    .btn-go:active { transform: scale(0.95); }
    .btn-abort {
      background: linear-gradient(135deg, #e74c3c 0%, #c0392b 100%);
      color: white;
    }
    .btn-abort:active { transform: scale(0.95); }
    .control-group {
      margin-bottom: 15px;
    }
    .control-label {
      display: block;
      color: #888;
      font-size: 12px;
      margin-bottom: 8px;
      text-transform: uppercase;
      letter-spacing: 1px;
    }
    input[type="number"], input[type="range"] {
      width: 100%;
      padding: 12px;
      font-size: 16px;
      border: 1px solid rgba(255,255,255,0.1);
      border-radius: 8px;
      background: rgba(255,255,255,0.05);
      color: #eee;
    }
    input[type="range"] {
      padding: 0;
      height: 40px;
    }
    .range-value {
      text-align: center;
      color: #e94560;
      font-size: 18px;
      font-weight: bold;
      margin-top: 5px;
    }
    .btn-set {
      background: linear-gradient(135deg, #3498db 0%, #2980b9 100%);
      color: white;
      padding: 12px;
      font-size: 14px;
    }
    .btn-set:active { transform: scale(0.95); }
  </style>
</head>
<body>
  <div class="container">
    <h1>🦶 FEET SCULPTURE</h1>

    <div class="card">
      <div class="status">
        <span class="status-label">State:</span>
        <span class="status-value" id="state">-</span>
      </div>
      <div class="status">
        <span class="status-label">Rotation:</span>
        <span class="status-value" id="rotation">-</span>
      </div>
      <div class="status">
        <span class="status-label">Wait:</span>
        <span class="status-value" id="wait">-</span>
      </div>
      <div class="status">
        <span class="status-label">Gear Ratio:</span>
        <span class="status-value" id="ratio">-</span>
      </div>
    </div>

    <div class="card">
      <button class="btn-go" onclick="sendCmd('g')">▶ START REVOLUTION</button>
      <button class="btn-abort" onclick="sendCmd('x')">■ ABORT</button>
    </div>

    <div class="card">
      <div class="control-group">
        <label class="control-label">Rotation Duration (seconds)</label>
        <input type="range" id="rotSlider" min="5" max="120" value="10" oninput="updateRotValue()">
        <div class="range-value" id="rotValue">10s</div>
        <button class="btn-set" onclick="setRotation()">SET</button>
      </div>

      <div class="control-group">
        <label class="control-label">Wait Time (seconds)</label>
        <input type="range" id="waitSlider" min="0" max="120" value="30" oninput="updateWaitValue()">
        <div class="range-value" id="waitValue">30s</div>
        <button class="btn-set" onclick="setWait()">SET</button>
      </div>

      <div class="control-group">
        <label class="control-label">Gear Ratio</label>
        <input type="number" id="ratioInput" value="3.2" step="0.1" min="0.1" max="20">
        <button class="btn-set" onclick="setRatio()">SET</button>
      </div>

      <div class="control-group">
        <label class="control-label">Min Speed</label>
        <input type="number" id="minSpeedInput" value="10" step="10" min="0" max="1000">
        <button class="btn-set" onclick="setMinSpeed()">SET</button>
      </div>
    </div>
  </div>

  <script>
    function updateRotValue() {
      document.getElementById('rotValue').textContent = document.getElementById('rotSlider').value + 's';
    }
    function updateWaitValue() {
      document.getElementById('waitValue').textContent = document.getElementById('waitSlider').value + 's';
    }
    function sendCmd(cmd) {
      fetch('/cmd?c=' + cmd).then(() => updateStatus());
    }
    function setRotation() {
      const val = document.getElementById('rotSlider').value;
      fetch('/cmd?c=r' + val).then(() => updateStatus());
    }
    function setWait() {
      const val = document.getElementById('waitSlider').value;
      fetch('/cmd?c=w' + val).then(() => updateStatus());
    }
    function setRatio() {
      const val = document.getElementById('ratioInput').value;
      fetch('/cmd?c=t' + val).then(() => updateStatus());
    }
    function setMinSpeed() {
      const val = document.getElementById('minSpeedInput').value;
      fetch('/cmd?c=m' + val).then(() => updateStatus());
    }
    function updateStatus() {
      fetch('/status').then(r => r.json()).then(data => {
        document.getElementById('state').textContent = data.state;
        document.getElementById('rotation').textContent = data.rotation + 's';
        document.getElementById('wait').textContent = data.wait + 's';
        document.getElementById('ratio').textContent = data.ratio + ':1';
      });
    }
    updateStatus();
    setInterval(updateStatus, 2000);
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleStatus() {
  String json = "{";
  json += "\"state\":\"" + getStateName() + "\",";
  json += "\"rotation\":" + String(rotationDuration) + ",";
  json += "\"wait\":" + String(waitTime) + ",";
  json += "\"ratio\":" + String(gearRatio);
  json += "}";

  server.send(200, "application/json", json);
}

void handleCommand() {
  if (server.hasArg("c")) {
    String cmd = server.arg("c");
    cmd.toLowerCase();
    cmd.trim();

    // Process command (same logic as serial)
    if (cmd == "g") {
      isPaused = false;
      startRotation();
    }
    else if (cmd == "x") {
      abortAll();
    }
    else if (cmd.startsWith("r")) {
      float val = cmd.substring(1).toFloat();
      if (val > 0 && val <= 300) rotationDuration = val;
    }
    else if (cmd.startsWith("w")) {
      float val = cmd.substring(1).toFloat();
      if (val >= 0 && val <= 600) waitTime = val;
    }
    else if (cmd.startsWith("t")) {
      float val = cmd.substring(1).toFloat();
      if (val >= 0.1 && val <= 20.0) gearRatio = val;
    }
    else if (cmd.startsWith("m")) {
      int32_t val = cmd.substring(1).toInt();
      if (val >= 0 && val <= 1000) minSpeed = val;
    }
    else if (cmd.startsWith("c")) {
      int32_t val = cmd.substring(1).toInt();
      if (val >= 100 && val <= 3000) {
        maxCurrent = val * 100;
        motorUpdateMaxCurrent();
      }
    }
    else if (cmd.startsWith("e")) {
      float val = cmd.substring(1).toFloat();
      if (val >= 1.0 && val <= 10.0) easingIntensity = val;
    }
  }

  server.send(200, "text/plain", "OK");
}

// =============================================================================
// SERIAL COMMAND PROCESSING
// =============================================================================

void printSettings() {
  Serial.println("\n========================================");
  Serial.println("        FEET SCULPTURE SETTINGS");
  Serial.println("========================================");
  Serial.print("State: ");
  Serial.println(getStateName());
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
  Serial.print("Easing Intensity: ");
  Serial.println(easingIntensity);
  Serial.println("\nCOMMANDS:");
  Serial.println("  g         - GO! Start revolution");
  Serial.println("  x         - ABORT! Stop everything");
  Serial.println("  r<n>      - Set rotation duration (r30)");
  Serial.println("  w<n>      - Set wait time (w30)");
  Serial.println("  t<n>      - Set gear raTio (t3.2)");
  Serial.println("  m<n>      - Set min speed (m50)");
  Serial.println("  c<n>      - Set max current mA (c1000)");
  Serial.println("  e<n>      - Set easing 1-10 (e5)");
  Serial.println("  s         - Show settings");

  if (apMode) {
    Serial.println("\nWIFI: AP Mode");
    Serial.print("Connect to: ");
    Serial.println(AP_SSID);
    Serial.print("Password: ");
    Serial.println(AP_PASSWORD);
    Serial.print("Then browse to: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("\nWIFI: Connected");
    Serial.print("Browse to: ");
    Serial.println(WiFi.localIP());
  }

  Serial.println("========================================\n");
}

void processSerialCommand() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();

    if (command == "g") {
      isPaused = false;
      startRotation();
    }
    else if (command == "x") {
      abortAll();
    }
    else if (command.startsWith("r")) {
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
      printSettings();
    }
    else if (command.length() > 0) {
      Serial.println("Unknown. Type 's' for help");
    }
  }
}

// =============================================================================
// WIFI SETUP
// =============================================================================

void setupWiFi() {
  WiFi.mode(WIFI_OFF);
  delay(100);

  // Check if we should connect to existing WiFi or create AP
  if (strlen(WIFI_SSID) > 0) {
    // Connect to existing WiFi
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Connected!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      apMode = false;
      return;
    } else {
      Serial.println("\nFailed to connect. Starting AP mode...");
    }
  }

  // Create Access Point
  Serial.println("Starting Access Point...");
  WiFi.mode(WIFI_AP);
  delay(100);

  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  apMode = true;
  Serial.println("AP Started!");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Password: ");
  Serial.println(AP_PASSWORD);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\nFEET SCULPTURE - WiFi Control");
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

  // Setup WiFi
  setupWiFi();

  // Setup web server
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/cmd", handleCommand);
  server.begin();

  Serial.println("\nWeb server started!");
  printSettings();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

unsigned long lastUpdate = 0;

void loop() {
  // Handle web server
  server.handleClient();

  // Process serial commands
  processSerialCommand();

  // Update state machine every 50ms
  if (millis() - lastUpdate >= 50) {
    lastUpdate = millis();

    unsigned long elapsed = millis() - stateStartTime;

    switch (currentState) {
      case STATE_ROTATING: {
        float cycleDuration = rotationDuration * 1000.0;

        if (elapsed >= cycleDuration) {
          motorSetSpeed(0);
          currentState = STATE_WAITING;
          stateStartTime = millis();
        } else {
          float progress = (float)elapsed / cycleDuration;
          float velocity = easeInOutCubicDerivative(progress);
          velocity = pow(velocity, 1.0 / easingIntensity);

          float sculptureRPM = 60.0 / rotationDuration;
          float motorRPM = sculptureRPM * gearRatio;
          int32_t baseSpeed = (int32_t)(motorRPM * 100.0);

          int32_t currentSpeed = (int32_t)(baseSpeed * velocity);

          if (currentSpeed < minSpeed && currentSpeed > 0) {
            currentSpeed = minSpeed;
          }

          motorSetSpeed(currentSpeed);
        }
        break;
      }

      case STATE_WAITING: {
        motorSetSpeed(0);

        float waitDuration = waitTime * 1000.0;
        if (elapsed >= waitDuration) {
          startRotation();
        }
        break;
      }

      case STATE_STOPPED: {
        motorSetSpeed(0);
        break;
      }
    }
  }

  delay(10);
}
