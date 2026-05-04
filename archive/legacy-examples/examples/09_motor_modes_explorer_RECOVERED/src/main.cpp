// Witness · 09_motor_modes_explorer
// Hands-on exploration of native motor modes. No host PI, no LUTs. Just CLI.
// Library refs cite line numbers in lib/M5UnitRoller/unit_rolleri2c.hpp.

#include <Arduino.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include "unit_rolleri2c.hpp"

// ---------- Robustness ----------
// 10 s WDT covers worst-case bounded-handshake (5 retries * 250 ms ~= 1.3 s).
static constexpr uint32_t TASK_WDT_TIMEOUT_S    = 10;
// Bounds endTransmission so a wedged slave can't lock the bus forever.
static constexpr uint16_t WIRE_TIMEOUT_MS       = 50;
static constexpr int      HANDSHAKE_RETRIES     = 5;
static constexpr uint32_t HANDSHAKE_RETRY_GAP_MS = 200;
// Runtime fault detection: 5 consecutive ack failures flips driver offline.
static constexpr int      I2C_FAIL_THRESHOLD    = 5;
// Recovery cadence: 2 s between attempts so we don't hammer a dead bus.
static constexpr uint32_t I2C_RECOVERY_PERIOD_MS = 2000;
static constexpr uint32_t I2C_PROBE_PERIOD_MS   = 100;

// ---------- Hardware ----------
#define I2C_SDA      8
#define I2C_SCL      9
#define ROLLER_ADDR  0x64

#define SKETCH_VERSION "0.1"

// 100 counts per degree, verified empirically: -19770 counts = -197.70 deg.
static constexpr float COUNTS_PER_DEG = 100.0f;
// Library speed unit is 0.01 RPM/count; 1 RPM = 6 deg/s, so dps -> count: *100/6.
static constexpr float SPEED_COUNTS_PER_DPS = 100.0f / 6.0f;
// Library current unit is 0.01 mA/count.
static constexpr int32_t CURRENT_COUNTS_PER_MA = 100;
// Default max current for MODE_POSITION moves. Motor spec is 0.5 A continuous,
// so 400 mA is safely under the rating.
static constexpr int32_t DEFAULT_POS_MAX_CURRENT_MA = 400;

// ---------- Unit helpers ----------
static inline int32_t deg_to_counts(float deg) {
  return (int32_t)lroundf(deg * COUNTS_PER_DEG);
}
static inline float counts_to_deg(int32_t counts) {
  return (float)counts / COUNTS_PER_DEG;
}

// ---------- RollerDriver ----------
// Thin wrapper over UnitRollerI2C. Every public method is guarded by online_
// so a missing/dead motor never wedges the CLI. Bounded handshake.
class RollerDriver {
 public:
  // hpp:36 — bool begin(TwoWire*, uint8_t addr, uint8_t sda, uint8_t scl, uint32_t speed)
  // First attempt uses library begin(); retries use bare Wire ACK probe to
  // avoid reconfiguring pins repeatedly.
  bool begin() {
    if (roller_.begin(&Wire, ROLLER_ADDR, I2C_SDA, I2C_SCL, 400000)) {
      online_ = true;
      return true;
    }
    for (int i = 1; i < HANDSHAKE_RETRIES; i++) {
      delay(HANDSHAKE_RETRY_GAP_MS);
      Wire.beginTransmission(ROLLER_ADDR);
      if (Wire.endTransmission() == 0) { online_ = true; return true; }
    }
    online_ = false;
    return false;
  }

  bool isOnline() const { return online_; }
  void setOnline(bool v) { online_ = v; }

  // Bare ACK probe used by health tracker and recovery.
  static bool ackProbe() {
    Wire.beginTransmission(ROLLER_ADDR);
    return Wire.endTransmission() == 0;
  }

  // hpp:659 — getPosReadback() returns actual encoder position in counts.
  // (getDialCounter at hpp:714 only updates in MODE_ENCODER passive mode.)
  int32_t getEncoder() { return online_ ? roller_.getPosReadback() : 0; }

  // hpp:56 — void setMode(roller_mode_t). Mode 5 (POS_SPEED cascade) is not in
  // the enum but firmware accepts it; cast through the parameter.
  void setMode(roller_mode_t m) { if (online_) roller_.setMode(m); }
  // hpp:887 — uint8_t getMotorMode() returns the active mode byte.
  uint8_t getMode() { return online_ ? roller_.getMotorMode() : 0; }

  // hpp:158 — void setPos(int32_t pos). Raw counts.
  void setPosTarget(int32_t pos_counts) {
    if (online_) roller_.setPos(pos_counts);
  }
  // hpp:92 — void setSpeed(int32_t speed). Raw counts (0.01 RPM/count).
  // Used as target in MODE_SPEED and as max speed in MODE_POSITION.
  void setSpeedTarget(int32_t speed_counts) {
    if (online_) roller_.setSpeed(speed_counts);
  }
  // hpp:224 — void setCurrent(int32_t current). Raw counts (0.01 mA/count).
  void setCurrentTarget(int32_t current_mA_x100) {
    if (online_) roller_.setCurrent(current_mA_x100);
  }

  // hpp:74 — void setOutput(uint8_t en). 1=on, 0=off.
  void setOutput(bool on) { if (online_) roller_.setOutput(on ? 1 : 0); }
  // hpp:431 — void setStallProtection(uint8_t en). 1=enable, 0=disable.
  void setStallProtection(bool on) {
    if (online_) roller_.setStallProtection(on ? 1 : 0);
  }

  // hpp:206 — void setPosPID(uint32_t p, uint32_t i, uint32_t d).
  void setPosPID(uint32_t p, uint32_t i, uint32_t d) {
    if (online_) roller_.setPosPID(p, i, d);
  }
  // hpp:527 — void getPosPID(uint32_t* p, uint32_t* i, uint32_t* d).
  void getPosPID(uint32_t& p, uint32_t& i, uint32_t& d) {
    p = i = d = 0;
    if (online_) roller_.getPosPID(&p, &i, &d);
  }

  // hpp:176 — void setPosMaxCurrent(int32_t). Library expects 0.01 mA per count
  // (same scaling as setCurrent). Caller passes mA, we multiply by 100.
  void setPosMaxCurrent(int32_t mA) {
    if (online_) roller_.setPosMaxCurrent(mA * CURRENT_COUNTS_PER_MA);
  }
  // No dedicated PosMaxSpeed register; firmware uses SPEED_REG as the speed
  // cap during position moves. So this is just setSpeed with dps->counts.
  void setPosMaxSpeed(int32_t dps) {
    if (online_) roller_.setSpeed((int32_t)lroundf((float)dps * SPEED_COUNTS_PER_DPS));
  }

  // hpp:733 — int32_t getVin(). 0.01 V/count.
  int32_t getVinRaw() { return online_ ? roller_.getVin() : 0; }
  // hpp:752 — int32_t getTemp(). Degrees Celsius (signed integer).
  int32_t getTempC() { return online_ ? roller_.getTemp() : 0; }
  // hpp:695 — int32_t getCurrentReadback(). 0.01 mA/count.
  int32_t getCurrentReadbackRaw() { return online_ ? roller_.getCurrentReadback() : 0; }
  // hpp:981 — uint8_t getFirmwareVersion().
  uint8_t getFirmwareVersion() { return online_ ? roller_.getFirmwareVersion() : 0; }
  // hpp:866 — uint8_t getOutputStatus(). For wiggle save/restore.
  uint8_t getOutputStatus() { return online_ ? roller_.getOutputStatus() : 0; }

  // hpp:463 — void startAngleCal(). Kicks off internal encoder/FOC angle
  // calibration. Motor will rotate while it runs.
  void runAngleCal() { if (online_) roller_.startAngleCal(); }
  // hpp:489 — uint8_t getCalBusyStatus(). 1=calibrating, 0=idle.
  uint8_t getCalBusyStatus() { return online_ ? roller_.getCalBusyStatus() : 0; }
  // hpp:361 — void saveConfigToFlash(). Persists current config across reboots.
  void saveToFlash() { if (online_) roller_.saveConfigToFlash(); }

 private:
  UnitRollerI2C roller_;
  bool          online_ = false;
};

static RollerDriver driver_;

// ---------- I2C health / non-blocking recovery ----------
static int      i2c_consecutive_fails_ = 0;
static uint32_t i2c_last_probe_ms_     = 0;
static uint32_t i2c_next_recovery_ms_  = 0;

// Light ACK probe at fixed cadence; trips offline at threshold.
static void i2cHealthCheck() {
  uint32_t now = millis();
  if (!driver_.isOnline()) return;
  if (now - i2c_last_probe_ms_ < I2C_PROBE_PERIOD_MS) return;
  i2c_last_probe_ms_ = now;
  if (RollerDriver::ackProbe()) { i2c_consecutive_fails_ = 0; return; }
  i2c_consecutive_fails_++;
  if (i2c_consecutive_fails_ >= I2C_FAIL_THRESHOLD) {
    Serial.println("\nI2C FAULT: roller stopped acking. Driver OFFLINE.");
    driver_.setOnline(false);
    i2c_next_recovery_ms_ = now + I2C_RECOVERY_PERIOD_MS;
  }
}

// Try one Wire reset + handshake every I2C_RECOVERY_PERIOD_MS while offline.
static void i2cRecoveryService() {
  if (driver_.isOnline()) return;
  uint32_t now = millis();
  if (now < i2c_next_recovery_ms_) return;
  i2c_next_recovery_ms_ = now + I2C_RECOVERY_PERIOD_MS;
  // Reset state and try a fresh begin/probe. Non-blocking: returns either way.
  Wire.end();
  delay(100);
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  Wire.setTimeOut(WIRE_TIMEOUT_MS);
  esp_task_wdt_reset();
  if (driver_.begin()) {
    Wire.setTimeOut(WIRE_TIMEOUT_MS);
    Serial.println("\nI2C RECOVERED: driver back online.");
    i2c_consecutive_fails_ = 0;
  }
}

// ---------- Helpers ----------
static const char* modeName(uint8_t m) {
  switch (m) {
    case 1: return "SPEED";
    case 2: return "POSITION";
    case 3: return "CURRENT";
    case 4: return "ENCODER";
    case 5: return "POS_SPEED";
    default: return "?";
  }
}

static void printPrompt() { Serial.print("> "); }

static void printHelp() {
  Serial.println("Single-key (then Enter):");
  Serial.println("  1            move to absolute 0 deg and hold");
  Serial.println("  2            +360 deg from current position");
  Serial.println("  3            move to absolute 180 deg");
  Serial.println("Commands:");
  Serial.println("  help | ?     this help");
  Serial.println("  status       angle, mode, output, vin, temp, current");
  Serial.println("  stop         setOutput(false), motor goes limp");
  Serial.println("  start        setOutput(true)");
  Serial.println("  mode <n>     1=SPEED 2=POSITION 3=CURRENT 5=POS_SPEED");
  Serial.println("  current <mA> switch to MODE_CURRENT and set torque");
  Serial.println("  speed <dps>  speed target / max speed (mode-dependent)");
  Serial.println("  maxc <mA>    set MODE_POSITION max current");
  Serial.println("  pospid <p> <i> <d>  set position PID gains (uint32)");
  Serial.println("  pid          read current position PID");
  Serial.println("  power        vin / temp / current_readback / fw_version");
  Serial.println("  wiggle       +250 / -250 / +250 mA proof-of-life");
  Serial.println("  breathe <amp> <period_ms> [dur_s] — sinusoidal current, no encoder needed");
  Serial.println("  cal          run encoder/FOC angle calibration");
  Serial.println("  reboot|reset ESP.restart()");
}

static void printStatus() {
  if (!driver_.isOnline()) {
    Serial.println("driver OFFLINE — only CLI works. Try 'reboot'.");
    return;
  }
  int32_t enc      = driver_.getEncoder();
  uint8_t m        = driver_.getMode();
  int32_t vin_raw  = driver_.getVinRaw();
  int32_t temp_c   = driver_.getTempC();
  int32_t cur_raw  = driver_.getCurrentReadbackRaw();
  uint8_t out      = driver_.getOutputStatus();
  Serial.printf("angle    : %.2f deg (%d counts)\n", counts_to_deg(enc), enc);
  Serial.printf("mode     : %u (%s)\n", m, modeName(m));
  Serial.printf("output   : %s\n", out ? "ON" : "OFF");
  Serial.printf("vin      : %.2f V\n", vin_raw / 100.0f);
  Serial.printf("temp     : %d C\n", temp_c);
  Serial.printf("current  : %.2f mA (readback)\n", cur_raw / 100.0f);
}

static void printPower() {
  if (!driver_.isOnline()) { Serial.println("driver OFFLINE"); return; }
  float vin_v = driver_.getVinRaw() / 100.0f;
  float cur_mA = driver_.getCurrentReadbackRaw() / 100.0f;
  Serial.printf("vin      : %.2f V\n", vin_v);
  Serial.printf("temp     : %d C\n", driver_.getTempC());
  Serial.printf("current  : %.2f mA\n", cur_mA);
  Serial.printf("fw_ver   : %u\n", driver_.getFirmwareVersion());
  if (vin_v >= 10.0f) Serial.println("vin >= 10 V -> OK");
  else                Serial.println("vin <  10 V -> LOW (motor on bus power)");
}

// Brief ±250 mA blip to confirm power and see motion. Saves and restores prior
// mode and output state so it can be invoked from any mode safely.
static void doWiggle() {
  if (!driver_.isOnline()) { Serial.println("driver OFFLINE"); return; }
  uint8_t prev_mode   = driver_.getMode();
  uint8_t prev_output = driver_.getOutputStatus();
  Serial.printf("wiggle: prev mode=%u output=%u\n", prev_mode, prev_output);
  driver_.setMode(ROLLER_MODE_CURRENT);
  driver_.setOutput(true);
  esp_task_wdt_reset();
  driver_.setCurrentTarget(+250 * CURRENT_COUNTS_PER_MA); delay(300);
  esp_task_wdt_reset();
  driver_.setCurrentTarget(-250 * CURRENT_COUNTS_PER_MA); delay(300);
  esp_task_wdt_reset();
  driver_.setCurrentTarget(+250 * CURRENT_COUNTS_PER_MA); delay(300);
  esp_task_wdt_reset();
  driver_.setCurrentTarget(0);
  // Restore. Cast prev_mode through roller_mode_t so mode 5 round-trips.
  driver_.setMode((roller_mode_t)prev_mode);
  driver_.setOutput(prev_output != 0);
  Serial.printf("wiggle done. restored mode=%u output=%u\n",
    prev_mode, prev_output);
}

// Run the encoder/FOC angle calibration. Internal factory routine — the M5
// firmware spins the motor through a known arc and rebuilds the angle table.
// Motor must be free to rotate during this. Caller's pointer/load must not
// resist motion or the cal will be biased.
static void doAngleCal() {
  if (!driver_.isOnline()) { Serial.println("driver OFFLINE"); return; }
  Serial.println("Running angle calibration. Motor will rotate. Pointer must be free to spin.");
  // Stall protection off so the cal sweep isn't aborted by transient torque.
  driver_.setStallProtection(false);
  delay(20);
  esp_task_wdt_reset();
  driver_.runAngleCal();
  // Poll cal-busy with WDT pets every 200 ms. Hard cap at ~6 s so a wedged
  // status read can't hang us. Library doc on startAngleCal doesn't specify
  // duration; bench behavior is typically a couple of seconds.
  const uint32_t kCalTimeoutMs = 6000;
  const uint32_t kPollGapMs    = 200;
  uint32_t start = millis();
  uint8_t busy = 0;
  while (millis() - start < kCalTimeoutMs) {
    delay(kPollGapMs);
    esp_task_wdt_reset();
    busy = driver_.getCalBusyStatus();
    if (busy == 0 && millis() - start > 500) break;  // ignore the first 500 ms
  }
  if (busy) {
    Serial.println("cal: timed out waiting for busy=0; persisting anyway.");
  }
  // Brief settle, then commit to flash so the freshly-built angle table
  // survives a reboot.
  delay(200);
  esp_task_wdt_reset();
  driver_.saveToFlash();
  delay(150);  // saveConfigToFlash docs say ~100 ms internal delay
  esp_task_wdt_reset();
  // Re-apply the post-cal posture. Mirrors the setup() startup posture so
  // MODE_POSITION holds at the reading the encoder gives us right now.
  driver_.setStallProtection(false);
  delay(20);
  driver_.setMode(ROLLER_MODE_POSITION);
  delay(20);
  driver_.setPosMaxCurrent(DEFAULT_POS_MAX_CURRENT_MA);
  delay(20);
  int32_t enc = driver_.getEncoder();
  driver_.setPosTarget(enc);
  delay(20);
  driver_.setOutput(true);
  delay(20);
  Serial.printf("cal complete. angle=%.2f deg, mode=%u\n",
    counts_to_deg(enc), driver_.getMode());
}

static void doReboot() {
  Serial.println("rebooting...");
  Serial.flush();
  delay(50);
  ESP.restart();
}

// Sinusoidal current drive. No encoder needed. Core of the BREATH gesture:
// current(t) = amp_mA * sin(2*pi * t / period_ms). Hard cap at 400 mA (motor
// continuous rating is 0.5 A). Stops on any keystroke or after duration_s.
// duration_s == 0 -> run forever until keystroke.
static void doBreathe(int32_t amp_mA, int32_t period_ms, int32_t duration_s) {
  if (!driver_.isOnline()) { Serial.println("driver OFFLINE"); return; }
  // Hard cap: motor protection is non-negotiable.
  static constexpr int32_t kBreatheMaxMA = 400;
  if (amp_mA < 0) amp_mA = -amp_mA;
  if (amp_mA > kBreatheMaxMA) {
    Serial.printf("breathe: amp clamped %d -> %d mA (cap)\n", (int)amp_mA, (int)kBreatheMaxMA);
    amp_mA = kBreatheMaxMA;
  }
  if (period_ms < 100) {
    Serial.printf("breathe: period %d ms too small, using 100 ms\n", (int)period_ms);
    period_ms = 100;
  }
  if (duration_s < 0) duration_s = 0;

  Serial.printf("breathe: amp=%dmA period=%dms duration=%ds. press any key to stop.\n",
    (int)amp_mA, (int)period_ms, (int)duration_s);

  // Drain residual chars (the Enter that submitted this command) before
  // entering the loop, otherwise we self-trigger the keystroke-stop check.
  while (Serial.available() > 0) Serial.read();

  driver_.setMode(ROLLER_MODE_CURRENT);
  delay(10);
  driver_.setOutput(true);
  delay(10);

  const float two_pi      = 6.283185307179586f;
  const float inv_period  = 1.0f / (float)period_ms;
  const uint32_t start_ms = millis();
  const uint32_t tick_ms  = 20;  // ~50 Hz
  uint32_t next_tick = start_ms;
  uint32_t t_ms = 0;
  bool stopped_by_key = false;

  while (true) {
    esp_task_wdt_reset();

    // Stop on any keystroke. Drain so we don't leave junk in the buffer.
    if (Serial.available() > 0) {
      while (Serial.available() > 0) Serial.read();
      stopped_by_key = true;
      break;
    }

    t_ms = millis() - start_ms;
    if (duration_s > 0 && t_ms >= (uint32_t)duration_s * 1000UL) break;

    float phase = two_pi * (float)t_ms * inv_period;
    float c_mA  = (float)amp_mA * sinf(phase);
    // Belt and suspenders: clamp regardless of amp.
    if (c_mA >  (float)kBreatheMaxMA) c_mA =  (float)kBreatheMaxMA;
    if (c_mA < -(float)kBreatheMaxMA) c_mA = -(float)kBreatheMaxMA;

    driver_.setCurrentTarget((int32_t)lroundf(c_mA) * CURRENT_COUNTS_PER_MA);

    // Compact log every 200 ms: t cmd ang ia. One line, no labels-per-tick.
    static uint32_t next_log_ms = 0;
    if (millis() >= next_log_ms) {
      int32_t enc      = driver_.getEncoder();
      int32_t ia_raw   = driver_.getCurrentReadbackRaw();
      int    ang_int   = (int)((float)enc * 0.01f);
      int    ia_int    = (int)((float)ia_raw * 0.01f);
      Serial.printf("%lu %d %d %d\n",
        (unsigned long)t_ms, (int)lroundf(c_mA), ang_int, ia_int);
      next_log_ms = millis() + 200;
    }

    // Pace at ~50 Hz without busy-waiting.
    next_tick += tick_ms;
    int32_t sleep_ms = (int32_t)(next_tick - millis());
    if (sleep_ms < 0) { next_tick = millis(); sleep_ms = 0; }
    if (sleep_ms > (int32_t)tick_ms) sleep_ms = tick_ms;
    if (sleep_ms > 0) delay(sleep_ms);
  }

  // Brief decay: ramp target to 0 and let the controller settle.
  driver_.setCurrentTarget(0);
  uint32_t decay_start = millis();
  while (millis() - decay_start < 200) {
    esp_task_wdt_reset();
    delay(20);
  }
  Serial.printf("breathe stopped at t=%ums.%s\n",
    (unsigned)t_ms, stopped_by_key ? " (key)" : "");
}

// ---------- CLI ----------
// Buffer fills until LF/CR, then dispatched. One full line per call.
static char    line_buf_[96];
static size_t  line_len_ = 0;

static void splitTokens(char* line, char* tokens[], int max_tokens, int& n) {
  n = 0;
  char* p = line;
  while (*p && n < max_tokens) {
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) break;
    tokens[n++] = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    if (*p) { *p = '\0'; p++; }
  }
}

static void handleLine(char* line) {
  // Skip empty lines (just Enter).
  while (*line == ' ' || *line == '\t') line++;
  if (*line == '\0') { printPrompt(); return; }

  // Single-key triggers — exact match before tokenizing.
  if (strcmp(line, "1") == 0) {
    int32_t target = deg_to_counts(0.0f);
    driver_.setPosTarget(target);
    Serial.printf("KEY 1 -> target=0 deg (counts=%d)\n", target);
    printPrompt(); return;
  }
  if (strcmp(line, "2") == 0) {
    int32_t cur    = driver_.getEncoder();
    int32_t target = cur + deg_to_counts(360.0f);
    driver_.setPosTarget(target);
    Serial.printf("KEY 2 -> +360 deg from current (target_counts=%d)\n", target);
    printPrompt(); return;
  }
  if (strcmp(line, "3") == 0) {
    int32_t target = deg_to_counts(180.0f);
    driver_.setPosTarget(target);
    Serial.printf("KEY 3 -> target=180 deg (counts=%d)\n", target);
    printPrompt(); return;
  }

  char* tokens[6];
  int   n = 0;
  splitTokens(line, tokens, 6, n);
  if (n == 0) { printPrompt(); return; }
  const char* cmd = tokens[0];

  if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
    printHelp();
  } else if (!strcmp(cmd, "status")) {
    printStatus();
  } else if (!strcmp(cmd, "stop")) {
    driver_.setOutput(false);
    Serial.println("output=OFF (motor limp)");
  } else if (!strcmp(cmd, "start")) {
    driver_.setOutput(true);
    Serial.println("output=ON");
  } else if (!strcmp(cmd, "mode")) {
    if (n < 2) { Serial.println("usage: mode <n>  (1|2|3|5)"); }
    else {
      int m = atoi(tokens[1]);
      if (m == 1 || m == 2 || m == 3 || m == 5) {
        driver_.setMode((roller_mode_t)m);
        delay(20);
        Serial.printf("mode=%d (%s)  readback=%u\n",
          m, modeName(m), driver_.getMode());
      } else {
        Serial.println("invalid mode. valid: 1 SPEED, 2 POSITION, 3 CURRENT, 5 POS_SPEED");
      }
    }
  } else if (!strcmp(cmd, "current")) {
    if (n < 2) { Serial.println("usage: current <mA>"); }
    else {
      int32_t mA = atol(tokens[1]);
      driver_.setMode(ROLLER_MODE_CURRENT);
      delay(10);
      driver_.setCurrentTarget(mA * CURRENT_COUNTS_PER_MA);
      Serial.printf("MODE_CURRENT  current=%d mA (counts=%d)\n",
        (int)mA, (int)(mA * CURRENT_COUNTS_PER_MA));
    }
  } else if (!strcmp(cmd, "speed")) {
    if (n < 2) { Serial.println("usage: speed <dps>"); }
    else {
      int32_t dps = atol(tokens[1]);
      int32_t counts = (int32_t)lroundf((float)dps * SPEED_COUNTS_PER_DPS);
      driver_.setSpeedTarget(counts);
      Serial.printf("speed=%d dps (counts=%d) — target in MODE_SPEED, max in MODE_POSITION\n",
        (int)dps, (int)counts);
    }
  } else if (!strcmp(cmd, "maxc")) {
    if (n < 2) { Serial.println("usage: maxc <mA>"); }
    else {
      int32_t mA = atol(tokens[1]);
      driver_.setPosMaxCurrent(mA);
      Serial.printf("pos_max_current=%d mA\n", (int)mA);
    }
  } else if (!strcmp(cmd, "pospid")) {
    if (n < 4) { Serial.println("usage: pospid <p> <i> <d>"); }
    else {
      uint32_t p = (uint32_t)strtoul(tokens[1], nullptr, 10);
      uint32_t i = (uint32_t)strtoul(tokens[2], nullptr, 10);
      uint32_t d = (uint32_t)strtoul(tokens[3], nullptr, 10);
      driver_.setPosPID(p, i, d);
      delay(20);
      uint32_t rp = 0, ri = 0, rd = 0;
      driver_.getPosPID(rp, ri, rd);
      Serial.printf("pospid set p=%u i=%u d=%u  readback p=%u i=%u d=%u\n",
        p, i, d, rp, ri, rd);
    }
  } else if (!strcmp(cmd, "pid")) {
    uint32_t p = 0, i = 0, d = 0;
    driver_.getPosPID(p, i, d);
    Serial.printf("pos_pid p=%u i=%u d=%u\n", p, i, d);
  } else if (!strcmp(cmd, "power")) {
    printPower();
  } else if (!strcmp(cmd, "wiggle")) {
    doWiggle();
  } else if (!strcmp(cmd, "breathe")) {
    // Defaults: 150 mA peak, 7 s period, 60 s duration.
    int32_t amp_mA     = 150;
    int32_t period_ms  = 7000;
    int32_t duration_s = 60;
    bool bad = false;
    if (n >= 2) {
      char* end = nullptr;
      long v = strtol(tokens[1], &end, 10);
      if (end == tokens[1] || *end != '\0') bad = true;
      else amp_mA = (int32_t)v;
    }
    if (!bad && n >= 3) {
      char* end = nullptr;
      long v = strtol(tokens[2], &end, 10);
      if (end == tokens[2] || *end != '\0' || v <= 0) bad = true;
      else period_ms = (int32_t)v;
    }
    if (!bad && n >= 4) {
      char* end = nullptr;
      long v = strtol(tokens[3], &end, 10);
      if (end == tokens[3] || *end != '\0' || v < 0) bad = true;
      else duration_s = (int32_t)v;
    }
    if (bad) {
      Serial.println("usage: breathe <amp_mA> <period_ms> [duration_s]");
      Serial.println("  defaults: amp=150 period=7000 duration=60. cap: |amp|<=400 mA.");
    } else {
      doBreathe(amp_mA, period_ms, duration_s);
    }
  } else if (!strcmp(cmd, "cal")) {
    doAngleCal();
  } else if (!strcmp(cmd, "reboot") || !strcmp(cmd, "reset")) {
    doReboot();
  } else {
    Serial.printf("unknown: %s — try 'help'\n", cmd);
  }
  printPrompt();
}

static void serviceSerial() {
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c < 0) break;
    if (c == '\n' || c == '\r') {
      if (line_len_ == 0) continue;        // ignore bare CR after LF or vice versa
      line_buf_[line_len_] = '\0';
      handleLine(line_buf_);
      line_len_ = 0;
    } else if (line_len_ < sizeof(line_buf_) - 1) {
      line_buf_[line_len_++] = (char)c;
    }
    // Overflow chars dropped silently — exploration sketch, not safety-critical.
  }
}

// ---------- Setup / Loop ----------
void setup() {
  // WDT FIRST — last-ditch safety against any later wedge.
  esp_task_wdt_init(TASK_WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(500);
  esp_task_wdt_reset();

  Serial.println();
  Serial.println("==========================================================");
  Serial.printf (" Witness · 09_motor_modes_explorer  sketch=%s\n", SKETCH_VERSION);
  Serial.println("==========================================================");

  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  Wire.setTimeOut(WIRE_TIMEOUT_MS);
  esp_task_wdt_reset();

  bool ok = driver_.begin();
  // Library begin() reconfigures Wire and resets timeout — re-apply ours.
  Wire.setTimeOut(WIRE_TIMEOUT_MS);
  esp_task_wdt_reset();

  if (!ok) {
    Serial.println("WARNING: roller not responding on I2C.");
    Serial.println("CLI active. Run 'reboot' or check hardware.");
  } else {
    Serial.printf("roller online at 0x%02X. fw_ver=%u\n",
      ROLLER_ADDR, driver_.getFirmwareVersion());
    // Default startup posture: stall protection off (we'll be pushing the
    // motor by hand), POSITION mode, output enabled. Hold at current angle.
    driver_.setStallProtection(false);
    delay(20);
    driver_.setMode(ROLLER_MODE_POSITION);
    delay(20);
    driver_.setPosMaxCurrent(DEFAULT_POS_MAX_CURRENT_MA);
    delay(20);
    int32_t enc = driver_.getEncoder();
    driver_.setPosTarget(enc);   // hold here so output=true doesn't slam to 0
    delay(20);
    driver_.setOutput(true);
    delay(20);
    float vin_v = driver_.getVinRaw() / 100.0f;
    Serial.printf("vin=%.2f V  angle=%.2f deg (%d counts)  mode=%u (%s)\n",
      vin_v, counts_to_deg(enc), enc,
      driver_.getMode(), modeName(driver_.getMode()));
  }

  Serial.println("type 'help' for commands");
  printPrompt();
}

void loop() {
  esp_task_wdt_reset();
  i2cHealthCheck();
  i2cRecoveryService();
  serviceSerial();
  delay(1);
}
