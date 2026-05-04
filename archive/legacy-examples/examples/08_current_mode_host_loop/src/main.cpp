// Witness · 08_current_mode_host_loop · Milestone 3.1 (directional LUTs).
//
// M1 scaffold preserved: MODE_CURRENT, 200 Hz tick, encoder read, zero-current
// default. M2 added velocity PI. M3 added a per-Witness 720-bin cogging LUT
// (0.5 deg resolution, int16_t hundredths-of-mA storage), a tick-driven
// calibration FSM, NVS persistence via Preferences, break-free kick on enable,
// and `ff_mA` / `pi_out_mA` in telemetry so we can see FF vs PI effort.
// M3.1 splits the single averaged LUT into two directional LUTs (fwd/rev) to
// kill the cogging hysteresis that caused "beautiful-moving, stops-and-restarts"
// behavior. Runtime picks fwd/rev by sign of target_dps with a 0.5 dps deadband.

#include <Arduino.h>
#include <Wire.h>
#include <esp_timer.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include "unit_rolleri2c.hpp"

// ---------- Robustness: task WDT + I2C health ----------
// Safety net against a library call that busy-waits on a dead I2C peripheral.
// If setup() or loop() fail to kick the WDT within this window, the chip
// auto-reboots. Chosen at 10 s: covers worst-case NVS load + handshake retries
// with margin. Cal FSM resets WDT from loop() every cycle so 8-min sweeps are
// unaffected.
static constexpr uint32_t TASK_WDT_TIMEOUT_S = 10;

// Per-transaction I2C timeout. Default core value is 50 ms; we set it
// explicitly so a dead slave holding SDA low can't wedge endTransmission().
static constexpr uint16_t WIRE_TIMEOUT_MS = 50;

// Initial handshake retry budget.
static constexpr int      HANDSHAKE_RETRIES        = 5;
static constexpr uint32_t HANDSHAKE_RETRY_GAP_MS   = 200;

// Runtime I2C health tracker. Consecutive-fail threshold → offline.
// 5 fails × 5 ms tick period ≈ 25 ms of silence.
static constexpr int      I2C_FAIL_THRESHOLD       = 5;

// Recovery state machine cadence.
static constexpr uint32_t I2C_RECOVERY_FIRST_MS    = 500;
static constexpr uint32_t I2C_RECOVERY_BACKOFF_MS  = 2000;
static constexpr int      I2C_RECOVERY_RETRIES     = 3;
static constexpr uint32_t I2C_RECOVERY_GAP_MS      = 100;

// ---------- Hardware (identical to 07/M1/M2) ----------
#define I2C_SDA 8
#define I2C_SCL 9
#define ROLLER_ADDR 0x64
#define STEPS_PER_REV 36000

// 200 Hz = 5000 us period.
#define TICK_PERIOD_US 5000

#define SKETCH_VERSION "0.3.1-M3-dir"

// LUT storage format version. v1 = single averaged LUT (deprecated, old builds).
// v2 = directional LUTs (fwd + rev) introduced in M3.1. Bumped when LutMeta or
// blob scheme changes incompatibly.
static constexpr uint8_t LUT_FORMAT_VERSION = 2;

// Directional threshold for runtime LUT selection. |target| below this uses no
// FF (motor is supposed to be holding, PI + integrator handle it). Keep above
// any numerical noise from BREATH-like primitives that park at 0.0 dps.
static constexpr float LUT_DIR_DEADBAND_DPS = 0.5f;

// ---------- M2 tuning constants ----------
// LPF time constant: 50 ms trades phase margin for noise rejection at 200 Hz.
static constexpr float VEL_LPF_TAU_MS = 50.0f;
static constexpr float CTRL_DT_S      = (float)TICK_PERIOD_US / 1e6f;
// Precomputed alpha = dt/(tau+dt) ~= 0.005/0.055 ~= 0.0909.
static constexpr float VEL_LPF_ALPHA  = CTRL_DT_S / (VEL_LPF_TAU_MS * 1e-3f + CTRL_DT_S);

// Motor spec (processed/01_shop_product_page.md): 0.5 A continuous, 1.0 A
// short-term peak phase current. 400 mA default is under continuous rating.
static constexpr int32_t CURRENT_LIMIT_MA_DEFAULT = 400;

// Starting gains seeded in M2. Preserved — don't refit without bench data.
static constexpr float KP_DEFAULT = 2.0f;
static constexpr float KI_DEFAULT = 10.0f;

// Safety thresholds.
static constexpr float    RUNAWAY_ERR_DPS      = 200.0f;
static constexpr uint32_t RUNAWAY_MS           = 500;
static constexpr uint32_t SAT_MS               = 2000;

// ---------- M3 LUT constants ----------
// 720 bins covers 0..360 deg at 0.5 deg resolution. 7-pole-pair motor has 14
// cogging cusps/rev; 720/14 = ~51 samples per cusp. Plenty of room for any
// realistic spatial harmonic content.
static constexpr int     LUT_BINS            = 720;
// Blob size must match exactly for NVS load path to accept.
static constexpr size_t  LUT_BLOB_BYTES      = LUT_BINS * sizeof(int16_t);
// Sentinel in per-pass buffers: bin never sampled on that pass.
static constexpr int16_t LUT_SENTINEL        = INT16_MIN;
// LUT stores mA*100 (int16 range ±327.68 A — absurd headroom, trivial precision).
static constexpr float   LUT_SCALE           = 100.0f;

// ---------- M3 calibration constants ----------
// 12 dps is the spec target sweep speed. Widened gate 3..30 dps captures bins
// where gravity is fighting (motor slow) or helping (motor fast) — uncovered
// bins at the 78%-coverage mark had ff_mA=0 and caused surges.
static constexpr float    CAL_TARGET_DPS     = 12.0f;
static constexpr float    CAL_VEL_MIN_DPS    = 3.0f;
static constexpr float    CAL_VEL_MAX_DPS    = 30.0f;
// EMA alpha per spec — later samples refine earlier ones.
static constexpr float    CAL_EMA_ALPHA      = 0.3f;
// Progress print cadence: every 5 deg traversed, logged by bin count (10 bins).
static constexpr int      CAL_PROGRESS_BINS  = 10;
// Total budget 8 min across up to 3 fwd + 3 rev passes. Pass completes on one
// full rev of angular travel (36000 encoder steps), not on all bins sampled.
// Early exit when coverage >= 95%.
static constexpr uint32_t CAL_TIMEOUT_MS     = 8UL * 60UL * 1000UL;
static constexpr int      CAL_MAX_PASSES_PER_DIR = 3;
static constexpr int      CAL_EARLY_EXIT_COVERAGE_PCT = 95;
// Motor settle before first motion.
static constexpr uint32_t CAL_SETTLE_MS      = 500;
// Ramp nudge current used before the PI is engaged — nudges the pointer out of
// deep stiction so the velocity PI has a measurement to work with.
static constexpr int32_t  CAL_NUDGE_START_MA = 50;
static constexpr int32_t  CAL_NUDGE_MAX_MA   = 300;
static constexpr uint32_t CAL_NUDGE_STEP_MS  = 200;
static constexpr int32_t  CAL_NUDGE_STEP_MA  = 25;
// Saturation abort: spec says |cmd| at climit for >2 s continuous.
static constexpr uint32_t CAL_SAT_ABORT_MS   = 2000;

// ---------- M3 break-free kick ----------
static constexpr int32_t  KICK_CURRENT_MA    = 600;
static constexpr uint32_t KICK_DURATION_MS   = 200;
// At-rest detector window: vel_filt_dps below 2.0 for this long = "from rest".
static constexpr float    KICK_REST_DPS      = 2.0f;
static constexpr uint32_t KICK_REST_MS       = 500;

// ---------- Auto-kick (stuck-during-motion) ----------
// Fires mid-motion when the motor gets pinned by cogging/stiction. Shares the
// break-free kick mechanism; shared cooldown prevents stacking.
static constexpr float    AUTO_KICK_VEL_THRESHOLD_DPS = 2.0f;
static constexpr float    AUTO_KICK_ERR_THRESHOLD_DPS = 5.0f;
static constexpr uint32_t AUTO_KICK_STUCK_MS          = 250;
static constexpr uint32_t AUTO_KICK_COOLDOWN_MS       = 800;
static constexpr int32_t  AUTO_KICK_LIMIT_MA          = 600;
static constexpr uint32_t AUTO_KICK_DURATION_MS       = 200;

// ---------- M3 NVS keys ----------
// NVS key max 15 chars (NVS_KEY_NAME_MAX_SIZE=16 includes null). Longest slot
// name we allow is 10 chars ("witness_XX"); "lut/" + 10 = 14, "m/" + 10 = 12.
static constexpr const char* NVS_NAMESPACE   = "witness";
static constexpr const char* NVS_KEY_SLOT    = "lut_slot";
static constexpr const char* NVS_SLOT_DEFAULT = "witness_02";
static constexpr size_t      NVS_SLOT_MAX    = 10;

// ---------- Driver ----------
// `online_` guards every public method. When offline:
//   - setCurrent / setOutput / setMode / setStall* become no-ops
//   - readEncoder returns the last good value (stale, but safe)
//   - telemetry returns 0 sentinels
// The library itself still blocks if called while the bus is wedged, so the
// guard is how we avoid calling it. Health tracking (below) flips online_
// when the bus stops responding.
class RollerDriver {
public:
  // Bounded-retry handshake. Uses a direct Wire ACK probe instead of
  // roller_.begin() on retries so we don't reconfigure Wire pins 5 times.
  // Returns true when the slave ACKs within HANDSHAKE_RETRIES.
  bool begin() {
    _wire_begin_called_once = true;
    // First attempt: full library begin() — it calls Wire.begin() internally.
    if (roller_.begin(&Wire, ROLLER_ADDR, I2C_SDA, I2C_SCL, 400000)) {
      online_ = true;
      return true;
    }
    // Library begin() already configured Wire. Subsequent probes can be bare.
    for (int i = 1; i < HANDSHAKE_RETRIES; i++) {
      delay(HANDSHAKE_RETRY_GAP_MS);
      Wire.beginTransmission(ROLLER_ADDR);
      if (Wire.endTransmission() == 0) {
        online_ = true;
        return true;
      }
    }
    online_ = false;
    return false;
  }

  bool online() const { return online_; }
  void setOnline(bool v) { online_ = v; }

  // Bare ACK probe — fast, no library call. Used by recovery and by the
  // runtime health tracker.
  static bool ackProbe() {
    Wire.beginTransmission(ROLLER_ADDR);
    return (Wire.endTransmission() == 0);
  }

  int32_t readEncoder() {
    if (!online_) return last_enc_;
    int32_t v = roller_.getPosReadback();
    last_enc_ = v;
    return v;
  }
  void    setCurrent(int32_t mA) {
    if (!online_) return;
    roller_.setCurrent(mA * 100);
  }
  void    setStall0() { if (!online_) return; roller_.setStallProtection(0); }
  void    setStall1() { if (!online_) return; roller_.setStallProtection(1); }
  void    setMode(roller_mode_t m) { if (!online_) return; roller_.setMode(m); }
  void    setOutput(bool on) { if (!online_) return; roller_.setOutput(on ? 1 : 0); }
  uint8_t getMotorMode() { return online_ ? roller_.getMotorMode() : 0; }
  uint8_t getFirmwareVersion() { return online_ ? roller_.getFirmwareVersion() : 0; }
  // Telemetry passthroughs — confirmed in M1/M2 log that 0.01 scaling matches.
  int32_t getVinRaw()             { return online_ ? roller_.getVin() : 0; }
  int32_t getTempC()              { return online_ ? roller_.getTemp() : 0; }
  int32_t getCurrentReadbackRaw() { return online_ ? roller_.getCurrentReadback() : 0; }
private:
  UnitRollerI2C roller_;
  bool          online_ = false;
  int32_t       last_enc_ = 0;
  bool          _wire_begin_called_once = false;
};

// ---------- Shared state ----------
struct ControlState {
  int32_t  enc_steps;
  int32_t  enc_prev;
  float    ang_deg;
  float    vel_raw_dps;
  float    vel_filt_dps;
  float    target_dps;
  float    err_dps;
  float    pi_integral_mA;
  float    pi_out_mA;
  float    ff_mA;
  int32_t  cmd_mA;
  int32_t  current_limit_mA;
  int32_t  current_limit_user_mA;
  float    kp;
  float    ki;
  bool     enabled;
  uint32_t runaway_accum_ms;
  uint32_t sat_accum_ms;
  uint32_t tick_count;
  uint32_t last_hz_sample_ms;
  uint32_t ticks_at_sample;
  float    measured_hz;
  uint32_t boot_ms;
  bool     first_tick;
  // Kick state (shared by break-free + auto-kick).
  bool     kick_active;
  uint32_t kick_start_ms;
  uint32_t kick_duration_ms;
  uint32_t last_kick_end_ms;
  uint32_t rest_accum_ms;
  // Auto-kick state.
  bool     auto_kick_enabled;
  uint32_t stuck_accum_ms;
  uint32_t auto_kicks_fired;
  // Warning one-shot for "no LUT, feedforward disabled".
  bool     lut_missing_warned;
  // Power-on wiggle state. `last_vin_ok` tracks the debounced rising-edge
  // detector driven from loop() at ~1 Hz. Hysteresis: rising edge at >=10 V,
  // falling edge at <6 V. `wiggle_running` gates the tick() disabled-branch
  // setCurrent(0) so the blocking wiggle sequence in loop() can drive the
  // motor without being clobbered every 5 ms.
  bool     last_vin_ok;
  bool     wiggle_running;
  uint32_t last_vin_poll_ms;
};

static ControlState state_;
static RollerDriver driver_;
static volatile bool tick_pending_ = false;
static esp_timer_handle_t tick_timer_;

// ---------- I2C health / recovery state (driven from loop(), never ISR) ----------
enum I2cRecoveryState : uint8_t {
  I2C_HEALTHY = 0,     // online, no recovery in flight
  I2C_FAULT_WAITING,   // offline, waiting for next recovery attempt
};
static I2cRecoveryState i2c_recovery_state_ = I2C_HEALTHY;
static volatile int     i2c_consecutive_fails_ = 0;
static volatile bool    i2c_fault_flag_ = false;  // set from health probe, read in loop()
static uint32_t         i2c_next_recovery_ms_ = 0;
static uint32_t         i2c_last_probe_ms_    = 0;
// Health probe cadence: every 5 ms from loop() when control is enabled. Light
// (2-byte ACK transaction with 50 ms timeout worst case), fast detection.
static constexpr uint32_t I2C_PROBE_PERIOD_MS = 5;

static inline float stepsToDeg(int32_t s) {
  return s * 360.0f / (float)STEPS_PER_REV;
}

static inline int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static inline float clamp_f(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// Wrap degrees to [0, 360). Encoder may return negative pos or >360.
static inline float wrap_deg_360(float d) {
  float r = fmodf(d, 360.0f);
  if (r < 0.0f) r += 360.0f;
  return r;
}

// ---------- CoggingLUT ----------
class CoggingLUT {
public:
  CoggingLUT() { clear(); }

  void clear() {
    for (int i = 0; i < LUT_BINS; i++) bins_[i] = 0;
    loaded_ = false;
  }

  bool loaded() const { return loaded_; }
  void markLoaded(bool v) { loaded_ = v; }

  int16_t get(int bin) const { return bins_[bin]; }
  void    set(int bin, int16_t v) { bins_[bin] = v; }

  // Wraps deg to [0,360) and returns bin in [0, LUT_BINS).
  static int bin_for_angle_deg(float deg) {
    float w = wrap_deg_360(deg);
    int b = (int)(w * 2.0f);
    if (b < 0) b = 0;
    if (b >= LUT_BINS) b = LUT_BINS - 1;
    return b;
  }

  // Linear interpolation between the two bracketing bins. Returns mA.
  float lookup_linear(float deg) const {
    float w = wrap_deg_360(deg);
    float idx = w * 2.0f;
    int b0 = (int)idx;
    if (b0 < 0) b0 = 0;
    if (b0 >= LUT_BINS) b0 = LUT_BINS - 1;
    int b1 = (b0 + 1) % LUT_BINS;
    float frac = idx - (float)b0;
    float v0 = (float)bins_[b0] / LUT_SCALE;
    float v1 = (float)bins_[b1] / LUT_SCALE;
    return v0 + (v1 - v0) * frac;
  }

  // Pointer to raw storage for NVS blob I/O.
  const int16_t* data() const { return bins_; }
  int16_t*       data()       { return bins_; }

  // Stats for `lut` CLI — scan once, no allocation.
  void stats(float& out_min_mA, float& out_max_mA,
             float& out_mean_mA, float& out_abs_max_mA,
             int& out_abs_max_bin) const {
    int32_t sum = 0;
    int16_t mn = INT16_MAX, mx = INT16_MIN;
    int16_t abs_mx = 0;
    int abs_mx_bin = 0;
    for (int i = 0; i < LUT_BINS; i++) {
      int16_t v = bins_[i];
      sum += v;
      if (v < mn) mn = v;
      if (v > mx) mx = v;
      int16_t av = (v < 0) ? (int16_t)(-v) : v;
      if (av > abs_mx) { abs_mx = av; abs_mx_bin = i; }
    }
    out_min_mA      = (float)mn / LUT_SCALE;
    out_max_mA      = (float)mx / LUT_SCALE;
    out_mean_mA     = ((float)sum / (float)LUT_BINS) / LUT_SCALE;
    out_abs_max_mA  = (float)abs_mx / LUT_SCALE;
    out_abs_max_bin = abs_mx_bin;
  }

private:
  int16_t bins_[LUT_BINS];
  bool    loaded_ = false;
};

// Two directional LUTs. Runtime picks one by sign of target_dps.
// Union state: "valid" = both loaded. See lutReady().
static CoggingLUT lut_fwd_;
static CoggingLUT lut_rev_;

static inline bool lutReady() { return lut_fwd_.loaded() && lut_rev_.loaded(); }

// ---------- VelocityPI ----------
class VelocityPI {
public:
  void setGains(float kp, float ki) { kp_ = kp; ki_ = ki; }
  void setLimit(int32_t mA) { out_limit_mA_ = mA; }
  int32_t getLimit() const { return out_limit_mA_; }
  void reset() { integral_mA_ = 0.0f; last_out_mA_ = 0.0f; }
  float integral() const { return integral_mA_; }
  float lastOut() const { return last_out_mA_; }

  // Returns commanded current in mA, clamped to ±out_limit_mA_.
  int32_t update(float target_dps, float measured_dps, float dt_s) {
    float err = target_dps - measured_dps;
    float new_integral = integral_mA_ + ki_ * err * dt_s;
    float int_bound = (float)out_limit_mA_;
    new_integral = clamp_f(new_integral, -int_bound, int_bound);
    integral_mA_ = new_integral;
    float out = kp_ * err + integral_mA_;
    last_out_mA_ = out;
    int32_t out_i = (int32_t)out;
    return clamp_i32(out_i, -out_limit_mA_, out_limit_mA_);
  }

private:
  float   kp_            = KP_DEFAULT;
  float   ki_            = KI_DEFAULT;
  float   integral_mA_   = 0.0f;
  float   last_out_mA_   = 0.0f;
  int32_t out_limit_mA_  = CURRENT_LIMIT_MA_DEFAULT;
};

static VelocityPI pi_;

// ---------- LUT NVS metadata ----------
// Kept tiny so the short NVS key ("m/<slot>") fits the binary blob.
// Version byte is FIRST so an old v1 blob (no version field) that happens to
// match the new sizeof() gets discriminated by reading offset 0 as version.
// Any value != LUT_FORMAT_VERSION is rejected as outdated.
struct LutMeta {
  uint8_t  version;          // LUT_FORMAT_VERSION (v2 = directional LUTs)
  uint8_t  _pad[3];          // reserved, keep struct 4-byte aligned
  uint32_t ts_ms_at_write;   // millis() at save. Limitation: no RTC.
  int32_t  vin_raw;          // 0.01 V units at save
  uint32_t fw_version;       // sketch firmware tag as uint32_t hash placeholder
};

// ---------- Slot state ----------
// Active slot for LUT load/save. Mutable via `lut slot <name>`.
static char current_slot_[NVS_SLOT_MAX + 1] = "";

// ---------- Forward declarations ----------
static void stopControlAutoAbort(const char* reason);
static void abortCalibration(const char* reason);
static bool nvsLoadLuts(const char* slot, CoggingLUT& out_fwd, CoggingLUT& out_rev, LutMeta& out_meta);
static bool nvsSaveLuts(const char* slot, const CoggingLUT& fwd, const CoggingLUT& rev, const LutMeta& meta);
static bool nvsClearLuts(const char* slot);
static bool nvsSaveActiveSlot(const char* slot);
static void nvsLoadActiveSlot(char* out_slot, size_t maxlen);
static void i2cMarkFault();
static void i2cServiceRecovery();
static void i2cHealthProbe();
static void doReboot();

// ---------- Calibration FSM ----------
// Non-blocking, advanced from tick() at 200 Hz. `stop` flips state to ABORT.
// Per-bin data for two passes lives in int16 buffers sized 1440 bytes each.
// Sentinel LUT_SENTINEL = "never sampled in this pass".
enum CalState : uint8_t {
  CAL_IDLE = 0,
  CAL_PREFLIGHT,
  CAL_SETTLE,
  CAL_NUDGE,
  CAL_SWEEP_FWD,
  CAL_SWEEP_REV,
  CAL_FINALIZE,
  CAL_DONE,
  CAL_ABORT,
};

class Calibration {
public:
  void begin(const char* slot) {
    state_cal_      = CAL_PREFLIGHT;
    t_phase_ms_     = millis();
    t_start_ms_     = millis();
    slot_[0]        = '\0';
    strncpy(slot_, slot, NVS_SLOT_MAX);
    slot_[NVS_SLOT_MAX] = '\0';
    for (int i = 0; i < LUT_BINS; i++) {
      pass_fwd_[i] = LUT_SENTINEL;
      pass_rev_[i] = LUT_SENTINEL;
    }
    fwd_bins_sampled_  = 0;
    rev_bins_sampled_  = 0;
    last_progress_bin_ = -1;
    dir_              = +1;
    nudge_mA_         = CAL_NUDGE_START_MA;
    sat_accum_ms_     = 0;
    fwd_passes_done_  = 0;
    rev_passes_done_  = 0;
    enc_at_pass_start_ = 0;
    runaway_accum_ms_ = 0;
  }

  bool active() const { return state_cal_ != CAL_IDLE && state_cal_ != CAL_DONE && state_cal_ != CAL_ABORT; }
  CalState state() const { return state_cal_; }

  // Called from tick(). Returns true if cal consumed this tick's control
  // responsibility (i.e. tick() should NOT do its own PI/FF path).
  bool tick(float ang_deg, float vel_filt_dps, int32_t tick_ms) {
    if (!active()) return false;

    uint32_t now = millis();
    uint32_t in_phase = now - t_phase_ms_;
    uint32_t total    = now - t_start_ms_;

    // Global timeout across all passes (8 min budget). On expiry, jump to
    // finalize. Gap-fill interpolation covers whatever bins didn't get sampled.
    if (total > CAL_TIMEOUT_MS && state_cal_ < CAL_FINALIZE) {
      Serial.printf("cal partial coverage fwd=%d%% rev=%d%%. Timeout, finalizing with what we have.\n",
        coveragePctFwd(), coveragePctRev());
      transitionTo(CAL_FINALIZE);
      return false;
    }

    switch (state_cal_) {
      case CAL_PREFLIGHT: {
        // Handled by beginCalibrate() before entering FSM; move on.
        transitionTo(CAL_SETTLE);
        break;
      }
      case CAL_SETTLE: {
        driver_.setCurrent(0);
        if (in_phase >= CAL_SETTLE_MS) {
          Serial.println("cal: settle done, nudging pointer out of stiction");
          pi_.reset();
          transitionTo(CAL_NUDGE);
        }
        break;
      }
      case CAL_NUDGE: {
        // Open-loop current nudge until pointer moves; ramps if it doesn't.
        driver_.setCurrent(nudge_mA_);
        if (fabsf(vel_filt_dps) > 3.0f) {
          Serial.printf("cal pass 1/%d fwd engaging PI at target=+%.1f dps\n",
            CAL_MAX_PASSES_PER_DIR, CAL_TARGET_DPS);
          startPass(+1);
          transitionTo(CAL_SWEEP_FWD);
        } else if (in_phase >= CAL_NUDGE_STEP_MS) {
          nudge_mA_ += CAL_NUDGE_STEP_MA;
          if (nudge_mA_ > CAL_NUDGE_MAX_MA) {
            Serial.println("cal ERROR: pointer did not move under nudge ramp");
            transitionTo(CAL_ABORT);
            driver_.setCurrent(0);
            break;
          }
          t_phase_ms_ = now;
        }
        break;
      }
      case CAL_SWEEP_FWD: {
        if (runPiSweep(ang_deg, vel_filt_dps, pass_fwd_, fwd_bins_sampled_, "fwd")) {
          fwd_passes_done_++;
          int cov_fwd = coveragePctFwd();
          int cov_rev = coveragePctRev();
          Serial.printf("cal pass %d/%d fwd complete, cov fwd=%d%% rev=%d%%\n",
            fwd_passes_done_, CAL_MAX_PASSES_PER_DIR, cov_fwd, cov_rev);
          // Both directions must hit early-exit threshold to finalize early.
          if (cov_fwd >= CAL_EARLY_EXIT_COVERAGE_PCT &&
              cov_rev >= CAL_EARLY_EXIT_COVERAGE_PCT) {
            Serial.printf("cal: both directions >= %d%%, early exit to finalize\n",
              CAL_EARLY_EXIT_COVERAGE_PCT);
            transitionTo(CAL_FINALIZE);
          } else if (rev_passes_done_ >= CAL_MAX_PASSES_PER_DIR &&
                     fwd_passes_done_ >= CAL_MAX_PASSES_PER_DIR) {
            Serial.println("cal: all passes exhausted, finalizing");
            transitionTo(CAL_FINALIZE);
          } else {
            Serial.printf("cal pass %d/%d rev starting\n",
              rev_passes_done_ + 1, CAL_MAX_PASSES_PER_DIR);
            startPass(-1);
            transitionTo(CAL_SWEEP_REV);
          }
        }
        break;
      }
      case CAL_SWEEP_REV: {
        if (runPiSweep(ang_deg, vel_filt_dps, pass_rev_, rev_bins_sampled_, "rev")) {
          rev_passes_done_++;
          int cov_fwd = coveragePctFwd();
          int cov_rev = coveragePctRev();
          Serial.printf("cal pass %d/%d rev complete, cov fwd=%d%% rev=%d%%\n",
            rev_passes_done_, CAL_MAX_PASSES_PER_DIR, cov_fwd, cov_rev);
          if (cov_fwd >= CAL_EARLY_EXIT_COVERAGE_PCT &&
              cov_rev >= CAL_EARLY_EXIT_COVERAGE_PCT) {
            Serial.printf("cal: both directions >= %d%%, early exit to finalize\n",
              CAL_EARLY_EXIT_COVERAGE_PCT);
            transitionTo(CAL_FINALIZE);
          } else if (fwd_passes_done_ >= CAL_MAX_PASSES_PER_DIR &&
                     rev_passes_done_ >= CAL_MAX_PASSES_PER_DIR) {
            Serial.println("cal: all passes exhausted, finalizing");
            transitionTo(CAL_FINALIZE);
          } else {
            Serial.printf("cal pass %d/%d fwd starting\n",
              fwd_passes_done_ + 1, CAL_MAX_PASSES_PER_DIR);
            startPass(+1);
            transitionTo(CAL_SWEEP_FWD);
          }
        }
        break;
      }
      case CAL_FINALIZE: {
        finalize();
        transitionTo(CAL_DONE);
        break;
      }
      default:
        break;
    }
    return true;
  }

  // External `stop` — abort cleanly, don't touch NVS.
  void abort(const char* reason) {
    if (!active()) return;
    Serial.printf("cal ERROR: %s. partial LUT discarded (NVS not written).\n", reason);
    printPartialStatsAndAbort();
  }

  // Advance out of terminal states back to IDLE so a subsequent `calibrate`
  // can run without reboot. CLI calls this after printing the summary.
  void reap() { state_cal_ = CAL_IDLE; }

private:
  CalState state_cal_       = CAL_IDLE;
  uint32_t t_phase_ms_      = 0;
  uint32_t t_start_ms_      = 0;
  char     slot_[NVS_SLOT_MAX + 1] = "";
  int16_t  pass_fwd_[LUT_BINS];
  int16_t  pass_rev_[LUT_BINS];
  int      fwd_bins_sampled_  = 0;
  int      rev_bins_sampled_  = 0;
  int      last_progress_bin_ = -1;
  int      dir_              = +1;
  int32_t  nudge_mA_         = CAL_NUDGE_START_MA;
  uint32_t sat_accum_ms_     = 0;
  uint32_t runaway_accum_ms_ = 0;
  int      fwd_passes_done_  = 0;
  int      rev_passes_done_  = 0;
  int32_t  enc_at_pass_start_ = 0;

  void transitionTo(CalState s) {
    state_cal_  = s;
    t_phase_ms_ = millis();
  }

  // Reset pass-local state and set up PI for the next pass. Preserve per-bin
  // sample buffers (EMA across passes is intentional refinement).
  void startPass(int direction) {
    dir_               = direction;
    state_.target_dps  = (direction > 0) ? CAL_TARGET_DPS : -CAL_TARGET_DPS;
    pi_.reset();
    sat_accum_ms_      = 0;
    runaway_accum_ms_  = 0;
    last_progress_bin_ = -1;
    enc_at_pass_start_ = state_.enc_steps;
  }

  // Per-direction coverage: fraction of bins sampled by that direction's sweep.
  int coveragePctFwd() const {
    int n = 0;
    for (int i = 0; i < LUT_BINS; i++) {
      if (pass_fwd_[i] != LUT_SENTINEL) n++;
    }
    return (n * 100) / LUT_BINS;
  }
  int coveragePctRev() const {
    int n = 0;
    for (int i = 0; i < LUT_BINS; i++) {
      if (pass_rev_[i] != LUT_SENTINEL) n++;
    }
    return (n * 100) / LUT_BINS;
  }
  // Union coverage retained for operator summary only (timeout path).
  int coveragePctUnion() const {
    int n = 0;
    for (int i = 0; i < LUT_BINS; i++) {
      if (pass_fwd_[i] != LUT_SENTINEL || pass_rev_[i] != LUT_SENTINEL) n++;
    }
    return (n * 100) / LUT_BINS;
  }

  // Drive velocity PI at target (±CAL_TARGET_DPS), sample pi_integral into bin.
  // Returns true when this pass is "done" (all bins sampled OR time budget hit).
  bool runPiSweep(float ang_deg, float vel_filt_dps,
                  int16_t* pass_buf, int& bins_sampled,
                  const char* dir_tag) {
    // Control tick: PI only, no FF during cal (the integrator IS the FF).
    int32_t cmd = pi_.update(state_.target_dps, vel_filt_dps, CTRL_DT_S);
    state_.err_dps        = state_.target_dps - vel_filt_dps;
    state_.pi_integral_mA = pi_.integral();
    state_.pi_out_mA      = pi_.lastOut();
    state_.ff_mA          = 0.0f;
    state_.cmd_mA         = cmd;
    driver_.setCurrent(cmd);

    // Saturation abort per spec §3.2 step 10.
    if (cmd >= state_.current_limit_mA || cmd <= -state_.current_limit_mA) {
      sat_accum_ms_ += (uint32_t)(CTRL_DT_S * 1000.0f);
      if (sat_accum_ms_ >= CAL_SAT_ABORT_MS) {
        char msg[64];
        snprintf(msg, sizeof(msg), "saturated at angle %.1f deg", ang_deg);
        Serial.printf("cal ERROR: %s\n", msg);
        printPartialStatsAndAbort();
        return false;
      }
    } else {
      sat_accum_ms_ = 0;
    }

    // Runaway detector per spec §3.7 — same thresholds as normal control path.
    // Catches "commanded within climit but velocity diverged from target."
    if (fabsf(state_.err_dps) > RUNAWAY_ERR_DPS) {
      runaway_accum_ms_ += (uint32_t)(CTRL_DT_S * 1000.0f);
      if (runaway_accum_ms_ >= RUNAWAY_MS) {
        Serial.printf("cal ERROR: runaway (|err|>%.0f dps >500 ms) at angle %.1f deg\n",
          RUNAWAY_ERR_DPS, ang_deg);
        printPartialStatsAndAbort();
        return false;
      }
    } else {
      runaway_accum_ms_ = 0;
    }

    // Sample gate: only snapshot pi_integral when velocity is in the stable
    // moving regime per spec. Sign-check avoids sampling through direction
    // reversals during settle.
    float vabs = fabsf(vel_filt_dps);
    bool in_band = (vabs >= CAL_VEL_MIN_DPS && vabs <= CAL_VEL_MAX_DPS);
    bool sign_ok = (dir_ > 0) ? (vel_filt_dps > 0.0f) : (vel_filt_dps < 0.0f);
    if (in_band && sign_ok) {
      int bin = CoggingLUT::bin_for_angle_deg(ang_deg);
      float ff_mA = state_.pi_integral_mA;
      float ff_x100 = ff_mA * LUT_SCALE;
      // Clamp to int16 range before cast.
      if (ff_x100 >  32760.0f) ff_x100 =  32760.0f;
      if (ff_x100 < -32760.0f) ff_x100 = -32760.0f;
      int16_t new_val = (int16_t)ff_x100;
      if (pass_buf[bin] == LUT_SENTINEL) {
        pass_buf[bin] = new_val;
        bins_sampled++;
      } else {
        // EMA alpha=0.3 per spec.
        float prev = (float)pass_buf[bin];
        float ema  = prev + CAL_EMA_ALPHA * ((float)new_val - prev);
        pass_buf[bin] = (int16_t)ema;
      }

      // Progress every CAL_PROGRESS_BINS (=5 deg). Show THIS direction's
      // coverage so the operator sees each direction converge independently.
      if (last_progress_bin_ < 0 || abs(bin - last_progress_bin_) >= CAL_PROGRESS_BINS) {
        int pass_num = (dir_ > 0) ? (fwd_passes_done_ + 1) : (rev_passes_done_ + 1);
        int cov_dir  = (dir_ > 0) ? coveragePctFwd() : coveragePctRev();
        Serial.printf("cal pass %d/%d %s [%+6.1f deg] bin=%d ff=%+6.1f mA  cov_%s=%d%%\n",
          pass_num, CAL_MAX_PASSES_PER_DIR, dir_tag,
          ang_deg, bin, ff_mA, dir_tag, cov_dir);
        last_progress_bin_ = bin;
      }
    }

    // Pass-done condition: one full rev of angular travel (36000 encoder steps)
    // since pass start. Using raw encoder delta avoids the 360-deg wrap edge
    // case that `ang_deg` suffers from.
    int32_t travel = state_.enc_steps - enc_at_pass_start_;
    if (travel < 0) travel = -travel;
    if (travel >= STEPS_PER_REV) return true;
    // Global timeout check is done in outer tick().
    return false;
  }

  void finalize() {
    // Directional finalize: pass_fwd_ -> lut_fwd_, pass_rev_ -> lut_rev_.
    // Each direction is gap-filled INDEPENDENTLY from its own sentinel mask.
    // A bin unsampled in forward does NOT borrow from reverse; it interpolates
    // across its forward-sweep neighbors. This is the whole point of M3.1:
    // preserve the asymmetry caused by cogging hysteresis.
    lut_fwd_.clear();
    lut_rev_.clear();
    finalizeOneDirection(pass_fwd_, lut_fwd_, "fwd");
    finalizeOneDirection(pass_rev_, lut_rev_, "rev");
    lut_fwd_.markLoaded(true);
    lut_rev_.markLoaded(true);

    // NVS save (both LUTs under one meta blob).
    LutMeta meta{};
    meta.version        = LUT_FORMAT_VERSION;
    meta.ts_ms_at_write = millis();
    meta.vin_raw        = driver_.getVinRaw();
    meta.fw_version     = 0x0310;
    if (nvsSaveLuts(slot_, lut_fwd_, lut_rev_, meta)) {
      Serial.printf("cal COMPLETE. saved fwd+rev to slot=%s (NVS).\n", slot_);
    } else {
      Serial.println("cal WARNING: NVS save failed; LUTs live in RAM only.");
    }

    // Restore state per spec §3.2 step 9.
    state_.target_dps = 0.0f;
    state_.enabled    = false;
    pi_.reset();
    driver_.setCurrent(0);
  }

  // Copy one pass buffer into a final LUT and gap-fill its unsampled bins by
  // linear interpolation across the nearest sampled neighbors on the SAME
  // sentinel mask. Prints a per-direction summary line.
  void finalizeOneDirection(const int16_t* pass_buf, CoggingLUT& out_lut,
                            const char* dir_tag) {
    int real_samples = 0;
    for (int i = 0; i < LUT_BINS; i++) {
      if (pass_buf[i] != LUT_SENTINEL) {
        out_lut.set(i, pass_buf[i]);
        real_samples++;
      } else {
        out_lut.set(i, 0);
      }
    }
    int interpolated = 0;
    if (real_samples == 0) {
      Serial.printf("cal WARNING: %s direction has zero samples; LUT is all-zero.\n", dir_tag);
    } else if (real_samples < LUT_BINS) {
      for (int i = 0; i < LUT_BINS; i++) {
        if (pass_buf[i] != LUT_SENTINEL) continue;
        // Walk forward (CW) to nearest sampled bin in the same direction's mask.
        int fwd_dist = 0;
        int fwd_bin  = -1;
        for (int k = 1; k < LUT_BINS; k++) {
          int idx = (i + k) % LUT_BINS;
          if (pass_buf[idx] != LUT_SENTINEL) {
            fwd_bin  = idx;
            fwd_dist = k;
            break;
          }
        }
        // Walk backward (CCW) to nearest sampled bin.
        int rev_dist = 0;
        int rev_bin  = -1;
        for (int k = 1; k < LUT_BINS; k++) {
          int idx = (i - k + LUT_BINS) % LUT_BINS;
          if (pass_buf[idx] != LUT_SENTINEL) {
            rev_bin  = idx;
            rev_dist = k;
            break;
          }
        }
        if (fwd_bin < 0 || rev_bin < 0) continue;
        float v_fwd = (float)out_lut.get(fwd_bin);
        float v_rev = (float)out_lut.get(rev_bin);
        float total = (float)(fwd_dist + rev_dist);
        float interp = (v_fwd * (float)rev_dist + v_rev * (float)fwd_dist) / total;
        out_lut.set(i, (int16_t)interp);
        interpolated++;
      }
      Serial.printf(
        "cal %s: gap-filled %d bins via interpolation (%d real + %d interpolated = %d)\n",
        dir_tag, interpolated, real_samples, interpolated, LUT_BINS);
    }

    float mn, mx, mean, absm;
    int absb;
    out_lut.stats(mn, mx, mean, absm, absb);
    int cov_dir_raw = (real_samples * 100) / LUT_BINS;
    Serial.printf(
      "cal summary %s: min=%+6.1f mA  mean=%+6.1f mA  max=%+6.1f mA  "
      "peak-to-peak=%6.1f mA  |max|=%6.1f @ bin %d (%.1f deg)  coverage=%d%%\n",
      dir_tag, mn, mean, mx, (mx - mn), absm, absb, (float)absb * 0.5f, cov_dir_raw);
  }

  void printPartialStatsAndAbort() {
    Serial.printf("cal aborted. partial cov fwd=%d%% rev=%d%%. NVS unchanged.\n",
      coveragePctFwd(), coveragePctRev());
    state_.target_dps = 0.0f;
    state_.enabled    = false;
    pi_.reset();
    driver_.setCurrent(0);
    transitionTo(CAL_ABORT);
  }
};

static Calibration cal_;

// ---------- Power report ----------
static void printPowerReport() {
  int32_t vin_raw  = driver_.getVinRaw();
  int32_t temp_c   = driver_.getTempC();
  int32_t curr_raw = driver_.getCurrentReadbackRaw();
  uint8_t fw       = driver_.getFirmwareVersion();
  float   vin_v    = vin_raw  / 100.0f;
  float   curr_mA  = curr_raw / 100.0f;

  Serial.printf(
    "vin=%.2f V (raw=%d)  temp=%d C  current_readback=%.2f mA (raw=%d)  fw_version=%u\n",
    vin_v, vin_raw, (int)temp_c, curr_mA, curr_raw, fw);

  if (vin_v >= 10.0f) {
    Serial.println("POWER STATUS: OK — 12 V supply detected.");
  } else {
    Serial.println("POWER STATUS: LOW — motor on bus power; switch to 12 V PD before calibration.");
    if (vin_v < 6.0f) {
      Serial.println("DO NOT RUN CALIBRATION. Switch supply first.");
    }
  }
}

// ---------- NVS helpers ----------
// Short keys required by NVS 15-char limit (NVS_KEY_NAME_MAX_SIZE=16 includes
// null). Slot strings are validated to NVS_SLOT_MAX (10), so:
//   f/<slot>   fwd LUT blob (2 + 10 = 12 chars)
//   r/<slot>   rev LUT blob (2 + 10 = 12 chars)
//   m/<slot>   versioned meta (2 + 10 = 12 chars)
// The old single-LUT key "lut/<slot>" is deliberately left orphan (not migrated,
// not auto-cleared): the version byte in m/<slot> is what makes old data safe
// to ignore. `lut clear` removes all three new keys and also the legacy key for
// hygiene.
static void buildFwdKey(const char* slot, char* out, size_t outlen) {
  snprintf(out, outlen, "f/%s", slot);
}
static void buildRevKey(const char* slot, char* out, size_t outlen) {
  snprintf(out, outlen, "r/%s", slot);
}
static void buildMetaKey(const char* slot, char* out, size_t outlen) {
  snprintf(out, outlen, "m/%s", slot);
}
static void buildLegacyLutKey(const char* slot, char* out, size_t outlen) {
  snprintf(out, outlen, "lut/%s", slot);
}

static bool nvsLoadLuts(const char* slot, CoggingLUT& out_fwd, CoggingLUT& out_rev,
                        LutMeta& out_meta) {
  Preferences p;
  if (!p.begin(NVS_NAMESPACE, true)) return false;
  char fkey[16], rkey[16], mkey[16];
  buildFwdKey(slot, fkey, sizeof(fkey));
  buildRevKey(slot, rkey, sizeof(rkey));
  buildMetaKey(slot, mkey, sizeof(mkey));

  // Meta size AND version must match exactly. Any mismatch means outdated.
  size_t mlen = p.getBytesLength(mkey);
  if (mlen != sizeof(LutMeta)) { p.end(); return false; }
  p.getBytes(mkey, &out_meta, sizeof(LutMeta));
  if (out_meta.version != LUT_FORMAT_VERSION) { p.end(); return false; }

  size_t flen = p.getBytesLength(fkey);
  size_t rlen = p.getBytesLength(rkey);
  if (flen != LUT_BLOB_BYTES || rlen != LUT_BLOB_BYTES) { p.end(); return false; }

  size_t gf = p.getBytes(fkey, out_fwd.data(), LUT_BLOB_BYTES);
  size_t gr = p.getBytes(rkey, out_rev.data(), LUT_BLOB_BYTES);
  p.end();
  if (gf != LUT_BLOB_BYTES || gr != LUT_BLOB_BYTES) return false;
  out_fwd.markLoaded(true);
  out_rev.markLoaded(true);
  return true;
}

static bool nvsSaveLuts(const char* slot, const CoggingLUT& fwd, const CoggingLUT& rev,
                        const LutMeta& meta) {
  Preferences p;
  if (!p.begin(NVS_NAMESPACE, false)) return false;
  char fkey[16], rkey[16], mkey[16];
  buildFwdKey(slot, fkey, sizeof(fkey));
  buildRevKey(slot, rkey, sizeof(rkey));
  buildMetaKey(slot, mkey, sizeof(mkey));
  size_t wf = p.putBytes(fkey, fwd.data(), LUT_BLOB_BYTES);
  size_t wr = p.putBytes(rkey, rev.data(), LUT_BLOB_BYTES);
  size_t wm = p.putBytes(mkey, &meta, sizeof(LutMeta));
  p.end();
  return (wf == LUT_BLOB_BYTES && wr == LUT_BLOB_BYTES && wm == sizeof(LutMeta));
}

static bool nvsClearLuts(const char* slot) {
  Preferences p;
  if (!p.begin(NVS_NAMESPACE, false)) return false;
  char fkey[16], rkey[16], mkey[16], legacy[16];
  buildFwdKey(slot, fkey, sizeof(fkey));
  buildRevKey(slot, rkey, sizeof(rkey));
  buildMetaKey(slot, mkey, sizeof(mkey));
  buildLegacyLutKey(slot, legacy, sizeof(legacy));
  bool a = p.remove(fkey);
  bool b = p.remove(rkey);
  bool c = p.remove(mkey);
  // Legacy v1 key: remove if present, ignore otherwise.
  p.remove(legacy);
  p.end();
  return a || b || c;
}

static bool nvsSaveActiveSlot(const char* slot) {
  Preferences p;
  if (!p.begin(NVS_NAMESPACE, false)) return false;
  bool ok = p.putString(NVS_KEY_SLOT, slot) > 0;
  p.end();
  return ok;
}

static void nvsLoadActiveSlot(char* out_slot, size_t maxlen) {
  Preferences p;
  if (!p.begin(NVS_NAMESPACE, true)) {
    strncpy(out_slot, NVS_SLOT_DEFAULT, maxlen);
    out_slot[maxlen - 1] = '\0';
    return;
  }
  String s = p.getString(NVS_KEY_SLOT, NVS_SLOT_DEFAULT);
  p.end();
  strncpy(out_slot, s.c_str(), maxlen);
  out_slot[maxlen - 1] = '\0';
}

// ---------- I2C health / recovery ----------
// Lightweight health probe: ACK-only transaction. Called from loop() at
// I2C_PROBE_PERIOD_MS cadence (never from the timer ISR). Does nothing while
// already offline — recovery owns the bus in that state.
static void i2cHealthProbe() {
  if (!driver_.online()) return;
  uint32_t now = millis();
  if (now - i2c_last_probe_ms_ < I2C_PROBE_PERIOD_MS) return;
  i2c_last_probe_ms_ = now;
  if (RollerDriver::ackProbe()) {
    i2c_consecutive_fails_ = 0;
  } else {
    i2c_consecutive_fails_++;
    if (i2c_consecutive_fails_ >= I2C_FAIL_THRESHOLD) {
      i2cMarkFault();
    }
  }
}

// Transition: healthy → fault. Disables control, zero current, schedules first
// recovery attempt. Prints the operator-facing line exactly once per fault.
static void i2cMarkFault() {
  if (!driver_.online()) return;  // already handled
  Serial.println("\nI2C FAULT: roller stopped responding. Attempting recovery...");
  driver_.setOnline(false);
  // Control must limp. Don't touch driver_.setCurrent (it no-ops now anyway),
  // but clear state so the user sees motor=OFF in telemetry.
  state_.enabled        = false;
  state_.target_dps     = 0.0f;
  state_.cmd_mA         = 0;
  state_.ff_mA          = 0.0f;
  state_.pi_out_mA      = 0.0f;
  pi_.reset();
  i2c_recovery_state_   = I2C_FAULT_WAITING;
  i2c_next_recovery_ms_ = millis() + I2C_RECOVERY_FIRST_MS;
  i2c_consecutive_fails_ = 0;
}

// Non-blocking recovery state machine. Called from loop() every cycle.
// Flow: reinit Wire → probe up to I2C_RECOVERY_RETRIES times with 100 ms gaps.
// On success mark online (control stays disabled — user must re-arm with
// `velocity`). On failure schedule next attempt in I2C_RECOVERY_BACKOFF_MS.
static void i2cServiceRecovery() {
  if (driver_.online()) return;
  if (i2c_recovery_state_ != I2C_FAULT_WAITING) return;
  uint32_t now = millis();
  if ((int32_t)(now - i2c_next_recovery_ms_) < 0) return;

  // Bus reset: end() + re-begin() + set timeout.
  Wire.end();
  delay(I2C_RECOVERY_GAP_MS);
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  Wire.setTimeOut(WIRE_TIMEOUT_MS);

  bool ok = false;
  for (int i = 0; i < I2C_RECOVERY_RETRIES; i++) {
    if (RollerDriver::ackProbe()) {
      // Follow up with firmware-version read to confirm the library side works.
      // If the library call itself wedges, the task WDT will catch it in 10 s.
      esp_task_wdt_reset();
      driver_.setOnline(true);       // briefly true so getFirmwareVersion is allowed
      uint8_t fw = driver_.getFirmwareVersion();
      if (fw != 0) { ok = true; break; }
      driver_.setOnline(false);
    }
    delay(I2C_RECOVERY_GAP_MS);
  }

  if (ok) {
    Serial.println("I2C RECOVERED");
    // Driver is online; control remains disabled. Force-disable to be explicit.
    state_.enabled = false;
    driver_.setCurrent(0);
    i2c_recovery_state_    = I2C_HEALTHY;
    i2c_consecutive_fails_ = 0;
  } else {
    Serial.printf("I2C recovery failed. Retrying in %lu ms.\n",
      (unsigned long)I2C_RECOVERY_BACKOFF_MS);
    driver_.setOnline(false);
    i2c_next_recovery_ms_ = millis() + I2C_RECOVERY_BACKOFF_MS;
  }
}

// ---------- Reboot / reset helper ----------
static void doReboot() {
  Serial.println("rebooting...");
  Serial.flush();
  delay(50);
  ESP.restart();
}

// ---------- ControlLoop ----------
static void IRAM_ATTR tickCallback(void*) { tick_pending_ = true; }

class ControlLoop {
public:
  void begin() {
    const esp_timer_create_args_t args = {
      .callback = &tickCallback,
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "ctrl200hz",
      .skip_unhandled_events = true,
    };
    esp_timer_create(&args, &tick_timer_);
    esp_timer_start_periodic(tick_timer_, TICK_PERIOD_US);
  }

  void service() {
    while (tick_pending_) {
      tick_pending_ = false;
      tick();
    }
  }

private:
  void tick() {
    int32_t enc = driver_.readEncoder();
    if (state_.first_tick) {
      state_.enc_prev = enc;
      state_.first_tick = false;
    }
    int32_t dsteps = enc - state_.enc_prev;
    float vel_raw = (dsteps * (360.0f / (float)STEPS_PER_REV)) / CTRL_DT_S;
    state_.enc_steps    = enc;
    state_.enc_prev     = enc;
    state_.ang_deg      = stepsToDeg(enc);
    state_.vel_raw_dps  = vel_raw;
    state_.vel_filt_dps += VEL_LPF_ALPHA * (vel_raw - state_.vel_filt_dps);

    // Rest detector drives break-free kick — counts consecutive ticks under
    // KICK_REST_DPS threshold, resets otherwise.
    if (fabsf(state_.vel_filt_dps) < KICK_REST_DPS) {
      state_.rest_accum_ms += (uint32_t)(CTRL_DT_S * 1000.0f);
    } else {
      state_.rest_accum_ms = 0;
    }

    // Calibration state machine consumes this tick's control if active.
    if (cal_.active()) {
      cal_.tick(state_.ang_deg, state_.vel_filt_dps, (int32_t)millis());
      state_.tick_count++;
      return;
    }

    if (state_.enabled) {
      // Kick expiry (shared: break-free or auto-kick). Restore user climit,
      // stamp last_kick_end_ms so the shared cooldown starts from release —
      // the motor gets at least AUTO_KICK_COOLDOWN_MS of "quiet" before
      // another kick can arm.
      if (state_.kick_active && (millis() - state_.kick_start_ms) >= state_.kick_duration_ms) {
        state_.kick_active       = false;
        state_.current_limit_mA  = state_.current_limit_user_mA;
        pi_.setLimit(state_.current_limit_mA);
        state_.last_kick_end_ms  = millis();
      }

      // Auto-kick evaluator. Gated off while a kick is already active so the
      // stuck clock can't keep ticking through the kick window and
      // immediately re-arm at cooldown expiry.
      if (state_.auto_kick_enabled && !state_.kick_active) {
        bool vel_low   = fabsf(state_.vel_filt_dps) < AUTO_KICK_VEL_THRESHOLD_DPS;
        bool err_high  = fabsf(state_.target_dps - state_.vel_filt_dps) > AUTO_KICK_ERR_THRESHOLD_DPS;
        if (vel_low && err_high) {
          state_.stuck_accum_ms += (uint32_t)(CTRL_DT_S * 1000.0f);
        } else {
          state_.stuck_accum_ms = 0;
        }
        uint32_t now_ms   = millis();
        uint32_t since_kick = now_ms - state_.last_kick_end_ms;
        if (state_.stuck_accum_ms > AUTO_KICK_STUCK_MS &&
            since_kick > AUTO_KICK_COOLDOWN_MS) {
          uint32_t stuck_for_ms = state_.stuck_accum_ms;
          state_.kick_active       = true;
          state_.kick_start_ms     = now_ms;
          state_.kick_duration_ms  = AUTO_KICK_DURATION_MS;
          state_.current_limit_mA  = AUTO_KICK_LIMIT_MA;
          pi_.setLimit(AUTO_KICK_LIMIT_MA);
          state_.auto_kicks_fired++;
          state_.stuck_accum_ms    = 0;
          Serial.printf("auto-kick @ angle=%.1f  stuck_for=%lums\n",
            state_.ang_deg, (unsigned long)stuck_for_ms);
        }
      } else {
        // Not evaluating — keep counter clean for next window.
        state_.stuck_accum_ms = 0;
      }

      // Directional FF lookup. Sharp switch at ±LUT_DIR_DEADBAND_DPS (0.5 dps).
      // In the deadband the motor is supposed to be holding; PI + integrator
      // carry the load, FF = 0. On direction crossings (e.g. BREATH reversing)
      // velocity is near zero at the switch so the discrete change is benign.
      // Future enhancement: linear blend over [0.5, 2.0] dps band for smoother
      // handoff. Keeping sharp for M3.1 per spec.
      float ff_mA = 0.0f;
      if (state_.target_dps > LUT_DIR_DEADBAND_DPS) {
        if (lut_fwd_.loaded()) ff_mA = lut_fwd_.lookup_linear(state_.ang_deg);
      } else if (state_.target_dps < -LUT_DIR_DEADBAND_DPS) {
        if (lut_rev_.loaded()) ff_mA = lut_rev_.lookup_linear(state_.ang_deg);
      }
      int32_t pi_out = pi_.update(state_.target_dps, state_.vel_filt_dps, CTRL_DT_S);
      float total_mA = (float)pi_out + ff_mA;
      int32_t cmd = clamp_i32((int32_t)total_mA,
                              -state_.current_limit_mA, state_.current_limit_mA);
      state_.err_dps        = state_.target_dps - state_.vel_filt_dps;
      state_.pi_integral_mA = pi_.integral();
      state_.pi_out_mA      = pi_.lastOut();
      state_.ff_mA          = ff_mA;
      state_.cmd_mA         = cmd;
      driver_.setCurrent(cmd);

      if (fabsf(state_.err_dps) > RUNAWAY_ERR_DPS) {
        state_.runaway_accum_ms += (uint32_t)(CTRL_DT_S * 1000.0f);
        if (state_.runaway_accum_ms >= RUNAWAY_MS) {
          stopControlAutoAbort("runaway (|err|>200 dps >500 ms)");
          return;
        }
      } else {
        state_.runaway_accum_ms = 0;
      }

      if (cmd >= state_.current_limit_mA || cmd <= -state_.current_limit_mA) {
        state_.sat_accum_ms += (uint32_t)(CTRL_DT_S * 1000.0f);
        if (state_.sat_accum_ms >= SAT_MS) {
          stopControlAutoAbort("saturated (|cmd|=climit >2000 ms)");
          return;
        }
      } else {
        state_.sat_accum_ms = 0;
      }
    } else {
      state_.cmd_mA       = 0;
      state_.pi_out_mA    = 0.0f;
      state_.ff_mA        = 0.0f;
      // Do not clobber the power-on wiggle. When wiggle_running is true, the
      // loop()-level blocking wiggle sequence owns the current setpoint.
      if (!state_.wiggle_running) {
        driver_.setCurrent(0);
      }
      state_.runaway_accum_ms = 0;
      state_.sat_accum_ms     = 0;
    }

    state_.tick_count++;
  }
};

static ControlLoop ctrl_;

// ---------- Control helpers ----------
static void enableControl(float target_dps) {
  pi_.reset();
  state_.target_dps       = target_dps;
  state_.pi_integral_mA   = 0.0f;
  state_.runaway_accum_ms = 0;
  state_.sat_accum_ms     = 0;

  // Break-free kick only when starting from rest (|vel| low for >=500 ms).
  // Also reset stuck clock on transition — we're entering control fresh, the
  // auto-kick evaluator shouldn't carry stale accumulation.
  state_.stuck_accum_ms = 0;
  if (state_.rest_accum_ms >= KICK_REST_MS) {
    state_.kick_active       = true;
    state_.kick_start_ms     = millis();
    state_.kick_duration_ms  = KICK_DURATION_MS;
    state_.current_limit_mA  = KICK_CURRENT_MA;
    pi_.setLimit(KICK_CURRENT_MA);
    Serial.println("break-free kick ON (600 mA, 200 ms)");
  } else {
    state_.kick_active       = false;
    state_.current_limit_mA  = state_.current_limit_user_mA;
    pi_.setLimit(state_.current_limit_user_mA);
  }

  // One-shot no-LUT warning per spec §3.3. Warn if EITHER directional LUT
  // is missing — fwd-only or rev-only is treated as incomplete.
  if (!lutReady() && !state_.lut_missing_warned) {
    Serial.printf("WARNING: no LUT (fwd=%s rev=%s), feedforward partial/disabled\n",
      lut_fwd_.loaded() ? "YES" : "NO",
      lut_rev_.loaded() ? "YES" : "NO");
    state_.lut_missing_warned = true;
  }

  state_.enabled = true;
}

static void disableControl() {
  // If a kick was active at teardown, stamp last_kick_end_ms so the shared
  // cooldown is honored if control is re-enabled quickly.
  if (state_.kick_active) {
    state_.last_kick_end_ms = millis();
  }
  state_.enabled        = false;
  state_.target_dps     = 0.0f;
  state_.err_dps        = 0.0f;
  state_.cmd_mA         = 0;
  state_.kick_active    = false;
  state_.stuck_accum_ms = 0;
  state_.current_limit_mA = state_.current_limit_user_mA;
  pi_.setLimit(state_.current_limit_user_mA);
  pi_.reset();
  state_.pi_integral_mA = 0.0f;
  state_.pi_out_mA      = 0.0f;
  state_.ff_mA          = 0.0f;
  driver_.setCurrent(0);
}

static void stopControlAutoAbort(const char* reason) {
  disableControl();
  Serial.printf("\nERROR: auto-stop — %s. control=OFF  motor=limp\n> ", reason);
}

static void abortCalibration(const char* reason) {
  cal_.abort(reason);
  cal_.reap();
  disableControl();
}

// ---------- SerialCLI ----------
class SerialCLI {
public:
  void begin() { Serial.print("> "); }

  void service() {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        String line = buf_;
        buf_ = "";
        handleLine(line);
      } else if (buf_.length() < 120) {
        buf_ += c;
      }
    }
    if (stream_) streamTick();

    // Reap cal FSM on completion so the next `calibrate` invocation is clean.
    if (cal_.state() == CAL_DONE || cal_.state() == CAL_ABORT) {
      cal_.reap();
      Serial.print("> ");
    }
  }

private:
  String   buf_;
  bool     stream_ = false;
  uint32_t last_stream_ms_ = 0;

  void streamTick() {
    uint32_t now = millis();
    if (now - last_stream_ms_ < 50) return;
    last_stream_ms_ = now;
    Serial.printf(
      "t=%lu ang=%.3f vel=%.3f vraw=%.3f tgt=%.3f err=%.3f "
      "ff_mA=%+6.1f pi_out_mA=%+6.1f cmd_mA=%d pi_i=%.1f\n",
      (unsigned long)now,
      state_.ang_deg, state_.vel_filt_dps, state_.vel_raw_dps,
      state_.target_dps, state_.err_dps,
      state_.ff_mA, state_.pi_out_mA, state_.cmd_mA, state_.pi_integral_mA);
  }

  static int splitArgs(const String& s, String out[4]) {
    int n = 0, start = 0;
    for (int i = 0; i <= (int)s.length() && n < 4; i++) {
      if (i == (int)s.length() || s[i] == ' ') {
        if (i > start) { out[n++] = s.substring(start, i); }
        start = i + 1;
      }
    }
    return n;
  }

  void handleLine(String line) {
    line.trim();
    if (line.length() == 0) { Serial.print("> "); return; }
    String lower = line;
    lower.toLowerCase();
    String parts[4];
    int nparts = splitArgs(lower, parts);
    const String& cmd = parts[0];

    if (cmd == "help") {
      Serial.println("commands:");
      Serial.println("  status                  one-shot angle/vel/cmd/ff/pi/rate/uptime");
      Serial.println("  stream on|off           toggle 20 Hz telemetry dump");
      Serial.println("  velocity <dps>          set target, enable PI+FF control");
      Serial.println("  stop                    disable control, motor limp, abort calib");
      Serial.println("  gains [<kp> <ki>]       print or set PI gains (resets integrator)");
      Serial.println("  climit [<mA>]           print or set current saturation limit");
      Serial.println("  stall0 | stall1         disable|enable firmware stall protection");
      Serial.println("  power                   one-shot vin/temp/current/fw + supply verdict");
      Serial.println("  calibrate               run multi-pass cogging calibration (<=8 min, up to 3 fwd + 3 rev)");
      Serial.println("  lut                     stats: min/mean/max + head/tail bins + cov");
      Serial.println("  lut slot <name>         set active slot (<=10 chars), volatile");
      Serial.println("  lut save                write current RAM LUT to NVS in active slot");
      Serial.println("  lut load                read active slot from NVS into RAM");
      Serial.println("  lut clear               wipe active slot from NVS");
      Serial.println("  lut raw                 print 720 bins CSV for external inspection");
      Serial.println("  autokick on|off         enable/disable mid-motion stuck-recovery kick");
      Serial.println("  autokick status         print auto-kick enable state, count, config");
      Serial.println("  reboot | reset          software restart of the Xiao (ESP.restart)");
      Serial.println("  help                    this list");
    } else if (cmd == "reboot" || cmd == "reset") {
      doReboot();
    } else if (cmd == "status") {
      handleStatus();
    } else if (cmd == "stream" && nparts >= 2 && parts[1] == "on") {
      stream_ = true;
      Serial.println("stream ON");
    } else if (cmd == "stream" && nparts >= 2 && parts[1] == "off") {
      stream_ = false;
      Serial.println("stream OFF");
    } else if (cmd == "velocity" && nparts >= 2) {
      if (!driver_.online()) {
        Serial.println("ERROR: roller OFFLINE; control disabled. Check hardware, then `reboot`.");
      } else if (cal_.active()) {
        Serial.println("ERROR: calibration in progress; run `stop` first");
      } else {
        float tgt = parts[1].toFloat();
        enableControl(tgt);
        Serial.printf("target=%.3f dps  control=ON\n", tgt);
      }
    } else if (cmd == "stop") {
      if (cal_.active()) {
        abortCalibration("operator stop");
      } else {
        disableControl();
      }
      Serial.println("control=OFF  motor=limp");
    } else if (cmd == "gains") {
      handleGains(parts, nparts);
    } else if (cmd == "climit") {
      handleClimit(parts, nparts);
    } else if (cmd == "stall0") {
      driver_.setStall0();
      Serial.println("stall protection DISABLED");
    } else if (cmd == "stall1") {
      driver_.setStall1();
      Serial.println("stall protection ENABLED");
    } else if (cmd == "power") {
      printPowerReport();
    } else if (cmd == "calibrate") {
      handleCalibrate();
    } else if (cmd == "lut") {
      handleLutCommand(parts, nparts);
    } else if (cmd == "autokick") {
      handleAutokick(parts, nparts);
    } else {
      Serial.println("? (try: help)");
    }

    // Don't print prompt while cal FSM is actively driving output lines.
    if (!cal_.active()) Serial.print("> ");
  }

  void handleStatus() {
    uint32_t up_ms = millis() - state_.boot_ms;
    Serial.printf(
      "roller=%s  angle=%.3f deg  vel=%.3f dps  vraw=%.3f dps  tgt=%.3f dps  cmd_mA=%d  "
      "ff=%+6.1f pi_out=%+6.1f  ctrl=%s  kp=%.2f ki=%.2f climit=%d  "
      "lut[%s]=fwd[%s]/rev[%s]  autokick=%s(%lu fired)  loop=%.2f Hz  up=%lu ms\n",
      driver_.online() ? "ONLINE" : "OFFLINE",
      state_.ang_deg, state_.vel_filt_dps, state_.vel_raw_dps,
      state_.target_dps, state_.cmd_mA,
      state_.ff_mA, state_.pi_out_mA,
      state_.enabled ? "ON" : "OFF",
      state_.kp, state_.ki, state_.current_limit_mA,
      current_slot_,
      lut_fwd_.loaded() ? "LOADED" : "empty",
      lut_rev_.loaded() ? "LOADED" : "empty",
      state_.auto_kick_enabled ? "ON" : "OFF",
      (unsigned long)state_.auto_kicks_fired,
      state_.measured_hz, (unsigned long)up_ms);
  }

  void handleAutokick(const String* parts, int nparts) {
    if (nparts < 2 || parts[1] == "status") {
      Serial.printf(
        "autokick=%s  fired=%lu  vel_thresh=%.1f dps  err_thresh=%.1f dps  "
        "stuck_ms=%lu  cooldown_ms=%lu  kick_ma=%d  kick_ms=%lu\n",
        state_.auto_kick_enabled ? "ON" : "OFF",
        (unsigned long)state_.auto_kicks_fired,
        AUTO_KICK_VEL_THRESHOLD_DPS, AUTO_KICK_ERR_THRESHOLD_DPS,
        (unsigned long)AUTO_KICK_STUCK_MS,
        (unsigned long)AUTO_KICK_COOLDOWN_MS,
        (int)AUTO_KICK_LIMIT_MA,
        (unsigned long)AUTO_KICK_DURATION_MS);
      return;
    }
    if (parts[1] == "on") {
      state_.auto_kick_enabled = true;
      state_.stuck_accum_ms    = 0;
      Serial.println("autokick=ON");
    } else if (parts[1] == "off") {
      state_.auto_kick_enabled = false;
      state_.stuck_accum_ms    = 0;
      Serial.println("autokick=OFF");
    } else {
      Serial.println("usage: autokick on|off|status");
    }
  }

  void handleGains(const String* parts, int nparts) {
    if (nparts == 1) {
      Serial.printf("kp=%.4f  ki=%.4f\n", state_.kp, state_.ki);
    } else if (nparts >= 3) {
      state_.kp = parts[1].toFloat();
      state_.ki = parts[2].toFloat();
      pi_.setGains(state_.kp, state_.ki);
      pi_.reset();
      state_.pi_integral_mA = 0.0f;
      Serial.printf("kp=%.4f  ki=%.4f  integrator=RESET\n", state_.kp, state_.ki);
    } else {
      Serial.println("usage: gains [<kp> <ki>]");
    }
  }

  void handleClimit(const String* parts, int nparts) {
    if (nparts == 1) {
      Serial.printf("climit=%d mA (user=%d)\n",
        state_.current_limit_mA, state_.current_limit_user_mA);
    } else {
      int32_t lim = parts[1].toInt();
      if (lim < 0) lim = -lim;
      if (lim > 1000) lim = 1000;
      state_.current_limit_user_mA = lim;
      // Don't override an active kick's elevated limit.
      if (!state_.kick_active) {
        state_.current_limit_mA = lim;
        pi_.setLimit(lim);
      }
      Serial.printf("climit=%d mA\n", lim);
    }
  }

  void handleCalibrate() {
    // Preflight checks per spec §3.2 step 1.
    if (!driver_.online()) {
      Serial.println("cal ERROR: roller OFFLINE. Check hardware, then `reboot`.");
      return;
    }
    float vin_v = driver_.getVinRaw() / 100.0f;
    if (vin_v < 10.0f) {
      Serial.printf("cal ERROR: vin=%.2f V below 10 V. Switch to 12 V PD first.\n", vin_v);
      return;
    }
    if (cal_.active()) {
      Serial.println("cal ERROR: calibration already running");
      return;
    }
    if (state_.enabled) {
      Serial.println("cal ERROR: control ON; run `stop` first");
      return;
    }
    if (current_slot_[0] == '\0') {
      Serial.println("cal ERROR: no slot set. run `lut slot <name>` first.");
      return;
    }

    Serial.printf("cal: starting multi-pass sweep on slot=%s\n", current_slot_);
    Serial.printf("cal: up to %d fwd + %d rev passes, <=8 min total, early exit when BOTH directions reach %d%% coverage\n",
      CAL_MAX_PASSES_PER_DIR, CAL_MAX_PASSES_PER_DIR, CAL_EARLY_EXIT_COVERAGE_PCT);

    // Wipe the live LUTs before cal so finalize() builds each from scratch.
    // Stale bins unsampled this run would otherwise leak into the new LUT.
    lut_fwd_.clear();
    lut_rev_.clear();

    // Arm the control path: MODE_CURRENT already set; just ensure output + stall.
    driver_.setStall0();
    driver_.setMode(ROLLER_MODE_CURRENT);
    driver_.setOutput(true);
    state_.enabled    = true;
    state_.target_dps = 0.0f;
    pi_.reset();
    pi_.setLimit(state_.current_limit_user_mA);
    state_.current_limit_mA = state_.current_limit_user_mA;

    cal_.begin(current_slot_);
  }

  void handleLutCommand(const String* parts, int nparts) {
    if (nparts == 1) {
      printLutStats();
      return;
    }
    const String& sub = parts[1];
    if (sub == "slot") {
      if (nparts < 3) {
        Serial.printf("lut slot=%s\n", current_slot_);
        return;
      }
      // Use raw (case-preserved) input — `lower` path mangles case.
      // Simpler: reject if lower and re-read would change; slot names are all
      // lowercase by convention, so this is fine.
      const String& name = parts[2];
      if (name.length() == 0 || name.length() > NVS_SLOT_MAX) {
        Serial.printf("ERROR: slot name must be 1..%u chars\n", (unsigned)NVS_SLOT_MAX);
        return;
      }
      strncpy(current_slot_, name.c_str(), NVS_SLOT_MAX);
      current_slot_[NVS_SLOT_MAX] = '\0';
      Serial.printf("lut slot=%s (volatile; use `lut save` to persist LUT)\n", current_slot_);
    } else if (sub == "save") {
      LutMeta meta{};
      meta.version        = LUT_FORMAT_VERSION;
      meta.ts_ms_at_write = millis();
      meta.vin_raw        = driver_.getVinRaw();
      meta.fw_version     = 0x0310;
      if (nvsSaveLuts(current_slot_, lut_fwd_, lut_rev_, meta)) {
        Serial.printf("lut saved (fwd+rev) to slot=%s\n", current_slot_);
      } else {
        Serial.println("lut save FAILED");
      }
    } else if (sub == "load") {
      LutMeta meta{};
      if (nvsLoadLuts(current_slot_, lut_fwd_, lut_rev_, meta)) {
        float mnf, mxf, meanf, absmf; int absbf;
        float mnr, mxr, meanr, absmr; int absbr;
        lut_fwd_.stats(mnf, mxf, meanf, absmf, absbf);
        lut_rev_.stats(mnr, mxr, meanr, absmr, absbr);
        Serial.printf("lut loaded: slot=%s v=%u\n", current_slot_, (unsigned)meta.version);
        Serial.printf("  fwd: min=%+.1f mean=%+.1f max=%+.1f mA\n", mnf, meanf, mxf);
        Serial.printf("  rev: min=%+.1f mean=%+.1f max=%+.1f mA\n", mnr, meanr, mxr);
      } else {
        Serial.printf("lut load FAILED (slot=%s missing or outdated)\n", current_slot_);
      }
    } else if (sub == "clear") {
      if (nvsClearLuts(current_slot_)) {
        Serial.printf("lut cleared from NVS slot=%s (RAM LUTs unchanged)\n", current_slot_);
      } else {
        Serial.println("lut clear: nothing to remove");
      }
    } else if (sub == "raw") {
      printLutRaw();
    } else {
      Serial.println("usage: lut | lut slot <name> | lut save | lut load | lut clear | lut raw");
    }
  }

  void printLutStats() {
    Serial.printf("lut slot=%s\n", current_slot_);
    printOneLutStats(lut_fwd_, "fwd");
    printOneLutStats(lut_rev_, "rev");
  }

  void printOneLutStats(const CoggingLUT& lut, const char* tag) {
    float mn, mx, mean, absm;
    int absb;
    lut.stats(mn, mx, mean, absm, absb);
    int nonzero = 0;
    for (int i = 0; i < LUT_BINS; i++) if (lut.get(i) != 0) nonzero++;
    int cov = (nonzero * 100) / LUT_BINS;
    Serial.printf(
      "  %s: loaded=%s  min=%+.2f mA  mean=%+.2f mA  max=%+.2f mA  "
      "|max|=%.2f mA @ bin %d (%.1f deg)  nonzero=%d%%\n",
      tag, lut.loaded() ? "YES" : "NO",
      mn, mean, mx, absm, absb, (float)absb * 0.5f, cov);
  }

  void printLutRaw() {
    Serial.println("bin,deg,ff_fwd_mA,ff_rev_mA");
    for (int i = 0; i < LUT_BINS; i++) {
      Serial.printf("%d,%.1f,%+.2f,%+.2f\n",
        i, (float)i * 0.5f,
        (float)lut_fwd_.get(i) / LUT_SCALE,
        (float)lut_rev_.get(i) / LUT_SCALE);
    }
  }
};

static SerialCLI cli_;

// ---------- Power-on wiggle ----------
// Proof-of-life diagnostic: brief blocking +250/-250/+250 mA wiggle so Edson
// can see from across the room whether the 12 V PD is delivering power.
// Caller is responsible for gating conditions (offline, cal, control ON).
// Sets state_.wiggle_running so the 200 Hz tick()'s disabled branch skips
// its setCurrent(0) during the 900 ms sequence.
static void runPowerOnWiggle() {
  if (!driver_.online()) return;
  float vin_v = driver_.getVinRaw() / 100.0f;
  Serial.printf("PD DETECTED: vin=%.2f V — running power-on wiggle\n", vin_v);

  state_.wiggle_running = true;

  // Ensure we're armed: MODE_CURRENT, output on, stall off. The setup path
  // already does this on boot, but a hot-plug re-trigger might land after a
  // fault recovered, so be explicit.
  driver_.setStall0();
  driver_.setMode(ROLLER_MODE_CURRENT);
  driver_.setOutput(true);

  esp_task_wdt_reset();
  driver_.setCurrent(+250);
  delay(300);
  esp_task_wdt_reset();
  driver_.setCurrent(-250);
  delay(300);
  esp_task_wdt_reset();
  driver_.setCurrent(+250);
  delay(300);
  esp_task_wdt_reset();
  driver_.setCurrent(0);

  state_.wiggle_running = false;

  float vin_v_after = driver_.getVinRaw() / 100.0f;
  Serial.printf("wiggle done. vin=%.2f V. Ready for calibrate or velocity.\n",
    vin_v_after);
}

// ---------- Setup / loop ----------
void setup() {
  // Arm task WDT FIRST. If any later step wedges (e.g. library call busy-waits
  // on a dead I2C slave), the chip auto-reboots after TASK_WDT_TIMEOUT_S. This
  // is the last-ditch safety net — every other robustness change is about
  // avoiding this reboot, but the WDT guarantees the device is never bricked.
  esp_task_wdt_init(TASK_WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(500);
  esp_task_wdt_reset();

  Serial.println();
  Serial.println("==========================================================");
  Serial.println(" Witness · 08_current_mode_host_loop (Path C · M3)");
  Serial.printf (" sketch=%s  tick=%u us (%.1f Hz)  lpf_tau=%.1f ms  alpha=%.4f\n",
    SKETCH_VERSION, (unsigned)TICK_PERIOD_US, 1e6f / (float)TICK_PERIOD_US,
    VEL_LPF_TAU_MS, VEL_LPF_ALPHA);
  Serial.println("==========================================================");

  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  Wire.setTimeOut(WIRE_TIMEOUT_MS);
  esp_task_wdt_reset();

  // Bounded handshake. driver_.begin() retries up to HANDSHAKE_RETRIES times
  // with HANDSHAKE_RETRY_GAP_MS between. Each endTransmission honors the
  // Wire.setTimeOut we set above, so total worst-case here ≈
  //   5 * (50ms timeout + 200ms gap) = 1.25 s. Comfortably under WDT window.
  bool roller_ok = driver_.begin();
  // The library's begin() re-initializes Wire internally; re-apply our timeout
  // so later hot-path calls are also bounded.
  Wire.setTimeOut(WIRE_TIMEOUT_MS);
  esp_task_wdt_reset();

  if (!roller_ok) {
    Serial.println("WARNING: roller not responding on I2C. CLI active; run 'reboot' or check hardware.");
  } else {
    Serial.printf("roller found at 0x64. fw_ver=%u\n", driver_.getFirmwareVersion());
    printPowerReport();

    // Init sequence — each call is guarded by driver_.online(). If any of
    // these were to hang the WDT catches it. In practice they won't: the
    // handshake just ACKed and Wire.setTimeOut bounds each call to 50 ms.
    driver_.setOutput(false); delay(30);
    driver_.setStall0();      delay(20);
    driver_.setMode(ROLLER_MODE_CURRENT); delay(30);
    driver_.setCurrent(0);    delay(20);
    driver_.setOutput(true);  delay(30);
    esp_task_wdt_reset();

    uint8_t m = driver_.getMotorMode();
    Serial.printf("mode readback: %u %s\n", m,
      (m == (uint8_t)ROLLER_MODE_CURRENT) ? "(MODE_CURRENT accepted)"
                                          : "(firmware did NOT accept MODE_CURRENT)");
  }

  // Seed state.
  state_ = ControlState{};
  state_.first_tick             = true;
  state_.boot_ms                = millis();
  state_.last_hz_sample_ms      = state_.boot_ms;
  state_.kp                     = KP_DEFAULT;
  state_.ki                     = KI_DEFAULT;
  state_.current_limit_mA       = CURRENT_LIMIT_MA_DEFAULT;
  state_.current_limit_user_mA  = CURRENT_LIMIT_MA_DEFAULT;
  state_.enabled                = false;
  state_.auto_kick_enabled      = true;
  state_.auto_kicks_fired       = 0;
  state_.stuck_accum_ms         = 0;
  state_.kick_duration_ms       = KICK_DURATION_MS;
  // Seed shared cooldown so the first kick (break-free or auto) is not blocked.
  state_.last_kick_end_ms       = 0;
  state_.lut_missing_warned     = false;
  // Power-on wiggle detector: initialized below after boot-time wiggle runs
  // (so the first loop() poll doesn't re-trigger on the same vin reading).
  state_.last_vin_ok            = false;
  state_.wiggle_running         = false;
  state_.last_vin_poll_ms       = 0;
  int32_t enc0 = driver_.readEncoder();
  state_.enc_steps = enc0;
  state_.enc_prev  = enc0;
  state_.ang_deg   = stepsToDeg(enc0);
  Serial.printf("initial encoder: %d steps (%.3f deg)\n", enc0, state_.ang_deg);

  pi_.setGains(KP_DEFAULT, KI_DEFAULT);
  pi_.setLimit(CURRENT_LIMIT_MA_DEFAULT);
  pi_.reset();

  // NVS: load active slot (default witness_02), try to load both directional
  // LUTs. If either is missing, or the meta version is not the current v2,
  // print the canonical "missing or outdated" line and keep both LUTs unloaded.
  // We never half-load one side: directional FF needs both or we're better off
  // falling back to PI-only during whichever direction is unbuilt.
  nvsLoadActiveSlot(current_slot_, sizeof(current_slot_));
  LutMeta meta{};
  if (nvsLoadLuts(current_slot_, lut_fwd_, lut_rev_, meta)) {
    float mnf, mxf, meanf, absmf; int absbf;
    float mnr, mxr, meanr, absmr; int absbr;
    lut_fwd_.stats(mnf, mxf, meanf, absmf, absbf);
    lut_rev_.stats(mnr, mxr, meanr, absmr, absbr);
    (void)meta;
    Serial.printf("LUTs loaded: slot=%s v=%u  age=n/a (no RTC)\n",
      current_slot_, (unsigned)meta.version);
    Serial.printf("  fwd: min=%+.2f mean=%+.2f max=%+.2f mA\n", mnf, meanf, mxf);
    Serial.printf("  rev: min=%+.2f mean=%+.2f max=%+.2f mA\n", mnr, meanr, mxr);
  } else {
    lut_fwd_.clear();
    lut_rev_.clear();
    Serial.printf("LUT missing or outdated (slot=%s). Run 'calibrate'.\n", current_slot_);
  }

  ctrl_.begin();
  cli_.begin();

  // Power-on wiggle: if 12 V PD is already live at end of setup, run the
  // proof-of-life wiggle so the first-power-up case confirms visually.
  // Then seed last_vin_ok so the 1 Hz detector in loop() doesn't re-trigger
  // on the same stable vin. Rising-edge retrigger only fires if vin first
  // drops below 6 V and then rises back above 10 V.
  if (driver_.online()) {
    float vin_v = driver_.getVinRaw() / 100.0f;
    if (vin_v >= 10.0f) {
      runPowerOnWiggle();
      state_.last_vin_ok = true;
    } else {
      // Below 10 V at boot — arm the detector. In the 6..10 V hysteresis
      // band we stay disarmed (false) so a clean rise to OK triggers.
      state_.last_vin_ok = false;
    }
  }
}

void loop() {
  // Kick the task WDT every cycle. Hot-path stays bounded even during an
  // 8-minute calibration sweep because ctrl_.service() returns after each
  // tick dispatch — the CAL FSM never blocks loop().
  esp_task_wdt_reset();

  ctrl_.service();
  cli_.service();

  // I2C health + non-blocking recovery. Probe runs only when online and at a
  // fixed 5 ms cadence; recovery state machine runs only when offline.
  i2cHealthProbe();
  i2cServiceRecovery();

  uint32_t now = millis();
  if (now - state_.last_hz_sample_ms >= 1000) {
    uint32_t dt_ms  = now - state_.last_hz_sample_ms;
    uint32_t d_ticks = state_.tick_count - state_.ticks_at_sample;
    state_.measured_hz = (dt_ms > 0) ? (1000.0f * (float)d_ticks / (float)dt_ms) : 0.0f;
    state_.ticks_at_sample    = state_.tick_count;
    state_.last_hz_sample_ms  = now;
  }

  // Power-on wiggle detector. Poll vin once per second (not at 200 Hz) to
  // avoid I2C spam. Hysteresis: rising edge at >=10 V, falling edge at <6 V.
  // Only fires on a clean LOW->OK transition when control is OFF, cal idle,
  // driver online, and no wiggle already running. This lets Edson unplug and
  // replug the PD to re-trigger without interrupting an active sweep.
  if (driver_.online() &&
      !state_.wiggle_running &&
      !state_.enabled &&
      !cal_.active() &&
      (now - state_.last_vin_poll_ms >= 1000)) {
    state_.last_vin_poll_ms = now;
    float vin_v = driver_.getVinRaw() / 100.0f;
    if (state_.last_vin_ok) {
      // Armed as OK; watch for falling edge to re-arm the detector.
      if (vin_v < 6.0f) {
        state_.last_vin_ok = false;
      }
    } else {
      // Armed as LOW; watch for rising edge above 10 V to fire wiggle.
      if (vin_v >= 10.0f) {
        runPowerOnWiggle();
        state_.last_vin_ok = true;
        Serial.print("> ");
      }
    }
  }

  delay(1);
}
