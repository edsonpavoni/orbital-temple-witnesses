/*
 * ============================================================================
 * ORBITAL TEMPLE - WITNESS SCULPTURE
 * ============================================================================
 *
 * Kinetic sculpture that "searches" for and "tracks" satellites.
 *
 * BEHAVIORS:
 * ----------
 * 1. SEARCHING: Slow continuous rotation (2 RPM) - "looking for satellite"
 * 2. TRACKING:  When satellite passes, stops and follows it (12 min pass)
 *
 * HARDWARE:
 * ---------
 * - Seeed XIAO ESP32-S3
 * - M5Stack Unit RollerCAN (I2C)
 *
 * WIFI:
 * -----
 * - Connects to saved WiFi on boot
 * - If no WiFi, creates AP "Witness-XXX" for configuration
 * - Web interface for calibration and control
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

// Sculpture identity (change for each unit: 001, 002, 003)
#define SCULPTURE_ID "001"
#define SCULPTURE_NAME "Witness-" SCULPTURE_ID

// I2C Settings
#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8    // XIAO D9
#define I2C_SCL_PIN      9    // XIAO D10
#define I2C_FREQ         100000

// Motor speeds (scaled by 100)
#define SPEED_SEARCHING   20000    // 200.00 RPM (scaled for demo, adjust as needed)
#define SPEED_STOP        0
#define SPEED_TRACKING    208      // 2.08 RPM = 0.125°/sec for 90° in 12min

// Timing
#define TRANSITION_TIME_MS     2000   // 2 seconds to transition speeds
#define TRACKING_DURATION_MS   720000 // 12 minutes = 720,000 ms
#define SATELLITE_INTERVAL_MS  5400000 // 90 minutes between passes (for demo: 60000 = 1 min)

// For quicker demo/testing, use these instead:
// #define SATELLITE_INTERVAL_MS  60000  // 1 minute between passes
// #define TRACKING_DURATION_MS   20000  // 20 seconds tracking

// WiFi AP settings
#define AP_PASSWORD "witness123"

// Cloud sync settings
#define CLOUD_URL "https://us-central1-orbital-temple.cloudfunctions.net/witnessReport"
#define CLOUD_SYNC_INTERVAL_MS 10000  // Report status every 10 seconds
#define CLOUD_ENABLED true            // Set to false to disable cloud sync

// Web server
WebServer server(80);
Preferences preferences;

// =============================================================================
// REGISTER ADDRESSES (M5Stack Unit Roller I2C Protocol)
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
// STATE MACHINE
// =============================================================================

enum SculptureState {
  STATE_SEARCHING,
  STATE_TRANSITION_TO_TRACKING,
  STATE_TRACKING,
  STATE_TRANSITION_TO_SEARCHING
};

SculptureState currentState = STATE_SEARCHING;
unsigned long stateStartTime = 0;
unsigned long lastSatellitePass = 0;
unsigned long nextSatellitePass = 0;

int32_t currentSpeed = 0;
int32_t targetSpeed = 0;
int32_t transitionStartSpeed = 0;

bool wifiConnected = false;
bool apMode = false;

// Cloud sync variables
unsigned long lastCloudSync = 0;
String pendingCommandAck = "";
bool motorsEnabled = true;

// =============================================================================
// FUNCTION PROTOTYPES
// =============================================================================

void motorInit();
void motorSetSpeed(int32_t speed);
void motorEnable(bool enable);
int32_t motorGetSpeed();
int32_t motorGetPosition();

void writeReg8(uint8_t reg, uint8_t value);
uint8_t readReg8(uint8_t reg);
void writeReg32(uint8_t reg, int32_t value);
int32_t readReg32(uint8_t reg);

void setupWiFi();
void setupWebServer();
void handleRoot();
void handleStatus();
void handleCalibrate();
void handleTriggerPass();

void changeState(SculptureState newState);
void updateStateMachine();
void updateMotorSpeed();

void cloudSync();
void processCloudCommand(const String& action);
String getStateName();

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  ORBITAL TEMPLE - WITNESS SCULPTURE");
  Serial.println("  " SCULPTURE_NAME);
  Serial.println("========================================");
  Serial.println();

  // Initialize I2C and motor
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  // Check motor connection
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: RollerCAN not found!");
    while (1) delay(1000);
  }
  Serial.println("Motor connected.");

  // Initialize motor
  motorInit();

  // Setup WiFi
  setupWiFi();

  // Setup web server
  setupWebServer();

  // Schedule first satellite pass
  nextSatellitePass = millis() + SATELLITE_INTERVAL_MS;

  // Start in searching mode
  changeState(STATE_SEARCHING);

  Serial.println();
  Serial.println("Sculpture running. Searching for satellites...");
  Serial.println();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  // Handle web server
  server.handleClient();

  // Update state machine
  updateStateMachine();

  // Update motor speed (smooth transitions)
  updateMotorSpeed();

  // Cloud sync (when WiFi connected)
  if (CLOUD_ENABLED && wifiConnected && (millis() - lastCloudSync > CLOUD_SYNC_INTERVAL_MS)) {
    lastCloudSync = millis();
    cloudSync();
  }

  // Status output every 2 seconds
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 2000) {
    lastStatus = millis();

    Serial.print("State: ");
    Serial.print(getStateName());
    Serial.print(" | Speed: ");
    Serial.print(currentSpeed / 100.0f, 1);
    Serial.print(" RPM | Next pass in: ");
    Serial.print((nextSatellitePass - millis()) / 1000);
    Serial.print("s");
    if (wifiConnected) {
      Serial.print(" | Cloud: OK");
    }
    Serial.println();
  }

  delay(50);  // 20Hz update rate
}

// =============================================================================
// STATE MACHINE
// =============================================================================

void changeState(SculptureState newState) {
  Serial.print("State change: ");
  Serial.print(currentState);
  Serial.print(" -> ");
  Serial.println(newState);

  currentState = newState;
  stateStartTime = millis();
  transitionStartSpeed = currentSpeed;

  switch (newState) {
    case STATE_SEARCHING:
      targetSpeed = SPEED_SEARCHING;
      break;

    case STATE_TRANSITION_TO_TRACKING:
      targetSpeed = SPEED_STOP;
      break;

    case STATE_TRACKING:
      targetSpeed = SPEED_TRACKING;
      lastSatellitePass = millis();
      break;

    case STATE_TRANSITION_TO_SEARCHING:
      targetSpeed = SPEED_SEARCHING;
      // Schedule next pass
      nextSatellitePass = millis() + SATELLITE_INTERVAL_MS;
      break;
  }
}

void updateStateMachine() {
  unsigned long elapsed = millis() - stateStartTime;

  switch (currentState) {
    case STATE_SEARCHING:
      // Check if it's time for a satellite pass
      if (millis() >= nextSatellitePass) {
        Serial.println("\n*** SATELLITE DETECTED! ***\n");
        changeState(STATE_TRANSITION_TO_TRACKING);
      }
      break;

    case STATE_TRANSITION_TO_TRACKING:
      // Wait for motor to stop
      if (elapsed >= TRANSITION_TIME_MS) {
        Serial.println("\n*** TRACKING SATELLITE ***\n");
        changeState(STATE_TRACKING);
      }
      break;

    case STATE_TRACKING:
      // Track for the duration of the pass
      if (elapsed >= TRACKING_DURATION_MS) {
        Serial.println("\n*** SATELLITE PASS COMPLETE ***\n");
        changeState(STATE_TRANSITION_TO_SEARCHING);
      }
      break;

    case STATE_TRANSITION_TO_SEARCHING:
      // Wait for motor to reach searching speed
      if (elapsed >= TRANSITION_TIME_MS) {
        changeState(STATE_SEARCHING);
      }
      break;
  }
}

void updateMotorSpeed() {
  // Don't update if motors are disabled
  if (!motorsEnabled) {
    currentSpeed = 0;
    return;
  }

  // Smooth transition between speeds
  unsigned long elapsed = millis() - stateStartTime;

  if (currentState == STATE_TRANSITION_TO_TRACKING ||
      currentState == STATE_TRANSITION_TO_SEARCHING) {
    // Linear interpolation during transitions
    float progress = min(1.0f, (float)elapsed / TRANSITION_TIME_MS);
    currentSpeed = transitionStartSpeed + (targetSpeed - transitionStartSpeed) * progress;
  } else {
    currentSpeed = targetSpeed;
  }

  motorSetSpeed(currentSpeed);
}

// =============================================================================
// MOTOR FUNCTIONS
// =============================================================================

void motorInit() {
  motorEnable(false);
  delay(50);

  writeReg8(REG_STALL_PROT, 0);
  delay(50);

  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);

  writeReg32(REG_SPEED_MAXCUR, 100000);  // 1000mA max
  delay(50);

  motorSetSpeed(0);
  delay(50);

  motorEnable(true);
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
// I2C FUNCTIONS
// =============================================================================

void writeReg8(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t readReg8(uint8_t reg) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)ROLLER_I2C_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
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
// WIFI SETUP
// =============================================================================

void setupWiFi() {
  // Disconnect any previous connection
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

      // Start mDNS responder
      String hostname = "witness-" SCULPTURE_ID;
      if (MDNS.begin(hostname.c_str())) {
        Serial.print("mDNS started: http://");
        Serial.print(hostname);
        Serial.println(".local");
        MDNS.addService("http", "tcp", 80);
      }

      // Sync time via NTP
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      Serial.println("NTP time sync requested.");
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
  Serial.println();
  Serial.println("Starting AP mode...");

  WiFi.mode(WIFI_AP);
  delay(100);

  // Configure AP with specific settings for better compatibility
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.softAPConfig(local_IP, gateway, subnet);
  delay(100);

  bool apStarted = WiFi.softAP(SCULPTURE_NAME, AP_PASSWORD, 1, 0, 4);
  // Parameters: ssid, password, channel, hidden(0=visible), max_connections

  if (apStarted) {
    apMode = true;
    Serial.println("AP started successfully!");
    Serial.print("SSID: ");
    Serial.println(SCULPTURE_NAME);
    Serial.print("Password: ");
    Serial.println(AP_PASSWORD);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("ERROR: Failed to start AP mode!");
  }
}

// =============================================================================
// WEB SERVER
// =============================================================================

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/calibrate", handleCalibrate);
  server.on("/trigger", handleTriggerPass);
  server.on("/stop", []() {
    motorsEnabled = false;
    motorEnable(false);
    Serial.println("Local: Motors stopped!");
    server.send(200, "text/plain", "OK");
  });
  server.on("/start", []() {
    motorsEnabled = true;
    motorEnable(true);
    Serial.println("Local: Motors started!");
    server.send(200, "text/plain", "OK");
  });
  server.on("/wifi", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    preferences.begin("witness", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();

    server.send(200, "text/html",
      "<html><body><h1>WiFi saved!</h1>"
      "<p>Restarting...</p></body></html>");

    delay(1000);
    ESP.restart();
  });

  server.begin();
  Serial.println("Web server started.");
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>" SCULPTURE_NAME "</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:20px;background:#1a1a2e;color:#eee}";
  html += "h1{color:#e94560}";
  html += ".card{background:#16213e;padding:20px;border-radius:10px;margin:10px 0}";
  html += ".searching{color:#f39c12}.tracking{color:#2ecc71}";
  html += "button{background:#e94560;color:white;border:none;padding:15px 30px;";
  html += "font-size:16px;border-radius:5px;cursor:pointer;margin:5px}";
  html += "button:hover{background:#c73e54}";
  html += "input{padding:10px;font-size:16px;margin:5px;width:200px}";
  html += "#status{font-size:18px}";
  html += "</style></head><body>";
  html += "<h1>" SCULPTURE_NAME "</h1>";

  html += "<div class='card'><h2>Status</h2>";
  html += "<div id='status'>Loading...</div></div>";

  html += "<div class='card'><h2>Controls</h2>";
  html += "<button onclick=\"triggerPass()\">TRIGGER SATELLITE PASS</button>";
  html += "<button onclick=\"calibrate()\">CALIBRATE</button>";
  html += "<button onclick=\"stopMotor()\" style='background:#e74c3c'>STOP</button>";
  html += "<button onclick=\"startMotor()\" style='background:#2ecc71'>START</button>";
  html += "</div>";

  if (apMode) {
    html += "<div class='card'><h2>WiFi Setup</h2>";
    html += "<form action='/wifi' method='post'>";
    html += "<input type='text' name='ssid' placeholder='WiFi Name'><br>";
    html += "<input type='password' name='password' placeholder='Password'><br>";
    html += "<button type='submit'>Connect</button>";
    html += "</form></div>";
  }

  html += "<script>";
  html += "function updateStatus(){";
  html += "fetch('/status').then(r=>r.json()).then(data=>{";
  html += "var cls=data.state.includes('TRACK')?'tracking':'searching';";
  html += "document.getElementById('status').innerHTML=";
  html += "'<p>State: <span class=\"'+cls+'\">'+data.state+'</span></p>'+";
  html += "'<p>Speed: '+data.speed.toFixed(1)+' RPM</p>'+";
  html += "'<p>Next pass: '+data.nextPass+'s</p>'+";
  html += "'<p>WiFi: '+(data.wifi?'Connected':'AP Mode')+'</p>';";
  html += "});}";
  html += "function triggerPass(){fetch('/trigger').then(()=>updateStatus());}";
  html += "function calibrate(){fetch('/calibrate').then(()=>updateStatus());}";
  html += "function stopMotor(){fetch('/stop').then(()=>updateStatus());}";
  html += "function startMotor(){fetch('/start').then(()=>updateStatus());}";
  html += "updateStatus();setInterval(updateStatus,1000);";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void handleStatus() {
  String state;
  switch (currentState) {
    case STATE_SEARCHING: state = "SEARCHING"; break;
    case STATE_TRANSITION_TO_TRACKING: state = "STOPPING"; break;
    case STATE_TRACKING: state = "TRACKING"; break;
    case STATE_TRANSITION_TO_SEARCHING: state = "RESUMING"; break;
  }

  String json = "{";
  json += "\"state\":\"" + state + "\",";
  json += "\"speed\":" + String(currentSpeed / 100.0f) + ",";
  json += "\"nextPass\":" + String((nextSatellitePass - millis()) / 1000) + ",";
  json += "\"wifi\":" + String(wifiConnected ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

void handleCalibrate() {
  // Reset position and sync time
  Serial.println("Calibration requested.");
  server.send(200, "text/plain", "OK");
}

void handleTriggerPass() {
  // Manually trigger a satellite pass
  if (currentState == STATE_SEARCHING) {
    nextSatellitePass = millis();  // Trigger now
    Serial.println("Manual satellite pass triggered!");
  }
  server.send(200, "text/plain", "OK");
}

// =============================================================================
// CLOUD SYNC
// =============================================================================

String getStateName() {
  switch (currentState) {
    case STATE_SEARCHING: return "SEARCHING";
    case STATE_TRANSITION_TO_TRACKING: return "STOPPING";
    case STATE_TRACKING: return "TRACKING";
    case STATE_TRANSITION_TO_SEARCHING: return "RESUMING";
    default: return "UNKNOWN";
  }
}

void cloudSync() {
  if (!wifiConnected) return;

  HTTPClient http;
  http.begin(CLOUD_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);  // 5 second timeout

  // Build JSON payload
  StaticJsonDocument<256> doc;
  doc["sculptureId"] = "witness-" SCULPTURE_ID;
  doc["state"] = getStateName();
  doc["speed"] = currentSpeed / 100.0f;
  doc["position"] = motorGetPosition();

  // Include command acknowledgment if pending
  if (pendingCommandAck.length() > 0) {
    doc["commandAck"] = pendingCommandAck;
    pendingCommandAck = "";
  }

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);

  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();

    // Parse response for commands
    StaticJsonDocument<256> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (!error) {
      // Check for pending command
      if (!responseDoc["command"].isNull()) {
        String action = responseDoc["command"]["action"].as<String>();
        if (action.length() > 0) {
          Serial.print("Cloud command received: ");
          Serial.println(action);
          processCloudCommand(action);
          pendingCommandAck = action;  // Will be sent on next sync
        }
      }
    }
  } else {
    Serial.print("Cloud sync failed: ");
    Serial.println(httpCode);
  }

  http.end();
}

void processCloudCommand(const String& action) {
  if (action == "trigger") {
    // Trigger satellite pass
    if (currentState == STATE_SEARCHING) {
      nextSatellitePass = millis();
      Serial.println("Cloud: Satellite pass triggered!");
    }
  }
  else if (action == "stop") {
    // Stop the motor
    motorsEnabled = false;
    motorEnable(false);
    Serial.println("Cloud: Motors stopped!");
  }
  else if (action == "start") {
    // Start the motor
    motorsEnabled = true;
    motorEnable(true);
    Serial.println("Cloud: Motors started!");
  }
  else if (action == "calibrate") {
    // Reset to searching state
    changeState(STATE_SEARCHING);
    nextSatellitePass = millis() + SATELLITE_INTERVAL_MS;
    Serial.println("Cloud: Calibration triggered!");
  }
}
