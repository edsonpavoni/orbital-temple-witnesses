/*
 * ============================================================================
 * ORBITAL TEMPLE - OTA UPDATE EXAMPLE
 * ============================================================================
 *
 * Demonstrates wireless code updates (OTA) for deployed sculptures.
 *
 * FEATURES:
 * ---------
 * - WiFi connection with credential storage
 * - ArduinoOTA for wireless uploads
 * - mDNS discovery (witness-ota.local)
 * - Web interface for WiFi setup
 * - Motor control example (RollerCAN)
 *
 * FIRST TIME SETUP:
 * -----------------
 * 1. Upload this code via USB
 * 2. Connect to AP "Witness-OTA" (password: witness123)
 * 3. Go to http://192.168.4.1 and enter your WiFi credentials
 * 4. Device will restart and connect to your WiFi
 * 5. Note the IP address shown in serial monitor
 *
 * WIRELESS UPDATES (after WiFi is configured):
 * --------------------------------------------
 * Option 1: Using IP address
 *   - Edit platformio.ini, uncomment and set:
 *     upload_protocol = espota
 *     upload_port = 192.168.1.XXX (your device IP)
 *   - Upload code normally (PlatformIO will use OTA)
 *
 * Option 2: Using mDNS name (easier!)
 *   - Edit platformio.ini, uncomment and set:
 *     upload_protocol = espota
 *     upload_port = witness-ota.local
 *   - Upload code normally
 *
 * Option 3: Arduino IDE
 *   - Go to Tools > Port
 *   - Select "witness-ota at 192.168.1.XXX"
 *   - Upload normally
 *
 * SECURITY NOTE:
 * --------------
 * This example uses a password for OTA updates. For production:
 * - Change OTA_PASSWORD to something secure
 * - Consider adding HTTPS for web interface
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

// OTA Settings
#define OTA_HOSTNAME "witness-ota"
#define OTA_PASSWORD "orbital2025"  // Change this for production!

// WiFi AP settings
#define AP_SSID "Witness-OTA"
#define AP_PASSWORD "witness123"

// I2C Settings (for RollerCAN motor)
#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8    // XIAO D9
#define I2C_SCL_PIN      9    // XIAO D10
#define I2C_FREQ         100000

// Motor registers
#define REG_OUTPUT       0x00
#define REG_MODE         0x01
#define REG_STALL_PROT   0x0F
#define REG_SPEED        0x40
#define REG_SPEED_MAXCUR 0x50
#define REG_SPEED_READ   0x60
#define MODE_SPEED       1

// Demo motor speed
#define DEMO_SPEED_RPM   50    // 50 RPM demo speed

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

WebServer server(80);
Preferences preferences;
bool wifiConnected = false;
bool apMode = false;
bool motorConnected = false;
unsigned long lastBlink = 0;
bool ledState = false;

// =============================================================================
// FUNCTION PROTOTYPES
// =============================================================================

void setupWiFi();
void setupOTA();
void setupWebServer();
void setupMotor();
void handleRoot();
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
  Serial.println("  ORBITAL TEMPLE - OTA UPDATE DEMO");
  Serial.println("========================================");
  Serial.println();

  // Setup WiFi
  setupWiFi();

  // Setup OTA updates
  setupOTA();

  // Setup web server
  setupWebServer();

  // Setup motor (optional - will work even if not connected)
  setupMotor();

  Serial.println();
  Serial.println("========================================");
  Serial.println("  READY FOR OTA UPDATES!");
  Serial.println("========================================");
  if (wifiConnected) {
    Serial.println();
    Serial.println("To upload wirelessly:");
    Serial.println("1. Edit platformio.ini");
    Serial.println("2. Uncomment OTA settings:");
    Serial.println("   upload_protocol = espota");
    Serial.println("   upload_port = " OTA_HOSTNAME ".local");
    Serial.println("3. Upload code normally!");
    Serial.println();
    Serial.println("OTA Password: " OTA_PASSWORD);
  }
  Serial.println("========================================");
  Serial.println();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();

  // Handle web server
  server.handleClient();

  // Blink LED to show we're alive
  if (millis() - lastBlink > 1000) {
    lastBlink = millis();
    ledState = !ledState;
    Serial.print(".");
    if (millis() % 30000 < 1000) {
      Serial.println();
      Serial.print("Status: ");
      Serial.print(wifiConnected ? "WiFi Connected" : "AP Mode");
      if (motorConnected) {
        int32_t speed = readReg32(REG_SPEED_READ);
        Serial.print(" | Motor: ");
        Serial.print(speed / 100.0f, 1);
        Serial.print(" RPM");
      }
      Serial.println();
    }
  }

  delay(10);
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
    WiFi.setHostname(OTA_HOSTNAME);
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
      if (MDNS.begin(OTA_HOSTNAME)) {
        Serial.print("mDNS started: http://");
        Serial.print(OTA_HOSTNAME);
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
  Serial.println();
  Serial.println("Starting AP mode...");

  WiFi.mode(WIFI_AP);
  delay(100);

  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.softAPConfig(local_IP, gateway, subnet);
  delay(100);

  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (apStarted) {
    apMode = true;
    Serial.println("AP started!");
    Serial.print("SSID: ");
    Serial.println(AP_SSID);
    Serial.print("Password: ");
    Serial.println(AP_PASSWORD);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("ERROR: Failed to start AP!");
  }
}

// =============================================================================
// OTA SETUP
// =============================================================================

void setupOTA() {
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname
  ArduinoOTA.setHostname(OTA_HOSTNAME);

  // Password for OTA updates
  ArduinoOTA.setPassword(OTA_PASSWORD);

  // Password can be set with it's md5 value as well
  // MD5(orbital2025) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_SPIFFS
      type = "filesystem";
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("  OTA UPDATE STARTED");
    Serial.println("  Type: " + type);
    Serial.println("========================================");

    // Stop motor during update
    if (motorConnected) {
      writeReg8(REG_OUTPUT, 0);
    }
  });

  ArduinoOTA.onEnd([]() {
    Serial.println();
    Serial.println("========================================");
    Serial.println("  OTA UPDATE COMPLETE!");
    Serial.println("========================================");
    Serial.println();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 500) {
      lastPrint = millis();
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.println();
    Serial.println("========================================");
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
    Serial.println("========================================");
  });

  ArduinoOTA.begin();
  Serial.println("OTA ready!");
}

// =============================================================================
// WEB SERVER
// =============================================================================

void setupWebServer() {
  server.on("/", handleRoot);

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
        "<p>Restarting device...</p>"
        "<p>Connect to your WiFi and check serial monitor for IP address.</p>"
        "</body></html>");

      delay(1000);
      ESP.restart();
    } else {
      server.send(400, "text/html",
        "<html><body style='font-family:Arial;background:#1a1a2e;color:#eee'>"
        "<h1 style='color:#e94560'>Error</h1>"
        "<p>SSID cannot be empty!</p>"
        "<a href='/'>Go back</a>"
        "</body></html>");
    }
  });

  server.on("/status", []() {
    String json = "{";
    json += "\"wifi\":" + String(wifiConnected ? "true" : "false") + ",";
    json += "\"ip\":\"" + (wifiConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\",";
    json += "\"hostname\":\"" OTA_HOSTNAME ".local\",";
    json += "\"motor\":" + String(motorConnected ? "true" : "false");
    if (motorConnected) {
      int32_t speed = readReg32(REG_SPEED_READ);
      json += ",\"speed\":" + String(speed / 100.0f);
    }
    json += "}";
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("Web server started.");
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>Orbital Temple - OTA Demo</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:20px;background:#1a1a2e;color:#eee}";
  html += "h1{color:#e94560}h2{color:#f39c12}";
  html += ".card{background:#16213e;padding:20px;border-radius:10px;margin:10px 0}";
  html += ".status{font-size:18px;line-height:1.8}";
  html += ".success{color:#2ecc71}.warning{color:#f39c12}";
  html += "input{padding:10px;font-size:16px;margin:5px;width:200px;border-radius:5px;border:none}";
  html += "button{background:#e94560;color:white;border:none;padding:15px 30px;";
  html += "font-size:16px;border-radius:5px;cursor:pointer;margin:5px}";
  html += "button:hover{background:#c73e54}";
  html += "code{background:#0f3460;padding:2px 6px;border-radius:3px}";
  html += "</style></head><body>";
  html += "<h1>🛰️ Orbital Temple</h1>";
  html += "<h2>OTA Update Demo</h2>";

  html += "<div class='card'><h2>Status</h2>";
  html += "<div class='status' id='status'>Loading...</div></div>";

  if (apMode) {
    html += "<div class='card'><h2>⚠️ WiFi Setup Required</h2>";
    html += "<p>Configure WiFi to enable OTA updates:</p>";
    html += "<form action='/wifi' method='post'>";
    html += "<input type='text' name='ssid' placeholder='WiFi Name' required><br>";
    html += "<input type='password' name='password' placeholder='Password'><br>";
    html += "<button type='submit'>💾 Save & Restart</button>";
    html += "</form></div>";
  } else {
    html += "<div class='card'><h2>✅ OTA Updates Enabled</h2>";
    html += "<p><strong>Method 1:</strong> PlatformIO (Recommended)</p>";
    html += "<ol>";
    html += "<li>Edit <code>platformio.ini</code></li>";
    html += "<li>Uncomment these lines:<br>";
    html += "<code>upload_protocol = espota</code><br>";
    html += "<code>upload_port = " OTA_HOSTNAME ".local</code></li>";
    html += "<li>Upload code normally!</li>";
    html += "</ol>";
    html += "<p><strong>OTA Password:</strong> <code>" OTA_PASSWORD "</code></p>";
    html += "<p><strong>Method 2:</strong> Arduino IDE</p>";
    html += "<ol>";
    html += "<li>Tools → Port → Select <code>" OTA_HOSTNAME "</code></li>";
    html += "<li>Upload normally</li>";
    html += "</ol>";
    html += "</div>";
  }

  html += "<script>";
  html += "function updateStatus(){";
  html += "fetch('/status').then(r=>r.json()).then(data=>{";
  html += "var status='';";
  html += "status+='<p><strong>Connection:</strong> ';";
  html += "status+=data.wifi?'<span class=\"success\">WiFi Connected</span>':'<span class=\"warning\">AP Mode</span>';";
  html += "status+='</p>';";
  html += "status+='<p><strong>IP Address:</strong> '+data.ip+'</p>';";
  html += "status+='<p><strong>mDNS:</strong> <code>'+data.hostname+'</code></p>';";
  html += "status+='<p><strong>Motor:</strong> ';";
  html += "status+=data.motor?'<span class=\"success\">Connected</span>':'Not Connected';";
  html += "if(data.motor){status+=' ('+data.speed.toFixed(1)+' RPM)';}";
  html += "status+='</p>';";
  html += "document.getElementById('status').innerHTML=status;";
  html += "});}";
  html += "updateStatus();setInterval(updateStatus,2000);";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

// =============================================================================
// MOTOR SETUP
// =============================================================================

void setupMotor() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  // Check motor connection
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("Motor not connected (optional - continuing anyway)");
    motorConnected = false;
    return;
  }

  motorConnected = true;
  Serial.println("Motor connected!");

  // Initialize motor
  writeReg8(REG_OUTPUT, 0);
  delay(50);
  writeReg8(REG_STALL_PROT, 0);
  delay(50);
  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);
  writeReg32(REG_SPEED_MAXCUR, 50000);  // 500mA
  delay(50);
  writeReg32(REG_SPEED, DEMO_SPEED_RPM * 100);  // Demo speed
  delay(50);
  writeReg8(REG_OUTPUT, 1);

  Serial.print("Motor running at ");
  Serial.print(DEMO_SPEED_RPM);
  Serial.println(" RPM");
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
