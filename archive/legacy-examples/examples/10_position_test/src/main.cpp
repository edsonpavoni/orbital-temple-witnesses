// Witness · 10_position_test
// Send the motor to a specific angle. Slowly. Hold there.
// Built directly from the M5 manufacturer example pattern, with correct unit
// scaling: setPos in 0.01 deg/count, setSpeed in 0.01 RPM/count,
// setPosMaxCurrent in 0.01 mA/count.

#include <Arduino.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include "unit_rolleri2c.hpp"

#define I2C_SDA      8
#define I2C_SCL      9
#define ROLLER_ADDR  0x64

// Hardware safety: motor is 0.5 A continuous, 1 A short-term.
// 400 mA leaves headroom and matches what we proved safe earlier.
static constexpr int32_t MAX_CURRENT_MA = 400;

UnitRollerI2C roller;

// Move to absolute angle in degrees. Uses MODE_POSITION (manufacturer's
// recommended mode for go-to-position). In this mode there is no speed cap;
// arrival speed depends on max current and PID. Lower max current = slower.
//
// max_mA approximate guide on D3504 motor unloaded:
//   400 mA -> fast (full speed in <1s)
//   100 mA -> moderate
//    50 mA -> visibly slow
//    25 mA -> very slow, may not break cogging cusps
void moveTo(float deg, int max_mA) {
  if (max_mA < 10)   max_mA = 10;
  if (max_mA > 400)  max_mA = 400;
  Serial.printf("\n-> moveTo %.2f deg, max_current=%d mA\n", deg, max_mA);

  // Toggle output OFF so target/maxc updates take effect on next ON.
  roller.setOutput(0);
  delay(50);
  roller.setMode(ROLLER_MODE_POSITION);          // mode 2, M5 manufacturer pattern
  delay(50);
  roller.setPos((int32_t)(deg * 100.0f));        // 0.01 deg/count
  delay(50);
  roller.setPosMaxCurrent(max_mA * 100);         // 0.01 mA/count
  delay(50);
  roller.setOutput(1);
}

void setup() {
  // Watchdog first so any wedge during setup auto-reboots in 10s.
  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);

  Serial.begin(115200);
  delay(1500);

  Serial.println("\n========================================");
  Serial.println("  Witness  10_position_test  v0.1");
  Serial.println("========================================");

  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  Wire.setTimeOut(50);

  // Bounded handshake so a missing roller doesn't wedge setup.
  bool ok = false;
  for (int i = 0; i < 5; i++) {
    if (roller.begin(&Wire, ROLLER_ADDR, I2C_SDA, I2C_SCL, 400000)) { ok = true; break; }
    Serial.printf("handshake retry %d/5...\n", i+1);
    delay(200);
    esp_task_wdt_reset();
  }
  Wire.setTimeOut(50);  // library begin reconfigures Wire; reapply

  if (!ok) {
    Serial.println("ERROR: roller not found. CLI inactive. Reboot to retry.");
    while (1) { esp_task_wdt_reset(); delay(500); }
  }

  Serial.printf("roller online. fw_ver=%d  vin=%.2f V  angle=%.2f deg\n",
    (int)roller.getFirmwareVersion(),
    roller.getVin() / 100.0f,
    roller.getPosReadback() / 100.0f);

  roller.setStallProtection(0);

  // Park at current angle so it doesn't slam to 0 on boot.
  float here = roller.getPosReadback() / 100.0f;
  moveTo(here, 50);  // park at current angle, gentle 50 mA cap

  Serial.println("\ncommands:");
  Serial.println("  <number>           go to that angle, 50 mA cap (gentle/slow)");
  Serial.println("  go <deg> <mA>      go to <deg> with custom max current");
  Serial.println("  stop               motor limp");
  Serial.println("  status             angle, current, mode, vin, temp");
  Serial.println("  reboot             software reset");
  Serial.println("");
  Serial.println("  current cap shapes speed: 25=very slow, 50=slow, 100=moderate, 400=fast");
  Serial.println("");
  Serial.print("> ");
}

static char   line[80];
static size_t len = 0;

void handleLine(const char* s) {
  if (!*s) { Serial.print("> "); return; }

  if (!strcmp(s, "stop")) {
    roller.setOutput(0);
    Serial.println("output OFF");
  } else if (!strcmp(s, "reboot") || !strcmp(s, "reset")) {
    Serial.println("rebooting...");
    Serial.flush();
    ESP.restart();
  } else if (!strcmp(s, "status")) {
    Serial.printf("angle=%.2f deg  current=%.2f mA  mode=%d  vin=%.2f V  temp=%d C  output=%d\n",
      roller.getPosReadback() / 100.0f,
      roller.getCurrentReadback() / 100.0f,
      (int)roller.getMotorMode(),
      roller.getVin() / 100.0f,
      (int)roller.getTemp(),
      (int)roller.getOutputStatus());
  } else if (!strncmp(s, "go ", 3)) {
    float deg = 0;
    int   mA  = 50;
    int n = sscanf(s + 3, "%f %d", &deg, &mA);
    if (n >= 1) {
      if (mA < 10) mA = 10;
      moveTo(deg, mA);
    } else {
      Serial.println("usage: go <deg> [<mA>]");
    }
  } else {
    // Try parse as a bare number = target degrees, default gentle 50 mA.
    char* endp = nullptr;
    float deg = strtof(s, &endp);
    if (endp != s) {
      moveTo(deg, 50);
    } else {
      Serial.printf("unknown: '%s'\n", s);
    }
  }

  Serial.print("> ");
}

void loop() {
  esp_task_wdt_reset();

  // Read serial line by line.
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (len > 0) {
        line[len] = 0;
        // Trim trailing whitespace.
        while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\t')) line[--len] = 0;
        handleLine(line);
        len = 0;
      } else if (c == '\n') {
        // Empty line: just reprint prompt.
        Serial.print("> ");
      }
    } else if (len < sizeof(line) - 1) {
      line[len++] = (char)c;
    }
  }

  // Live progress print every 200 ms.
  static uint32_t next_log = 0;
  if (millis() >= next_log) {
    static float last_deg = 999999.0f;
    float deg = roller.getPosReadback() / 100.0f;
    // Print only if it changed by >= 0.5 deg, to keep the log compact.
    if (fabsf(deg - last_deg) >= 0.5f) {
      Serial.printf("  %.2f deg  %.0f mA\n", deg, roller.getCurrentReadback() / 100.0f);
      last_deg = deg;
    }
    next_log = millis() + 200;
  }
}
