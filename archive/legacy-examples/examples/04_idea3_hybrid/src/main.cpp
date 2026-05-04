/*
 * ============================================================================
 * IDEA 3 HYBRID - BREATH + RITUAL
 * ============================================================================
 *
 * Choreography for Witness video capture (Apr 16, 2026 ship target).
 *
 * BEHAVIOR:
 *   BREATH (51s) — gentle ±15° sine sweep around current anchor position,
 *                  7-second breath period.
 *   RITUAL (9s)  — 3 full revolutions with sine velocity profile.
 *
 * Cycle: 60 seconds. Repeats forever.
 *
 * Both Witness 1/12 and Witness 2/12 run this same firmware.
 *
 * NO satellite tracking, NO WiFi, NO data fetching. This is the choreography
 * for the launch video. Real satellite tracking is post-launch work.
 *
 * HARDWARE:
 *   - Seeed XIAO ESP32-S3
 *   - M5Stack Unit RollerCAN BLDC (I2C)
 *   - 15V via USB-C PD trigger board
 *   - Direct drive: 36000 steps = 360°
 *
 * Based on patterns from 03_first_witness (sine velocity profile)
 * and 02_witness_sculpture (state machine).
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <math.h>
#include "secrets.h"

// =============================================================================
// HARDWARE CONFIG
// =============================================================================

#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8
#define I2C_SCL_PIN      9
#define I2C_FREQ         400000

#define REG_OUTPUT       0x00
#define REG_MODE         0x01
#define REG_STALL_PROT   0x0F
#define REG_RGB          0x30
#define REG_SPEED        0x40
#define REG_SPEED_MAXCUR 0x50
#define REG_POS_READ     0x90

#define MODE_SPEED       1

#define STEPS_PER_REV    36000
#define CURRENT_LIMIT    200000  // 2A
#define UPDATE_RATE_HZ   100

// =============================================================================
// CHOREOGRAPHY PARAMETERS — tune these on Wednesday morning
// =============================================================================

// BREATH
#define BREATH_DURATION_MS    51000UL   // total time in BREATH per cycle
#define BREATH_AMPLITUDE_DEG  15.0f     // half-sweep, total range = 2x
#define BREATH_PERIOD_MS      7000UL    // one full sine breath cycle

// RITUAL
#define RITUAL_DURATION_MS    9000UL    // time in RITUAL per cycle
#define RITUAL_REVOLUTIONS    3         // number of full turns
#define RITUAL_PEAK_RPM       40.0f     // peak rotation speed (avg = peak/2)

// Boot
#define BOOT_DELAY_MS         3000      // wait after power on, motor settles

// =============================================================================
// STATE
// =============================================================================

enum SystemState { STATE_BOOT, STATE_BREATH, STATE_RITUAL, STATE_TESTSYNC };
SystemState currentState = STATE_BOOT;
const char* stateNames[] = { "BOOT", "BREATH", "RITUAL", "TESTSYNC" };

enum TestPhase { TEST_RELEASE, TEST_SPIN, TEST_PAUSE };
TestPhase testPhase = TEST_RELEASE;
unsigned long testPhaseStart = 0;
int32_t testCycle = 0;

#define TEST_RELEASE_MS     1000UL
#define TEST_SPIN_MS        2000UL   // 1 full revolution over 2s
#define TEST_PAUSE_MS       5000UL
#define TEST_SPIN_PEAK_RPM  60.0f    // avg 30 RPM → 1 rev in 2s

volatile bool testRequested = false;
volatile bool resumeRequested = false;

unsigned long stateStartTime = 0;
int32_t anchorSteps = 0;        // BREATH anchors here, RITUAL returns here
int32_t lastTargetSteps = 0;    // last target sent to motor
int32_t cycleCount = 0;
int32_t currentPosSteps = 0;    // live position, refreshed in loop
float   currentAngleDeg = 0.0f; // live angle 0..360

// =============================================================================
// WEB SERVER
// =============================================================================

WebServer server(80);
unsigned long lastPosPoll = 0;
bool wifiConnected = false;

void handleStatus() {
  StaticJsonDocument<384> doc;
  doc["hostname"]     = WITNESS_HOSTNAME;
  doc["state"]        = stateNames[currentState];
  doc["elapsed_ms"]   = (uint32_t)(millis() - stateStartTime);
  // Anchor is the reference angle for choreography math. The app computes
  // target = anchor_deg + choreography(state, elapsed_ms). Because
  // choreography is deterministic, the SVG and the sculpture play the same
  // score — no encoder feedback needed.
  int32_t anchorMod = anchorSteps % STEPS_PER_REV;
  if (anchorMod < 0) anchorMod += STEPS_PER_REV;
  doc["anchor_deg"]   = (float)anchorMod * 360.0f / (float)STEPS_PER_REV;
  doc["cycle"]        = cycleCount;
  doc["uptime_s"]     = (uint32_t)(millis() / 1000);
  doc["rssi"]         = WiFi.RSSI();
  doc["ip"]           = WiFi.localIP().toString();
  // Kept for observability/debug only. Not used by the app to drive the SVG.
  doc["angle_deg"]    = currentAngleDeg;
  // Choreography constants so the app never drifts from the firmware design.
  JsonObject choreo = doc.createNestedObject("choreo");
  choreo["breath_duration_ms"] = BREATH_DURATION_MS;
  choreo["breath_amplitude_deg"] = BREATH_AMPLITUDE_DEG;
  choreo["breath_period_ms"]   = BREATH_PERIOD_MS;
  choreo["ritual_duration_ms"] = RITUAL_DURATION_MS;
  choreo["ritual_revolutions"] = RITUAL_REVOLUTIONS;
  choreo["ritual_peak_rpm"]    = RITUAL_PEAK_RPM;
  choreo["boot_delay_ms"]      = BOOT_DELAY_MS;

  String out;
  serializeJson(doc, out);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", out);
}

void handleRoot() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain",
    String("Orbital Witness · ") + WITNESS_HOSTNAME + "\n"
    "GET /status              — JSON telemetry\n"
    "GET /cmd?action=testsync — enter test-sync loop\n"
    "GET /cmd?action=resume   — back to BREATH/RITUAL\n");
}

void handleCmd() {
  String action = server.arg("action");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (action == "testsync") {
    testRequested = true;
    server.send(200, "application/json", "{\"ok\":true,\"action\":\"testsync\"}");
  } else if (action == "resume") {
    resumeRequested = true;
    server.send(200, "application/json", "{\"ok\":true,\"action\":\"resume\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"unknown action\"}");
  }
}

void wifiScan() {
  Serial.println("[WiFi] scanning 2.4GHz networks...");
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    Serial.println("[WiFi] no networks visible (2.4GHz radio may not see your SSID)");
    return;
  }
  Serial.printf("[WiFi] %d networks found:\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("  %2d) RSSI=%4d  ch=%2d  '%s'\n",
      i, WiFi.RSSI(i), WiFi.channel(i), WiFi.SSID(i).c_str());
  }
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_START:
      Serial.println("\n[WiFi evt] STA_START");
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.printf("\n[WiFi evt] CONNECTED  bssid=%02X:%02X:%02X:%02X:%02X:%02X  ch=%d\n",
        info.wifi_sta_connected.bssid[0], info.wifi_sta_connected.bssid[1],
        info.wifi_sta_connected.bssid[2], info.wifi_sta_connected.bssid[3],
        info.wifi_sta_connected.bssid[4], info.wifi_sta_connected.bssid[5],
        info.wifi_sta_connected.channel);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("\n[WiFi evt] GOT_IP  %s\n",
        IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("\n[WiFi evt] DISCONNECTED  reason=%d\n",
        info.wifi_sta_disconnected.reason);
      // Common reasons:
      //  2   AUTH_EXPIRE
      //  4   ASSOC_EXPIRE
      //  15  4WAY_HANDSHAKE_TIMEOUT   ← typical bad password
      //  201 NO_AP_FOUND
      //  202 AUTH_FAIL
      //  203 ASSOC_FAIL
      //  204 HANDSHAKE_TIMEOUT        ← also bad password / WPA3 mismatch
      break;
    default: break;
  }
}

void wifiConnect() {
  WiFi.onEvent(onWiFiEvent);
  WiFi.disconnect(true, true);
  delay(200);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(WITNESS_HOSTNAME);
  wifiScan();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WiFi] connecting to '%s' (len=%d/%d)",
    WIFI_SSID, (int)strlen(WIFI_SSID), (int)strlen(WIFI_PASSWORD));
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 25000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("[WiFi] connected: %s  RSSI %d\n",
      WiFi.localIP().toString().c_str(), WiFi.RSSI());
    if (MDNS.begin(WITNESS_HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[mDNS] http://%s.local/\n", WITNESS_HOSTNAME);
    } else {
      Serial.println("[mDNS] failed");
    }
    server.on("/",       handleRoot);
    server.on("/status", handleStatus);
    server.on("/cmd",    handleCmd);
    server.onNotFound([]() {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(404, "text/plain", "not found");
    });
    server.begin();
    Serial.println("[HTTP] server on :80");
  } else {
    Serial.println("[WiFi] FAILED — continuing offline");
  }
}

// Forward declarations (state-transition functions called from update loops)
void enterBoot();
void enterBreath();
void enterRitual();
void enterTestsync();

// =============================================================================
// I2C HELPERS
// =============================================================================

void writeReg8(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void writeReg32(uint8_t reg, int32_t val) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)(val & 0xFF));
  Wire.write((uint8_t)((val >> 8) & 0xFF));
  Wire.write((uint8_t)((val >> 16) & 0xFF));
  Wire.write((uint8_t)((val >> 24) & 0xFF));
  Wire.endTransmission();
}

int32_t readReg32(uint8_t reg) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)ROLLER_I2C_ADDR, (uint8_t)4);
  int32_t val = 0;
  if (Wire.available() >= 4) {
    val = Wire.read();
    val |= (int32_t)Wire.read() << 8;
    val |= (int32_t)Wire.read() << 16;
    val |= (int32_t)Wire.read() << 24;
  }
  return val;
}

void setLED(uint8_t r, uint8_t g, uint8_t b) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(REG_RGB);
  Wire.write(r); Wire.write(g); Wire.write(b);
  Wire.endTransmission();
}

// =============================================================================
// MOTOR
// =============================================================================

void motorInit() {
  writeReg8(REG_OUTPUT, 0);
  delay(50);
  writeReg8(REG_STALL_PROT, 0);
  delay(50);
  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);
  writeReg32(REG_SPEED_MAXCUR, CURRENT_LIMIT);
  delay(50);
  writeReg32(REG_SPEED, 0);
  delay(50);
  writeReg8(REG_OUTPUT, 1);
}

void setSpeed(int32_t spd) {
  writeReg32(REG_SPEED, spd);
}

int32_t getPos() {
  return readReg32(REG_POS_READ);
}

// =============================================================================
// BREATH — sine sweep around anchor in speed mode
// =============================================================================
//
// Strategy: compute the desired position at time t, and the desired position
// one tick ahead. The required speed is the difference. Send that speed.
// This keeps us in the same control mode as RITUAL (speed mode), so we don't
// have to reconfigure the motor on each state transition.
//
// Position over time:
//   pos(t) = anchorSteps + amplitude_steps * sin(2π * t / BREATH_PERIOD_MS)
// Speed:
//   v(t) = (2π * amplitude_steps / BREATH_PERIOD_MS) * cos(2π * t / BREATH_PERIOD_MS)
//
// Speed register units: hundredths of RPM. So 100 = 1 RPM.
//   steps_per_sec → RPM: rpm = steps_per_sec * 60 / 36000

float degToSteps(float deg) {
  return deg * (STEPS_PER_REV / 360.0f);
}

void updateBreath() {
  unsigned long elapsed = millis() - stateStartTime;
  if (elapsed >= BREATH_DURATION_MS) {
    setSpeed(0);
    enterRitual();
    return;
  }

  float t_ms = (float)elapsed;
  float amplitude_steps = degToSteps(BREATH_AMPLITUDE_DEG);
  float omega = TWO_PI / (float)BREATH_PERIOD_MS;
  // Speed at this instant in steps per millisecond
  float speed_steps_per_ms = amplitude_steps * omega * cosf(omega * t_ms);
  // Convert to steps per second, then to RPM*100
  float speed_steps_per_sec = speed_steps_per_ms * 1000.0f;
  float rpm = (speed_steps_per_sec * 60.0f) / (float)STEPS_PER_REV;
  int32_t spd_register = (int32_t)(rpm * 100.0f);
  setSpeed(spd_register);
}

// =============================================================================
// RITUAL — 3 revolutions with sine velocity profile in speed mode
// =============================================================================
//
// v(t) = v_peak * (1 - cos(2π * t / RITUAL_DURATION_MS)) / 2
// Average over the duration = v_peak / 2
// Total distance = v_avg * duration = v_peak/2 * RITUAL_DURATION_MS
//
// We want total distance = RITUAL_REVOLUTIONS * STEPS_PER_REV.
// So v_peak = 2 * total_distance / RITUAL_DURATION_MS (in steps per ms)

void updateRitual() {
  unsigned long elapsed = millis() - stateStartTime;
  if (elapsed >= RITUAL_DURATION_MS) {
    setSpeed(0);
    cycleCount++;
    enterBreath();
    return;
  }

  // Target peak speed in RPM
  float peak_rpm = RITUAL_PEAK_RPM;

  // sine velocity profile: 0 at start and end, peak at middle
  float t = (float)elapsed / (float)RITUAL_DURATION_MS;
  float current_rpm = peak_rpm * (1.0f - cosf(TWO_PI * t)) * 0.5f;
  // The (1-cos)/2 form ranges 0..1..0 over t in [0,1]
  // Wait — that gives a single hump from 0 to 1 to 0. Actually we want
  // average = peak_rpm/2 over the duration, total revs = peak_rpm/2 * (duration_minutes)
  // Let's verify: integral of (1-cos(2πt))/2 from 0 to 1 = 0.5
  // So avg = peak_rpm * 0.5. For 9s and peak 40 RPM: avg 20 RPM, distance in 9s
  // = 20 RPM * (9/60) min = 3 revs. Correct.

  int32_t spd_register = (int32_t)(current_rpm * 100.0f);
  setSpeed(spd_register);
}

// =============================================================================
// STATE TRANSITIONS
// =============================================================================

void enterBoot() {
  currentState = STATE_BOOT;
  stateStartTime = millis();
  setLED(50, 50, 0);  // yellow
  setSpeed(0);
  Serial.printf(">>> STATE: BOOT (waiting %lums)\n", BOOT_DELAY_MS);
}

void enterBreath() {
  currentState = STATE_BREATH;
  stateStartTime = millis();
  anchorSteps = getPos();  // remember where we are
  setLED(0, 50, 0);  // green
  Serial.printf(">>> STATE: BREATH (cycle %d, anchor=%d)\n", cycleCount, anchorSteps);
}

void enterRitual() {
  currentState = STATE_RITUAL;
  stateStartTime = millis();
  setLED(0, 0, 50);  // blue
  Serial.printf(">>> STATE: RITUAL (cycle %d)\n", cycleCount);
}

void enterTestsync() {
  currentState = STATE_TESTSYNC;
  stateStartTime = millis();
  testPhase = TEST_RELEASE;
  testPhaseStart = millis();
  testCycle = 0;
  setSpeed(0);
  writeReg8(REG_OUTPUT, 0);  // release — free swing
  setLED(60, 10, 60);        // magenta
  Serial.println(">>> STATE: TESTSYNC (RELEASE)");
}

void updateTestsync() {
  unsigned long elapsed = millis() - testPhaseStart;
  switch (testPhase) {
    case TEST_RELEASE:
      if (elapsed >= TEST_RELEASE_MS) {
        writeReg8(REG_OUTPUT, 1);  // re-enable motor
        delay(20);
        setSpeed(0);
        testPhase = TEST_SPIN;
        testPhaseStart = millis();
        Serial.printf(">>> TESTSYNC cycle %d: SPIN\n", testCycle);
      }
      break;
    case TEST_SPIN: {
      if (elapsed >= TEST_SPIN_MS) {
        setSpeed(0);
        testPhase = TEST_PAUSE;
        testPhaseStart = millis();
        Serial.println(">>> TESTSYNC: PAUSE");
        break;
      }
      // sine velocity profile: (1-cos)/2 gives area=0.5 over t∈[0,1]
      // avg rpm = peak/2. For 2s * (peak/2) RPM/60 = 1 rev → peak = 60 RPM.
      float t = (float)elapsed / (float)TEST_SPIN_MS;
      float rpm = TEST_SPIN_PEAK_RPM * (1.0f - cosf(TWO_PI * t)) * 0.5f;
      int32_t spd = (int32_t)(rpm * 100.0f);
      setSpeed(spd);
      break;
    }
    case TEST_PAUSE:
      if (elapsed >= TEST_PAUSE_MS) {
        testCycle++;
        writeReg8(REG_OUTPUT, 0);  // release again
        testPhase = TEST_RELEASE;
        testPhaseStart = millis();
        Serial.printf(">>> TESTSYNC cycle %d: RELEASE\n", testCycle);
      }
      break;
  }
}

void updateStateMachine() {
  // Honor pending command requests at any time
  if (testRequested) {
    testRequested = false;
    enterTestsync();
    return;
  }
  if (resumeRequested) {
    resumeRequested = false;
    // Ensure motor output is enabled before resuming
    writeReg8(REG_OUTPUT, 1);
    delay(20);
    setSpeed(0);
    enterBreath();
    return;
  }

  switch (currentState) {
    case STATE_BOOT:
      if (millis() - stateStartTime >= BOOT_DELAY_MS) {
        enterBreath();
      }
      break;
    case STATE_BREATH:
      updateBreath();
      break;
    case STATE_RITUAL:
      updateRitual();
      break;
    case STATE_TESTSYNC:
      updateTestsync();
      break;
  }
}

// =============================================================================
// SETUP / LOOP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==============================================");
  Serial.println("  IDEA 3 HYBRID — BREATH + RITUAL");
  Serial.println("==============================================");
  Serial.printf("  Breath: %lus, ±%.1f°, period %lus\n",
    BREATH_DURATION_MS / 1000, BREATH_AMPLITUDE_DEG, BREATH_PERIOD_MS / 1000);
  Serial.printf("  Ritual: %lus, %d revs, peak %.1f RPM\n",
    RITUAL_DURATION_MS / 1000, RITUAL_REVOLUTIONS, RITUAL_PEAK_RPM);
  Serial.println("==============================================\n");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  // Find motor (retry up to 10 times)
  bool motorFound = false;
  for (int i = 0; i < 10; i++) {
    Wire.beginTransmission(ROLLER_I2C_ADDR);
    if (Wire.endTransmission() == 0) {
      motorFound = true;
      break;
    }
    delay(500);
  }
  if (!motorFound) {
    Serial.println("ERROR: motor not found");
    setLED(80, 0, 0);
    while(1) delay(1000);
  }
  Serial.println("Motor found.");

  motorInit();

  // Confirmation wiggle
  setSpeed(500); delay(150);
  setSpeed(-500); delay(150);
  setSpeed(0);

  wifiConnect();

  enterBoot();
}

void loop() {
  updateStateMachine();

  // Refresh live position at 20Hz (avoid saturating I2C at 100Hz)
  if (millis() - lastPosPoll >= 50) {
    lastPosPoll = millis();
    currentPosSteps = getPos();
    int32_t mod = currentPosSteps % STEPS_PER_REV;
    if (mod < 0) mod += STEPS_PER_REV;
    currentAngleDeg = (float)mod * 360.0f / (float)STEPS_PER_REV;
  }

  if (wifiConnected) server.handleClient();

  delay(1000 / UPDATE_RATE_HZ);  // 100Hz update
}
