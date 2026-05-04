// Witness · 09_motor_modes_explorer  (MONOLITHIC pre-refactor copy)
// Hands-on exploration of native motor modes. No host PI, no LUTs. Just CLI.
// This is the single-file pre-refactor version, kept beside the modular
// 09_motor_modes_explorer for A/B testing.

#include <Arduino.h>
#include <Wire.h>
#include <esp_task_wdt.h>

#include "unit_rolleri2c.hpp"  // pulls in ROLLER_MODE_* enum names

// ---------- Hardware ----------
#define I2C_SDA      8
#define I2C_SCL      9
#define ROLLER_ADDR  0x64
#define I2C_FREQ_HZ  400000

#define SKETCH_VERSION "0.1"

// ---------- Robustness ----------
// 10 s WDT covers worst-case bounded-handshake (5 retries * ~250 ms ~= 1.3 s).
static constexpr uint32_t TASK_WDT_TIMEOUT_S    = 10;
// Bounds endTransmission so a wedged slave can't lock the bus forever.
static constexpr uint16_t WIRE_TIMEOUT_MS       = 50;
// Bounded-handshake / I2C health-recovery tunables.
static constexpr int      HANDSHAKE_RETRIES       = 5;
static constexpr uint32_t HANDSHAKE_RETRY_GAP_MS  = 200;
static constexpr int      I2C_FAIL_THRESHOLD      = 5;
static constexpr uint32_t I2C_RECOVERY_PERIOD_MS  = 2000;
static constexpr uint32_t I2C_PROBE_PERIOD_MS     = 100;

// ---------- Motor units ----------
// 100 counts per degree, verified empirically: -19770 counts = -197.70 deg.
static constexpr float   COUNTS_PER_DEG          = 100.0f;
// Library speed unit is 0.01 RPM/count; 1 RPM = 6 deg/s, so dps -> count: *100/6.
static constexpr float   SPEED_COUNTS_PER_DPS    = 100.0f / 6.0f;
// Library current unit is 0.01 mA/count.
static constexpr int32_t CURRENT_COUNTS_PER_MA   = 100;
// Default max current for MODE_POSITION moves. Motor spec is 0.5 A continuous,
// so 400 mA is safely under the rating.
static constexpr int32_t DEFAULT_POS_MAX_CURRENT_MA = 400;

// ===========================================================================
// Watchdog  (stateless wrapper around esp_task_wdt; namespace, not a class)
// ===========================================================================
namespace Watchdog {

  // Arm WDT and add the current task. Call first thing in setup.
  inline void arm(uint32_t timeout_seconds) {
    // Panic-on-timeout (true) so a wedge resets the chip rather than hanging.
    esp_task_wdt_init(timeout_seconds, true);
    esp_task_wdt_add(NULL);
  }

  // Reset the WDT for the current task. Call from loop and any long path.
  inline void pet() {
    esp_task_wdt_reset();
  }

  // Soft restart. Wraps ESP.restart() so callers don't include esp_system.
  inline void rebootChip() {
    ESP.restart();
  }

}  // namespace Watchdog

// ===========================================================================
// RollerDriver
// Thin wrapper over UnitRollerI2C. Owns bounded-timeout I/O, online state,
// and the non-blocking I2C health/recovery state machine. Every public method
// guards on online_ so a missing/dead motor never wedges callers.
// ===========================================================================
class RollerDriver {
 public:
  // Bounded handshake. Returns true if roller responded within retries.
  // Stores wire/addr for later recovery service.
  bool begin(TwoWire& wire, uint8_t addr_7bit, int sda_pin, int scl_pin, uint32_t freq_hz);

  bool isOnline() const { return online_; }

  // Hot-path. All return zero / no-op when offline.
  int32_t getEncoder();
  void    setMode(uint8_t mode);            // raw byte; covers mode 5 cast
  uint8_t getMotorMode();
  void    setOutput(bool on);
  uint8_t getOutputStatus();
  void    setStallProtection(bool on);
  void    setPosTarget(int32_t pos_counts);
  void    setSpeedTarget(int32_t speed_counts);
  void    setCurrentTarget(int32_t current_counts);   // 0.01 mA per count
  void    setPosMaxCurrent(int32_t mA);
  void    setPosPID(uint32_t p, uint32_t i, uint32_t d);
  void    getPosPID(uint32_t& p, uint32_t& i, uint32_t& d);

  // Slow-path / telemetry.
  int32_t getVinRaw();             // 0.01 V per count
  int32_t getTempC();               // C
  int32_t getCurrentReadbackRaw();  // 0.01 mA per count
  uint8_t getFirmwareVersion();

  // Health: probe + non-blocking recovery. Call from loop().
  void serviceHealth();

 private:
  // Bare ACK probe: 0 == ack received.
  bool ackProbe_();
  // Internal handshake helper used by both begin() and recovery.
  bool handshake_();

  UnitRollerI2C roller_;
  TwoWire*      wire_       = nullptr;
  uint8_t       addr_       = 0;
  int           sda_pin_    = -1;
  int           scl_pin_    = -1;
  uint32_t      freq_hz_    = 0;
  bool          online_     = false;

  // Health state.
  int       consecutive_fails_ = 0;
  uint32_t  last_probe_ms_     = 0;
  uint32_t  next_recovery_ms_  = 0;
};

bool RollerDriver::ackProbe_() {
  if (!wire_) return false;
  wire_->beginTransmission(addr_);
  return wire_->endTransmission() == 0;
}

bool RollerDriver::handshake_() {
  // First attempt uses library begin(); retries use bare Wire ACK probe to
  // avoid reconfiguring pins repeatedly.
  if (roller_.begin(wire_, addr_, sda_pin_, scl_pin_, freq_hz_)) {
    online_ = true;
    return true;
  }
  for (int i = 1; i < HANDSHAKE_RETRIES; i++) {
    delay(HANDSHAKE_RETRY_GAP_MS);
    if (ackProbe_()) { online_ = true; return true; }
  }
  online_ = false;
  return false;
}

bool RollerDriver::begin(TwoWire& wire, uint8_t addr_7bit, int sda_pin, int scl_pin, uint32_t freq_hz) {
  wire_    = &wire;
  addr_    = addr_7bit;
  sda_pin_ = sda_pin;
  scl_pin_ = scl_pin;
  freq_hz_ = freq_hz;
  return handshake_();
}

int32_t RollerDriver::getEncoder() {
  // hpp:714 - getDialCounter returns encoder counts.
  return online_ ? roller_.getDialCounter() : 0;
}

void RollerDriver::setMode(uint8_t mode) {
  // hpp:56 - setMode(roller_mode_t). Mode 5 (POS_SPEED cascade) not in enum
  // but firmware accepts it; cast through the parameter.
  if (online_) roller_.setMode((roller_mode_t)mode);
}

uint8_t RollerDriver::getMotorMode() {
  // hpp:887 - getMotorMode returns the active mode byte.
  return online_ ? roller_.getMotorMode() : 0;
}

void RollerDriver::setOutput(bool on) {
  // hpp:74 - setOutput(uint8_t en). 1=on, 0=off.
  if (online_) roller_.setOutput(on ? 1 : 0);
}

uint8_t RollerDriver::getOutputStatus() {
  // hpp:866 - getOutputStatus, used by wiggle save/restore.
  return online_ ? roller_.getOutputStatus() : 0;
}

void RollerDriver::setStallProtection(bool on) {
  // hpp:431 - setStallProtection(uint8_t en).
  if (online_) roller_.setStallProtection(on ? 1 : 0);
}

void RollerDriver::setPosTarget(int32_t pos_counts) {
  // hpp:158 - setPos(int32_t). Raw counts.
  if (online_) roller_.setPos(pos_counts);
}

void RollerDriver::setSpeedTarget(int32_t speed_counts) {
  // hpp:92 - setSpeed(int32_t). Raw counts (0.01 RPM/count).
  if (online_) roller_.setSpeed(speed_counts);
}

void RollerDriver::setCurrentTarget(int32_t current_counts) {
  // hpp:224 - setCurrent(int32_t). Raw counts (0.01 mA/count).
  if (online_) roller_.setCurrent(current_counts);
}

void RollerDriver::setPosMaxCurrent(int32_t mA) {
  // hpp:176 - setPosMaxCurrent(int32_t). Raw mA, no scaling.
  if (online_) roller_.setPosMaxCurrent(mA);
}

void RollerDriver::setPosPID(uint32_t p, uint32_t i, uint32_t d) {
  // hpp:206 - setPosPID(uint32_t, uint32_t, uint32_t).
  if (online_) roller_.setPosPID(p, i, d);
}

void RollerDriver::getPosPID(uint32_t& p, uint32_t& i, uint32_t& d) {
  // hpp:527 - getPosPID(uint32_t*, uint32_t*, uint32_t*).
  p = i = d = 0;
  if (online_) roller_.getPosPID(&p, &i, &d);
}

int32_t RollerDriver::getVinRaw() {
  // hpp:733 - getVin(). 0.01 V/count.
  return online_ ? roller_.getVin() : 0;
}

int32_t RollerDriver::getTempC() {
  // hpp:752 - getTemp(). Degrees Celsius (signed integer).
  return online_ ? roller_.getTemp() : 0;
}

int32_t RollerDriver::getCurrentReadbackRaw() {
  // hpp:695 - getCurrentReadback(). 0.01 mA/count.
  return online_ ? roller_.getCurrentReadback() : 0;
}

uint8_t RollerDriver::getFirmwareVersion() {
  // hpp:981 - getFirmwareVersion().
  return online_ ? roller_.getFirmwareVersion() : 0;
}

void RollerDriver::serviceHealth() {
  uint32_t now = millis();

  // Light ACK probe at fixed cadence; trips offline at threshold.
  if (online_) {
    if (now - last_probe_ms_ >= I2C_PROBE_PERIOD_MS) {
      last_probe_ms_ = now;
      if (ackProbe_()) {
        consecutive_fails_ = 0;
      } else {
        consecutive_fails_++;
        if (consecutive_fails_ >= I2C_FAIL_THRESHOLD) {
          Serial.println("\nI2C FAULT: roller stopped acking. Driver OFFLINE.");
          online_ = false;
          next_recovery_ms_ = now + I2C_RECOVERY_PERIOD_MS;
        }
      }
    }
    return;
  }

  // Offline: try one Wire reset + handshake every recovery period.
  if (now < next_recovery_ms_) return;
  next_recovery_ms_ = now + I2C_RECOVERY_PERIOD_MS;
  // Non-blocking reset: end, brief delay, reopen, re-apply timeout, pet WDT,
  // attempt bounded handshake. Returns either way.
  wire_->end();
  delay(100);
  wire_->begin(sda_pin_, scl_pin_, freq_hz_);
  wire_->setTimeOut(WIRE_TIMEOUT_MS);
  Watchdog::pet();
  if (handshake_()) {
    // Library begin() reconfigures Wire and resets timeout - re-apply ours.
    wire_->setTimeOut(WIRE_TIMEOUT_MS);
    Serial.println("\nI2C RECOVERED: driver back online.");
    consecutive_fails_ = 0;
  }
}

// ===========================================================================
// SerialCLI
// Reusable line-based serial command registry. Reads lines from Serial,
// trims, splits into command + args, dispatches to a registered handler.
// Built-in 'help' and '?' enumerate registered commands.
// ===========================================================================
class SerialCLI {
 public:
  // Plain C function pointer. Handler receives the args portion (everything
  // after the first whitespace), or "" if no args.
  using Handler = void(*)(const char* args);

  void begin(uint32_t baud = 115200);
  // name and help_text must point to literals or otherwise stable storage.
  void registerCommand(const char* name, Handler handler, const char* help_text);
  // Pump from main loop().
  void loop();
  // Prints "> ".
  void printPrompt();

 private:
  void handleLine_(char* line);
  void splitFirstToken_(char* line, char*& cmd_out, char*& args_out);
  void printHelp_();

  // Buffer matches prior monolith (96 chars). Long enough for "pospid p i d".
  static constexpr size_t kLineCap = 96;
  static constexpr size_t kMaxCmds = 24;

  struct Entry {
    const char* name;
    Handler     handler;
    const char* help_text;
  };

  Entry  cmds_[kMaxCmds];
  size_t cmd_count_ = 0;

  char   line_buf_[kLineCap];
  size_t line_len_ = 0;
};

void SerialCLI::begin(uint32_t baud) {
  Serial.begin(baud);
  // Make Serial.print non-blocking when host disconnects.
  Serial.setTxTimeoutMs(0);
}

void SerialCLI::registerCommand(const char* name, Handler handler, const char* help_text) {
  if (cmd_count_ >= kMaxCmds) return;  // silently drop overflow; caller bug
  cmds_[cmd_count_++] = Entry{ name, handler, help_text };
}

void SerialCLI::printPrompt() {
  Serial.print("> ");
}

void SerialCLI::printHelp_() {
  Serial.println("Commands:");
  for (size_t i = 0; i < cmd_count_; i++) {
    const Entry& e = cmds_[i];
    // Two-column-ish: pad name to 12 chars so help_text aligns.
    Serial.printf("  %-12s %s\n", e.name, e.help_text ? e.help_text : "");
  }
  Serial.println("  help | ?     this help");
}

void SerialCLI::splitFirstToken_(char* line, char*& cmd_out, char*& args_out) {
  // Skip leading whitespace.
  while (*line == ' ' || *line == '\t') line++;
  cmd_out = line;
  args_out = line;
  if (!*line) { args_out = line; return; }
  // Walk to first whitespace - terminates the command token.
  while (*line && *line != ' ' && *line != '\t') line++;
  if (*line) {
    *line++ = '\0';
    while (*line == ' ' || *line == '\t') line++;
  }
  args_out = line;
}

void SerialCLI::handleLine_(char* line) {
  // Skip wholly-empty lines (just Enter).
  char* trim = line;
  while (*trim == ' ' || *trim == '\t') trim++;
  if (*trim == '\0') { printPrompt(); return; }

  char* cmd  = nullptr;
  char* args = nullptr;
  splitFirstToken_(line, cmd, args);
  if (!cmd || !*cmd) { printPrompt(); return; }

  // Built-ins first so user can't accidentally override.
  if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
    printHelp_();
    printPrompt();
    return;
  }

  for (size_t i = 0; i < cmd_count_; i++) {
    if (!strcmp(cmd, cmds_[i].name)) {
      cmds_[i].handler(args ? args : "");
      printPrompt();
      return;
    }
  }

  Serial.printf("unknown: %s — try 'help'\n", cmd);
  printPrompt();
}

void SerialCLI::loop() {
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c < 0) break;
    if (c == '\n' || c == '\r') {
      if (line_len_ == 0) continue;        // ignore bare CR after LF or vice versa
      line_buf_[line_len_] = '\0';
      handleLine_(line_buf_);
      line_len_ = 0;
    } else if (line_len_ < kLineCap - 1) {
      line_buf_[line_len_++] = (char)c;
    }
    // Overflow chars dropped silently - exploration sketch, not safety-critical.
  }
}

// ---------- Module instances ----------
static RollerDriver driver_;
static SerialCLI    cli_;

// ---------- Unit helpers ----------
static inline int32_t deg_to_counts(float deg) {
  return (int32_t)lroundf(deg * COUNTS_PER_DEG);
}
static inline float counts_to_deg(int32_t counts) {
  return (float)counts / COUNTS_PER_DEG;
}

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

// ---------- Args parsing ----------
// Returns count. Modifies the (mutable) args buffer in place to NUL-terminate.
static int splitArgs(char* args, char* out_tokens[], int max_tokens) {
  int n = 0;
  char* p = args;
  while (*p && n < max_tokens) {
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) break;
    out_tokens[n++] = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    if (*p) { *p = '\0'; p++; }
  }
  return n;
}

// ---------- Command handlers ----------
static void cmd_key1(const char* /*args*/) {
  int32_t target = deg_to_counts(0.0f);
  driver_.setPosTarget(target);
  Serial.printf("KEY 1 -> target=0 deg (counts=%d)\n", (int)target);
}

static void cmd_key2(const char* /*args*/) {
  int32_t cur    = driver_.getEncoder();
  int32_t target = cur + deg_to_counts(360.0f);
  driver_.setPosTarget(target);
  Serial.printf("KEY 2 -> +360 deg from current (target_counts=%d)\n", (int)target);
}

static void cmd_key3(const char* /*args*/) {
  int32_t target = deg_to_counts(180.0f);
  driver_.setPosTarget(target);
  Serial.printf("KEY 3 -> target=180 deg (counts=%d)\n", (int)target);
}

static void cmd_status(const char* /*args*/) {
  if (!driver_.isOnline()) {
    Serial.println("driver OFFLINE — only CLI works. Try 'reboot'.");
    return;
  }
  int32_t enc      = driver_.getEncoder();
  uint8_t m        = driver_.getMotorMode();
  int32_t vin_raw  = driver_.getVinRaw();
  int32_t temp_c   = driver_.getTempC();
  int32_t cur_raw  = driver_.getCurrentReadbackRaw();
  uint8_t out      = driver_.getOutputStatus();
  Serial.printf("angle    : %.2f deg (%d counts)\n", counts_to_deg(enc), (int)enc);
  Serial.printf("mode     : %u (%s)\n", m, modeName(m));
  Serial.printf("output   : %s\n", out ? "ON" : "OFF");
  Serial.printf("vin      : %.2f V\n", vin_raw / 100.0f);
  Serial.printf("temp     : %d C\n", (int)temp_c);
  Serial.printf("current  : %.2f mA (readback)\n", cur_raw / 100.0f);
}

static void cmd_stop(const char* /*args*/) {
  driver_.setOutput(false);
  Serial.println("output=OFF (motor limp)");
}

static void cmd_start(const char* /*args*/) {
  driver_.setOutput(true);
  Serial.println("output=ON");
}

static void cmd_mode(const char* args) {
  if (!*args) { Serial.println("usage: mode <n>  (1|2|3|5)"); return; }
  int m = atoi(args);
  if (m == 1 || m == 2 || m == 3 || m == 5) {
    driver_.setMode((uint8_t)m);
    delay(20);
    Serial.printf("mode=%d (%s)  readback=%u\n",
      m, modeName(m), driver_.getMotorMode());
  } else {
    Serial.println("invalid mode. valid: 1 SPEED, 2 POSITION, 3 CURRENT, 5 POS_SPEED");
  }
}

static void cmd_current(const char* args) {
  if (!*args) { Serial.println("usage: current <mA>"); return; }
  int32_t mA = atol(args);
  driver_.setMode(ROLLER_MODE_CURRENT);
  delay(10);
  driver_.setCurrentTarget(mA * CURRENT_COUNTS_PER_MA);
  Serial.printf("MODE_CURRENT  current=%d mA (counts=%d)\n",
    (int)mA, (int)(mA * CURRENT_COUNTS_PER_MA));
}

static void cmd_speed(const char* args) {
  if (!*args) { Serial.println("usage: speed <dps>"); return; }
  int32_t dps = atol(args);
  int32_t counts = (int32_t)lroundf((float)dps * SPEED_COUNTS_PER_DPS);
  driver_.setSpeedTarget(counts);
  Serial.printf("speed=%d dps (counts=%d) — target in MODE_SPEED, max in MODE_POSITION\n",
    (int)dps, (int)counts);
}

static void cmd_maxc(const char* args) {
  if (!*args) { Serial.println("usage: maxc <mA>"); return; }
  int32_t mA = atol(args);
  driver_.setPosMaxCurrent(mA);
  Serial.printf("pos_max_current=%d mA\n", (int)mA);
}

static void cmd_pospid(const char* args) {
  // Mutable copy so splitArgs can NUL-terminate in place.
  char buf[64];
  strncpy(buf, args, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  char* tokens[3];
  int n = splitArgs(buf, tokens, 3);
  if (n < 3) { Serial.println("usage: pospid <p> <i> <d>"); return; }
  uint32_t p = (uint32_t)strtoul(tokens[0], nullptr, 10);
  uint32_t i = (uint32_t)strtoul(tokens[1], nullptr, 10);
  uint32_t d = (uint32_t)strtoul(tokens[2], nullptr, 10);
  driver_.setPosPID(p, i, d);
  delay(20);
  uint32_t rp = 0, ri = 0, rd = 0;
  driver_.getPosPID(rp, ri, rd);
  Serial.printf("pospid set p=%u i=%u d=%u  readback p=%u i=%u d=%u\n",
    p, i, d, rp, ri, rd);
}

static void cmd_pid(const char* /*args*/) {
  uint32_t p = 0, i = 0, d = 0;
  driver_.getPosPID(p, i, d);
  Serial.printf("pos_pid p=%u i=%u d=%u\n", p, i, d);
}

static void cmd_power(const char* /*args*/) {
  if (!driver_.isOnline()) { Serial.println("driver OFFLINE"); return; }
  float vin_v  = driver_.getVinRaw() / 100.0f;
  float cur_mA = driver_.getCurrentReadbackRaw() / 100.0f;
  Serial.printf("vin      : %.2f V\n", vin_v);
  Serial.printf("temp     : %d C\n", (int)driver_.getTempC());
  Serial.printf("current  : %.2f mA\n", cur_mA);
  Serial.printf("fw_ver   : %u\n", driver_.getFirmwareVersion());
  if (vin_v >= 10.0f) Serial.println("vin >= 10 V -> OK");
  else                Serial.println("vin <  10 V -> LOW (motor on bus power)");
}

// Brief +/-250 mA blip to confirm power and see motion. Saves and restores
// prior mode and output so it can be invoked from any mode safely.
static void cmd_wiggle(const char* /*args*/) {
  if (!driver_.isOnline()) { Serial.println("driver OFFLINE"); return; }
  uint8_t prev_mode   = driver_.getMotorMode();
  uint8_t prev_output = driver_.getOutputStatus();
  Serial.printf("wiggle: prev mode=%u output=%u\n", prev_mode, prev_output);
  driver_.setMode(ROLLER_MODE_CURRENT);
  driver_.setOutput(true);
  Watchdog::pet();
  driver_.setCurrentTarget(+250 * CURRENT_COUNTS_PER_MA); delay(300);
  Watchdog::pet();
  driver_.setCurrentTarget(-250 * CURRENT_COUNTS_PER_MA); delay(300);
  Watchdog::pet();
  driver_.setCurrentTarget(+250 * CURRENT_COUNTS_PER_MA); delay(300);
  Watchdog::pet();
  driver_.setCurrentTarget(0);
  // Restore. Cast preserves mode 5 round-trip (not in enum).
  driver_.setMode(prev_mode);
  driver_.setOutput(prev_output != 0);
  Serial.printf("wiggle done. restored mode=%u output=%u\n",
    prev_mode, prev_output);
}

static void cmd_reboot(const char* /*args*/) {
  Serial.println("rebooting...");
  Serial.flush();
  delay(50);
  Watchdog::rebootChip();
}

// ---------- Setup / Loop ----------
void setup() {
  // WDT FIRST - last-ditch safety against any later wedge.
  Watchdog::arm(TASK_WDT_TIMEOUT_S);

  cli_.begin(115200);
  delay(500);
  Watchdog::pet();

  Serial.println();
  Serial.println("==========================================================");
  Serial.printf (" Witness · 09_motor_modes_explorer  sketch=%s\n", SKETCH_VERSION);
  Serial.println("==========================================================");

  Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ_HZ);
  Wire.setTimeOut(WIRE_TIMEOUT_MS);
  Watchdog::pet();

  bool ok = driver_.begin(Wire, ROLLER_ADDR, I2C_SDA, I2C_SCL, I2C_FREQ_HZ);
  // Library begin() reconfigures Wire and resets timeout - re-apply ours.
  Wire.setTimeOut(WIRE_TIMEOUT_MS);
  Watchdog::pet();

  if (!ok) {
    Serial.println("WARNING: roller not responding on I2C.");
    Serial.println("CLI active. Run 'reboot' or check hardware.");
  } else {
    Serial.printf("roller online at 0x%02X. fw_ver=%u\n",
      ROLLER_ADDR, driver_.getFirmwareVersion());
    // Default startup posture: stall protection off (we will be pushing the
    // motor by hand), POSITION mode, output enabled, holding at current angle.
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
      vin_v, counts_to_deg(enc), (int)enc,
      driver_.getMotorMode(), modeName(driver_.getMotorMode()));
  }

  // Register motor-test commands. Single-key 1/2/3 are normal commands now.
  cli_.registerCommand("1",       cmd_key1,    "move to absolute 0 deg and hold");
  cli_.registerCommand("2",       cmd_key2,    "+360 deg from current position");
  cli_.registerCommand("3",       cmd_key3,    "move to absolute 180 deg");
  cli_.registerCommand("status",  cmd_status,  "angle, mode, output, vin, temp, current");
  cli_.registerCommand("stop",    cmd_stop,    "setOutput(false), motor goes limp");
  cli_.registerCommand("start",   cmd_start,   "setOutput(true)");
  cli_.registerCommand("mode",    cmd_mode,    "<n> 1=SPEED 2=POSITION 3=CURRENT 5=POS_SPEED");
  cli_.registerCommand("current", cmd_current, "<mA> switch to MODE_CURRENT and set torque");
  cli_.registerCommand("speed",   cmd_speed,   "<dps> speed target / max speed (mode-dependent)");
  cli_.registerCommand("maxc",    cmd_maxc,    "<mA> set MODE_POSITION max current");
  cli_.registerCommand("pospid",  cmd_pospid,  "<p> <i> <d> set position PID gains (uint32)");
  cli_.registerCommand("pid",     cmd_pid,     "read current position PID");
  cli_.registerCommand("power",   cmd_power,   "vin / temp / current_readback / fw_version");
  cli_.registerCommand("wiggle",  cmd_wiggle,  "+250 / -250 / +250 mA proof-of-life");
  cli_.registerCommand("reboot",  cmd_reboot,  "ESP.restart()");
  cli_.registerCommand("reset",   cmd_reboot,  "alias for reboot");

  Serial.println("type 'help' for commands");
  cli_.printPrompt();
}

void loop() {
  Watchdog::pet();
  driver_.serviceHealth();
  cli_.loop();
  delay(1);
}
