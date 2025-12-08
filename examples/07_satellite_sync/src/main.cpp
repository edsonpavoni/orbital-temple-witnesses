/*
 * ============================================================================
 * ORBITAL TEMPLE - SATELLITE SYNCHRONIZED SCULPTURE
 * Calibrated unidirectional tracking system
 * ============================================================================
 *
 * CALIBRATION WORKFLOW:
 * ---------------------
 * 1. Manually position sculpture to point UP (12:00 position)
 * 2. Access web panel at http://[ESP32_IP]
 * 3. Click "SET CALIBRATION" button
 * 4. Sculpture will now track satellite position relative to Miami
 *
 * POSITION MAPPING:
 * -----------------
 * - Satellite above Miami (0°) → Sculpture points UP (12:00)
 * - Satellite to right (90°) → Sculpture points RIGHT (3:00)
 * - Satellite below Miami (180°) → Sculpture points DOWN (6:00)
 * - Satellite to left (270°) → Sculpture points LEFT (9:00)
 *
 * UNIDIRECTIONAL ROTATION:
 * ------------------------
 * Motor only rotates forward (clockwise when viewed from above)
 * Always takes the shortest path forward to reach target
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Firebase_ESP_Client.h>
#include <Preferences.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// =============================================================================
// WIFI & FIREBASE CONFIGURATION
// =============================================================================

#define WIFI_SSID "iPhone Pavoni"
#define WIFI_PASSWORD "12345678"

#define FIREBASE_HOST "orbital-temple-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "TFWzZASAG79bobA3pGUpqgrsD97ItaE00hbckqnh"

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
#define REG_SPEED_READ   0x60
#define REG_POS_READ     0x90

#define MODE_SPEED       1

// =============================================================================
// MOTOR CONFIGURATION
// =============================================================================

const float GEAR_RATIO = 1.0;           // Motor:sculpture ratio
const int32_t MAX_CURRENT = 100000;     // 1000mA max current
const int32_t BASE_SPEED = 100;         // Base rotation speed
const int32_t MIN_SPEED = 10;           // Minimum speed threshold
const float POSITION_TOLERANCE = 2.0;   // Degrees tolerance for "reached"

// =============================================================================
// CALIBRATION & STATE
// =============================================================================

Preferences preferences;
bool isCalibrated = false;
float calibrationOffset = 0.0;          // Motor position when sculpture points up
float currentSculptureAngle = 0.0;      // Current sculpture angle (0-360)
float targetSatelliteAngle = 0.0;       // Target angle from Firebase
unsigned long lastFirebaseUpdate = 0;
const unsigned long FIREBASE_UPDATE_INTERVAL = 500;  // Update every 500ms

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool firebaseReady = false;

// Web server
WebServer server(80);

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
// ANGLE CALCULATIONS
// =============================================================================

// Normalize angle to 0-360 range
float normalizeAngle(float angle) {
  while (angle < 0) angle += 360.0;
  while (angle >= 360.0) angle -= 360.0;
  return angle;
}

// Convert motor encoder position to degrees
float motorPositionToAngle(int32_t position) {
  // Assuming motor encoder gives pulses per revolution
  // Adjust based on your motor's actual encoder resolution
  const float PULSES_PER_REV = 1000.0;  // Adjust this value
  float rotations = position / PULSES_PER_REV;
  float angle = rotations * 360.0 / GEAR_RATIO;
  return normalizeAngle(angle);
}

// Get current sculpture angle
float getCurrentSculptureAngle() {
  if (!isCalibrated) return 0.0;

  int32_t motorPos = motorGetPosition();
  float motorAngle = motorPositionToAngle(motorPos);
  float sculptureAngle = normalizeAngle(motorAngle - calibrationOffset);

  return sculptureAngle;
}

// Calculate forward distance to target (always positive, unidirectional)
float getForwardDistance(float current, float target) {
  float distance = normalizeAngle(target - current);
  return distance;
}

// =============================================================================
// CALIBRATION
// =============================================================================

void saveCalibration() {
  preferences.begin("sculpture", false);
  preferences.putBool("calibrated", true);
  preferences.putFloat("offset", calibrationOffset);
  preferences.end();
  Serial.println("✓ Calibration saved to flash");
}

void loadCalibration() {
  preferences.begin("sculpture", true);
  isCalibrated = preferences.getBool("calibrated", false);
  calibrationOffset = preferences.getFloat("offset", 0.0);
  preferences.end();

  if (isCalibrated) {
    Serial.println("✓ Calibration loaded from flash");
    Serial.printf("  Offset: %.2f°\n", calibrationOffset);
  } else {
    Serial.println("⚠ No calibration found - use web panel to calibrate");
  }
}

void setCalibration() {
  // Store current motor position as the "up" position (0°)
  int32_t motorPos = motorGetPosition();
  calibrationOffset = motorPositionToAngle(motorPos);
  isCalibrated = true;
  saveCalibration();

  // Enable motor now that calibration is set
  motorEnable();

  Serial.println("✓ CALIBRATION SET");
  Serial.printf("  Motor position: %d\n", motorPos);
  Serial.printf("  Calibration offset: %.2f°\n", calibrationOffset);
  Serial.println("  Sculpture is now at 0° (pointing UP)");
  Serial.println("  Motor ENABLED - tracking started");
}

void clearCalibration() {
  isCalibrated = false;
  calibrationOffset = 0.0;
  preferences.begin("sculpture", false);
  preferences.clear();
  preferences.end();

  // Disable motor when calibration is cleared
  motorSetSpeed(0);
  motorDisable();

  Serial.println("✓ Calibration cleared");
  Serial.println("  Motor DISABLED - ready for manual positioning");
}

// =============================================================================
// MOVEMENT CONTROL
// =============================================================================

void moveToAngle(float targetAngle) {
  if (!isCalibrated) {
    Serial.println("⚠ Cannot move - not calibrated");
    return;
  }

  float current = getCurrentSculptureAngle();
  float distance = getForwardDistance(current, targetAngle);

  // If we're close enough, stop
  if (distance < POSITION_TOLERANCE) {
    motorSetSpeed(0);
    currentSculptureAngle = current;
    return;
  }

  // Calculate speed based on distance (slow down when close)
  int32_t speed;
  if (distance < 10.0) {
    // Slow approach
    speed = MIN_SPEED + (int32_t)((distance / 10.0) * (BASE_SPEED - MIN_SPEED));
  } else if (distance < 30.0) {
    // Medium speed
    speed = BASE_SPEED;
  } else {
    // Full speed for large distances
    speed = BASE_SPEED * 2;
  }

  // Always move forward (positive speed)
  motorSetSpeed(speed);
  currentSculptureAngle = current;
}

// =============================================================================
// FIREBASE
// =============================================================================

void initFirebase() {
  Serial.println("Initializing Firebase...");

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  firebaseReady = true;
  Serial.println("✓ Firebase initialized");

  // Register ESP32 IP address in Firebase
  String ipAddress = WiFi.localIP().toString();
  if (Firebase.RTDB.setString(&fbdo, "/sculpture/esp32/ip", ipAddress)) {
    Serial.println("✓ IP address registered in Firebase: " + ipAddress);
  } else {
    Serial.println("⚠ Failed to register IP: " + fbdo.errorReason());
  }

  // Also send device info
  Firebase.RTDB.setString(&fbdo, "/sculpture/esp32/lastSeen", String(millis()));
  Firebase.RTDB.setString(&fbdo, "/sculpture/esp32/version", "v1.0");
}

void updateFromFirebase() {
  if (!firebaseReady) return;

  unsigned long now = millis();
  if (now - lastFirebaseUpdate < FIREBASE_UPDATE_INTERVAL) return;
  lastFirebaseUpdate = now;

  // Read satellite angle from Firebase
  if (Firebase.RTDB.getFloat(&fbdo, "/sculpture/angle/angle")) {
    float newAngle = fbdo.floatData();

    // Only update if angle changed significantly
    if (abs(newAngle - targetSatelliteAngle) > 0.5) {
      targetSatelliteAngle = normalizeAngle(newAngle);
      Serial.printf("📡 New target: %.1f°\n", targetSatelliteAngle);
    }
  } else {
    // Only print error occasionally
    static unsigned long lastErrorPrint = 0;
    if (now - lastErrorPrint > 5000) {
      Serial.println("⚠ Firebase read error: " + fbdo.errorReason());
      lastErrorPrint = now;
    }
  }

  // Check for remote commands
  if (Firebase.RTDB.getString(&fbdo, "/sculpture/commands/action")) {
    String command = fbdo.stringData();

    if (command == "calibrate") {
      Serial.println("📡 Remote command: CALIBRATE");
      setCalibration();
      Firebase.RTDB.setString(&fbdo, "/sculpture/commands/action", "");
      Firebase.RTDB.setString(&fbdo, "/sculpture/commands/lastExecuted", "calibrate");
    } else if (command == "clear") {
      Serial.println("📡 Remote command: CLEAR CALIBRATION");
      clearCalibration();
      Firebase.RTDB.setString(&fbdo, "/sculpture/commands/action", "");
      Firebase.RTDB.setString(&fbdo, "/sculpture/commands/lastExecuted", "clear");
    } else if (command == "stop") {
      Serial.println("📡 Remote command: STOP");
      motorSetSpeed(0);
      Firebase.RTDB.setString(&fbdo, "/sculpture/commands/action", "");
      Firebase.RTDB.setString(&fbdo, "/sculpture/commands/lastExecuted", "stop");
    }
  }
}

void publishStatusToFirebase() {
  static unsigned long lastPublish = 0;
  unsigned long now = millis();

  // Publish status every 2 seconds
  if (now - lastPublish < 2000) return;
  lastPublish = now;

  if (!firebaseReady) return;

  float current = isCalibrated ? getCurrentSculptureAngle() : 0.0;
  int32_t motorPos = motorGetPosition();
  float distance = isCalibrated ? getForwardDistance(current, targetSatelliteAngle) : 0.0;

  // Write status to Firebase
  Firebase.RTDB.setBool(&fbdo, "/sculpture/status/calibrated", isCalibrated);
  Firebase.RTDB.setFloat(&fbdo, "/sculpture/status/sculptureAngle", current);
  Firebase.RTDB.setFloat(&fbdo, "/sculpture/status/satelliteAngle", targetSatelliteAngle);
  Firebase.RTDB.setFloat(&fbdo, "/sculpture/status/distance", distance);
  Firebase.RTDB.setInt(&fbdo, "/sculpture/status/motorPosition", motorPos);
  Firebase.RTDB.setFloat(&fbdo, "/sculpture/status/gearRatio", GEAR_RATIO);

  // Use Firebase server timestamp for lastUpdate (UNIX timestamp in milliseconds)
  Firebase.RTDB.setTimestamp(&fbdo, "/sculpture/status/lastUpdate");
}

// =============================================================================
// WEB SERVER
// =============================================================================

// Enable CORS for all requests
void enableCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleRoot() {
  enableCORS();
  float current = isCalibrated ? getCurrentSculptureAngle() : 0.0;
  int32_t motorPos = motorGetPosition();

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Orbital Temple - Sculpture Control</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; max-width: 600px; margin: 50px auto; padding: 20px; background: #000; color: #fff; }";
  html += "h1 { color: #fff; text-align: center; }";
  html += ".status { background: #222; padding: 20px; border-radius: 10px; margin: 20px 0; }";
  html += ".status-item { margin: 10px 0; padding: 10px; background: #333; border-radius: 5px; }";
  html += ".label { color: #999; font-size: 14px; }";
  html += ".value { color: #fff; font-size: 24px; font-weight: bold; }";
  html += "button { background: #ff8800; color: #fff; border: none; padding: 15px 30px; ";
  html += "font-size: 16px; border-radius: 5px; cursor: pointer; margin: 10px 5px; width: calc(50% - 15px); }";
  html += "button:hover { background: #ff9900; }";
  html += "button:active { background: #ee7700; }";
  html += ".calibrated { color: #00ff00; }";
  html += ".not-calibrated { color: #ff0000; }";
  html += "</style>";
  html += "<script>";
  html += "function setCalibration() { fetch('/calibrate').then(() => location.reload()); }";
  html += "function clearCalibration() { fetch('/clear').then(() => location.reload()); }";
  html += "function stopMotor() { fetch('/stop').then(() => location.reload()); }";
  html += "setInterval(() => location.reload(), 2000);";  // Auto-refresh every 2s
  html += "</script>";
  html += "</head><body>";

  html += "<h1>🛰️ Orbital Temple</h1>";
  html += "<h2 style='text-align: center; color: #999;'>Sculpture Control</h2>";

  html += "<div class='status'>";
  html += "<div class='status-item'>";
  html += "<div class='label'>Calibration Status</div>";
  if (isCalibrated) {
    html += "<div class='value calibrated'>✓ CALIBRATED</div>";
  } else {
    html += "<div class='value not-calibrated'>⚠ NOT CALIBRATED</div>";
  }
  html += "</div>";

  html += "<div class='status-item'>";
  html += "<div class='label'>Satellite Angle (Target)</div>";
  html += "<div class='value'>" + String(targetSatelliteAngle, 1) + "°</div>";
  html += "</div>";

  if (isCalibrated) {
    html += "<div class='status-item'>";
    html += "<div class='label'>Sculpture Angle (Current)</div>";
    html += "<div class='value'>" + String(current, 1) + "°</div>";
    html += "</div>";

    float distance = getForwardDistance(current, targetSatelliteAngle);
    html += "<div class='status-item'>";
    html += "<div class='label'>Distance to Target</div>";
    html += "<div class='value'>" + String(distance, 1) + "°</div>";
    html += "</div>";
  }

  html += "<div class='status-item'>";
  html += "<div class='label'>Motor Position (Raw)</div>";
  html += "<div class='value'>" + String(motorPos) + "</div>";
  html += "</div>";
  html += "</div>";

  html += "<div style='text-align: center;'>";
  if (!isCalibrated) {
    html += "<p style='color: #ff8800;'>⚠ Position sculpture pointing UP (12:00), then click:</p>";
    html += "<button onclick='setCalibration()' style='width: 80%;'>SET CALIBRATION</button>";
  } else {
    html += "<button onclick='stopMotor()'>STOP MOTOR</button>";
    html += "<button onclick='clearCalibration()'>CLEAR CALIBRATION</button>";
  }
  html += "</div>";

  html += "<div style='text-align: center; margin-top: 30px; color: #666; font-size: 12px;'>";
  html += "IP: " + WiFi.localIP().toString() + "<br>";
  html += "Firmware: v1.0 - Unidirectional Tracking";
  html += "</div>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleCalibrate() {
  enableCORS();
  setCalibration();
  server.send(200, "text/plain", "OK");
}

void handleClear() {
  enableCORS();
  clearCalibration();
  server.send(200, "text/plain", "OK");
}

void handleStop() {
  enableCORS();
  motorSetSpeed(0);
  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  enableCORS();

  float current = isCalibrated ? getCurrentSculptureAngle() : 0.0;
  int32_t motorPos = motorGetPosition();
  float distance = isCalibrated ? getForwardDistance(current, targetSatelliteAngle) : 0.0;

  String json = "{";
  json += "\"calibrated\":" + String(isCalibrated ? "true" : "false") + ",";
  json += "\"satelliteAngle\":" + String(targetSatelliteAngle, 1) + ",";
  json += "\"sculptureAngle\":" + String(current, 1) + ",";
  json += "\"distance\":" + String(distance, 1) + ",";
  json += "\"motorPosition\":" + String(motorPos);
  json += "}";

  server.send(200, "application/json", json);
}

void handleOptions() {
  enableCORS();
  server.send(200, "text/plain", "OK");
}

void initWebServer() {
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/calibrate", handleCalibrate);
  server.on("/clear", handleClear);
  server.on("/stop", handleStop);
  server.onNotFound(handleOptions);  // Handle OPTIONS requests for CORS
  server.begin();

  Serial.println("✓ Web server started");
  Serial.printf("  Control panel: http://%s\n", WiFi.localIP().toString().c_str());
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n");
  Serial.println("============================================");
  Serial.println("  ORBITAL TEMPLE - SCULPTURE CONTROLLER");
  Serial.println("  Satellite Synchronized Tracking System");
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

  // Load calibration from flash
  loadCalibration();

  // Set motor state based on calibration
  if (isCalibrated) {
    motorEnable();
    Serial.println("  Motor ENABLED (calibration loaded)");
  } else {
    motorDisable();
    Serial.println("  Motor DISABLED (awaiting calibration)");
  }

  // Connect to WiFi
  Serial.printf("Connecting to WiFi '%s'... ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" ✓");
    Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());

    // Initialize Firebase
    initFirebase();

    // Initialize web server
    initWebServer();
  } else {
    Serial.println(" ✗ FAILED");
    Serial.println("  Continuing without WiFi");
  }

  Serial.println("\n✓ System ready!");
  Serial.println("============================================\n");

  if (!isCalibrated) {
    Serial.println("⚠ CALIBRATION REQUIRED:");
    Serial.println("  1. Manually position sculpture pointing UP (12:00)");
    Serial.println("  2. Access web panel and click 'SET CALIBRATION'");
    Serial.println();
  }
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  server.handleClient();

  if (firebaseReady) {
    updateFromFirebase();
    publishStatusToFirebase();

    if (isCalibrated) {
      moveToAngle(targetSatelliteAngle);
    }
  }

  delay(10);
}
