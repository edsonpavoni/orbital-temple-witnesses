/*
 * ============================================================================
 * ORBITAL TEMPLE - MOTION TEST (APRIL)
 * ============================================================================
 *
 * Forked from 03_motion_test. Identical control logic. Only additions:
 *   - Compressed delta log (machine-readable: timestamp, encoder pos, temp,
 *     voltage; only fields that changed since the last line are printed).
 *   - 'p' command also reports vin (V), encoder position (deg), temp (C).
 *
 * Simple serial-controlled motion test for developing smooth transitions.
 *
 * SERIAL COMMANDS:
 * ----------------
 * s<value>    Set target speed (RPM, can be negative)
 *             Example: s50 (50 RPM), s-30 (-30 RPM), s0 (stop)
 *
 * c<value>    Set max current/torque (mA)
 *             Example: c500 (500mA), c1000 (1000mA)
 *
 * t<value>    Set transition time (ms)
 *             Example: t2000 (2 second transitions)
 *
 * e<0-3>      Set easing mode
 *             0 = Linear
 *             1 = Ease-In-Out (smooth)
 *             2 = Ease-In (slow start)
 *             3 = Ease-Out (slow end)
 *
 * p           Print current settings
 *
 * x           Emergency stop (immediate)
 *
 * EXAMPLES:
 * ---------
 * t3000       Set 3 second transitions
 * e1          Use smooth ease-in-out
 * s200        Ramp to 200 RPM smoothly
 * s2          Slow down to 2 RPM (tracking speed)
 * s0          Stop
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8
#define I2C_SCL_PIN      9
#define I2C_FREQ         100000

// Register addresses
#define REG_OUTPUT       0x00
#define REG_MODE         0x01
#define REG_STALL_PROT   0x0F
#define REG_POS_MAXCUR   0x20   // Position mode max current
#define REG_VIN          0x34   // Supply voltage (int32, scaled x100, volts)
#define REG_TEMP         0x38   // Motor temperature (int32, degrees C)
#define REG_SPEED        0x40
#define REG_SPEED_MAXCUR 0x50
#define REG_SPEED_READ   0x60
#define REG_SPEED_PID    0x70   // Speed PID: 12 bytes (P, I, D as int32)
#define REG_POS          0x80   // Position target
#define REG_POS_READ     0x90   // Position readback (int32, scaled x100, deg)
#define REG_POS_PID      0xA0   // Position PID
#define REG_CURRENT      0xB0   // Direct current control
#define REG_CURRENT_READ 0xC0

#define MODE_SPEED       1
#define MODE_POSITION    2
#define MODE_CURRENT     3      // Direct current/torque mode

// =============================================================================
// EASING FUNCTIONS
// =============================================================================

enum EasingMode {
  EASE_LINEAR = 0,
  EASE_IN_OUT = 1,
  EASE_IN = 2,
  EASE_OUT = 3
};

// Easing function: takes progress 0.0-1.0, returns eased value 0.0-1.0
float applyEasing(float t, EasingMode mode) {
  switch (mode) {
    case EASE_LINEAR:
      return t;

    case EASE_IN_OUT:
      // Smooth S-curve (sine-based)
      return 0.5f * (1.0f - cos(t * PI));

    case EASE_IN:
      // Slow start, fast end (quadratic)
      return t * t;

    case EASE_OUT:
      // Fast start, slow end (quadratic)
      return 1.0f - (1.0f - t) * (1.0f - t);

    default:
      return t;
  }
}

const char* easingName(EasingMode mode) {
  switch (mode) {
    case EASE_LINEAR: return "LINEAR";
    case EASE_IN_OUT: return "EASE_IN_OUT";
    case EASE_IN: return "EASE_IN";
    case EASE_OUT: return "EASE_OUT";
    default: return "UNKNOWN";
  }
}

// =============================================================================
// GLOBAL STATE
// =============================================================================

// Motion parameters
float targetSpeedRPM = 0;
float currentSpeedRPM = 0;
float startSpeedRPM = 0;
int32_t maxCurrentMA = 1000;      // Tuned 2026-04-28: 1000mA gives torque
                                  // headroom to break static friction at 8 RPM
                                  // with the unbalanced pointer; thermals stable
                                  // at ~45 C for intermittent ritual cycles.
uint32_t transitionTimeMS = 2000; // Tuned 2026-04-28: 2 s transitions are slow
                                  // enough to read as graceful, fast enough to
                                  // not feel sluggish.
EasingMode easingMode = EASE_IN_OUT;  // Cosine S-curve — zero accel at endpoints.

// PID parameters (scaled by 100000 internally)
int32_t speedPID_P = 150000;  // Default P
int32_t speedPID_I = 15000;   // Default I
int32_t speedPID_D = 200000;  // Default D

// Control mode
bool usePositionMode = false;  // false = speed mode, true = position mode
float floatPosition = 0;       // Accumulated position in degrees (float for precision)
float positionIncrement = 0;   // Degrees per update cycle

// Transition state
bool inTransition = false;
unsigned long transitionStartTime = 0;

// =============================================================================
// MOTION STATE MACHINE
// =============================================================================
// Two top-level actions, each a sequence of phases on this single state
// machine:
//
//   r<deg>  — slow reposition to absolute physical angle:
//               REPOS_RAMP_UP → REPOS_CRUISE → REPOS_RAMP_DOWN → STILL_WAIT
//             Position-mode rate-integrated. Peak 4 RPM with 2 s eased ramps;
//             ramps shorten for short moves so they don't overshoot.
//
//   g       — 1080° spin that lands at the start angle:
//               STILL_WAIT → SPIN_RAMP_UP → SPIN_CRUISE → SPIN_BRAKE → SPIN_SETTLE
//             Position-mode rate-integrated cruise at 8 RPM (motor PID
//             enforces the setpoint, no gravity runaway). Brake = freeze
//             setpoint exactly at start + 1080°; the bumped position-PID
//             current ceiling halts the rotor on target.
//
//   q<deg>  — chains r<deg> → STILL_WAIT → spin into one command.
//
// STILL_WAIT requires the encoder to deviate by less than STILL_THRESH_DEG
// for a continuous STILL_HOLD_MS window before proceeding.
enum SpinPhase {
  SPIN_IDLE,
  REPOS_RAMP_UP,
  REPOS_CRUISE,
  REPOS_RAMP_DOWN,
  STILL_WAIT,
  SPIN_RAMP_UP,
  SPIN_CRUISE,
  SPIN_BRAKE,
  SPIN_SETTLE
};
SpinPhase g_spinPhase        = SPIN_IDLE;

// Spin (1080°) state
float     g_spinStartEnc     = 0.0f;
float     g_spinTargetEndEnc = 0.0f;
uint32_t  g_spinPhaseStartMS = 0;
float     g_spinBrakeEnc     = 0.0f;
float     g_spinBrakeRPM     = 0.0f;

// Reposition state
float     g_reposTargetSetpoint = 0.0f;
float     g_reposPeakRPM        = 0.0f;       // signed
uint32_t  g_savedTransitionMS   = 2000;
bool      g_pendingSpin         = false;      // chain a spin after stillness?

// Stillness check state
uint32_t  g_stillStartMS    = 0;
float     g_stillRefEnc     = 0.0f;

const float    SPIN_TOTAL_DEG    = 1080.0f;        // 3 full revolutions
const float    SPIN_CRUISE_RPM   = 8.0f;           // setpoint speed during cruise
int32_t        BRAKE_MAX_MA      = 1500;           // position-mode current ceiling
const uint32_t SETTLE_MS         = 2000;           // hold after brake before reporting
const int32_t  BRAKE_PID_P       = 30000000;       // production gains from 04
const int32_t  BRAKE_PID_I       = 1000;
const int32_t  BRAKE_PID_D       = 40000000;
const float    REPOS_PEAK_RPM    = 4.0f;           // peak speed during reposition
const float    STILL_THRESH_DEG  = 0.4f;           // max enc dev to count as "still"
const uint32_t STILL_HOLD_MS     = 2000;           // continuous stillness required

// Serial input buffer
String inputBuffer = "";

// =============================================================================
// FUNCTION PROTOTYPES
// =============================================================================

void motorInit();
void motorSetSpeed(float rpm);
void motorSetMaxCurrent(int32_t mA);
void motorEnable(bool enable);
float motorGetSpeed();
void motorSetSpeedPID(int32_t p, int32_t i, int32_t d);
void motorReadSpeedPID();
void motorSetPosition(int32_t pos);
int32_t motorGetPosition();
float motorGetVinV();
int32_t motorGetTempC();
float motorGetEncoderDeg();
void switchToSpeedMode();
void switchToPositionMode();
void emitLogDelta();

void writeReg8(uint8_t reg, uint8_t value);
void writeReg32(uint8_t reg, int32_t value);
int32_t readReg32(uint8_t reg);
void writeReg96(uint8_t reg, int32_t v1, int32_t v2, int32_t v3);
void readReg96(uint8_t reg, int32_t* v1, int32_t* v2, int32_t* v3);

void processSerialCommand(String cmd);
void startTransition(float newTarget);
void updateMotion();
void updateSpin();
void startSpin1080();
void startReposition(float deg, bool chainSpin);
void enterStillWait();
void enterSpinRampUp();
void printSettings();
void emergencyStop();

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("============================================");
  Serial.println("  ORBITAL TEMPLE - MOTION TEST");
  Serial.println("============================================");
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  s<rpm>   Set target speed (e.g., s50, s-30, s0)");
  Serial.println("  c<mA>    Set max current (e.g., c500, c1000)");
  Serial.println("  t<ms>    Set transition time (e.g., t3000)");
  Serial.println("  e<0-3>   Set easing (0=linear, 1=smooth, 2=ease-in, 3=ease-out)");
  Serial.println();
  Serial.println("  m        Toggle mode: SPEED <-> POSITION");
  Serial.println("           (Position mode = smoother at low speeds)");
  Serial.println();
  Serial.println("  kp<val>  Set PID P gain (e.g., kp100000)");
  Serial.println("  ki<val>  Set PID I gain (e.g., ki20000)");
  Serial.println("  kd<val>  Set PID D gain (e.g., kd150000)");
  Serial.println("  k        Read current PID values from motor");
  Serial.println("  p        Print settings");
  Serial.println("  r<deg>   Reposition slowly to absolute angle (0-359)");
  Serial.println("  g        Spin 1080 deg + brake at start angle (waits for still)");
  Serial.println("  q<deg>   Reposition -> still-check -> spin (one shot)");
  Serial.println("  B<mA>    Position-mode max current (brake authority)");
  Serial.println("  x        Emergency stop");
  Serial.println();

  // Initialize I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  // Check motor connection
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: Motor not found at 0x64!");
    Serial.println("Check wiring: SDA=D9(GPIO8), SCL=D10(GPIO9)");
    while (1) delay(1000);
  }

  Serial.println("Motor connected!");
  motorInit();

  // Read current PID values from motor
  Serial.println("Reading motor PID values...");
  motorReadSpeedPID();

  printSettings();
  Serial.println();
  Serial.println("# log schema (delta-encoded, fields omitted = unchanged):");
  Serial.println("#   t=ms  m=S|P  T=tgtRPM  c=cmdRPM  a=actRPM  p=encDeg");
  Serial.println("#   tr=transPct  tmp=tempC  v=vinV  mc=maxCurMA  tt=transMS");
  Serial.println("#   e=ease(0-3)  kP/kI/kD=PID");
  Serial.println("Ready. Enter commands:");
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  // Handle serial input
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processSerialCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }

  // Drive the 1080° spin-and-brake state machine (no-op when idle).
  updateSpin();

  // Update motion
  updateMotion();

  // Compressed delta log every 200ms (machine-readable for Claude)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    emitLogDelta();
  }

  delay(10);  // 100Hz update rate
}

// =============================================================================
// COMPRESSED DELTA LOG
// =============================================================================
//
// Prints one line per tick with key=value fields, comma-separated. A field is
// only emitted when its value changed since the previous line, so silence
// means "still the same as last time you saw it." Timestamp (t=) is always
// printed so each line has an anchor.
//
// Float change detection uses small epsilons to avoid noise spam on jittery
// readings (RPM, position, voltage). Integers (temp, percent, settings) use
// strict equality.

struct LogState {
  bool initialized = false;
  bool m_pos;
  float T;        // target RPM
  float c;        // commanded RPM
  float a;        // actual RPM (speed mode)
  float p;        // encoder position deg
  int   tr;       // transition pct (-1 = none)
  int   tmp;      // temp C
  float v;        // vin V
  int32_t mc;     // max current mA
  uint32_t tt;    // transition ms
  int   e;        // easing
  int32_t kP, kI, kD;
};
static LogState g_log;

static inline bool fchg(float a, float b, float eps) {
  return fabsf(a - b) >= eps;
}

void emitLogDelta() {
  // Snapshot current state
  bool m_pos = usePositionMode;
  float T = targetSpeedRPM;
  float c = currentSpeedRPM;
  float a = m_pos ? 0.0f : (motorGetSpeed() / 100.0f);
  float p = motorGetEncoderDeg();
  int tr = -1;
  if (inTransition) {
    float progress = (float)(millis() - transitionStartTime) / transitionTimeMS;
    if (progress < 0) progress = 0;
    if (progress > 1) progress = 1;
    tr = (int)(progress * 100);
  }
  int tmp = (int)motorGetTempC();
  float v = motorGetVinV();

  // Build delta payload first; only emit a line if at least one field changed.
  // Timestamp alone is suppressed (silence == "everything still the same").
  // Per-field baseline is updated ONLY when that field actually prints, so
  // sub-epsilon drift accumulates against the last logged value.
  String delta;
  delta.reserve(96);
  bool first = !g_log.initialized;

  if (first || g_log.m_pos != m_pos) {
    delta += ",m="; delta += (m_pos ? 'P' : 'S');
    g_log.m_pos = m_pos;
  }
  if (first || fchg(g_log.T, T, 0.05f)) {
    delta += ",T="; delta += String(T, 1);
    g_log.T = T;
  }
  if (first || fchg(g_log.c, c, 0.05f)) {
    delta += ",c="; delta += String(c, 1);
    g_log.c = c;
  }
  if (!m_pos && (first || fchg(g_log.a, a, 0.1f))) {
    delta += ",a="; delta += String(a, 1);
    g_log.a = a;
  }
  if (first || fchg(g_log.p, p, 0.1f)) {
    delta += ",p="; delta += String(p, 1);
    g_log.p = p;
  }
  if (first || g_log.tr != tr) {
    if (tr < 0) delta += ",tr=-";
    else { delta += ",tr="; delta += String(tr); }
    g_log.tr = tr;
  }
  // Temperature: only emit while the motor is moving or trying to move
  // (commanded RPM != 0, or target != 0, or mid-transition). On the very
  // first line we always include it as a baseline anchor.
  bool motorActive = (currentSpeedRPM != 0.0f) || (targetSpeedRPM != 0.0f) || inTransition;
  if (first || (motorActive && g_log.tmp != tmp)) {
    delta += ",tmp="; delta += String(tmp);
    g_log.tmp = tmp;
  }
  if (first || fchg(g_log.v, v, 0.05f)) {
    delta += ",v="; delta += String(v, 2);
    g_log.v = v;
  }
  if (first || g_log.mc != maxCurrentMA) {
    delta += ",mc="; delta += String(maxCurrentMA);
    g_log.mc = maxCurrentMA;
  }
  if (first || g_log.tt != transitionTimeMS) {
    delta += ",tt="; delta += String(transitionTimeMS);
    g_log.tt = transitionTimeMS;
  }
  if (first || g_log.e != (int)easingMode) {
    delta += ",e="; delta += String((int)easingMode);
    g_log.e = (int)easingMode;
  }
  if (first || g_log.kP != speedPID_P) {
    delta += ",kP="; delta += String(speedPID_P);
    g_log.kP = speedPID_P;
  }
  if (first || g_log.kI != speedPID_I) {
    delta += ",kI="; delta += String(speedPID_I);
    g_log.kI = speedPID_I;
  }
  if (first || g_log.kD != speedPID_D) {
    delta += ",kD="; delta += String(speedPID_D);
    g_log.kD = speedPID_D;
  }

  g_log.initialized = true;

  if (delta.length() > 0) {
    Serial.print("t="); Serial.print(millis());
    Serial.println(delta);
  }
}

// =============================================================================
// SERIAL COMMAND PROCESSING
// =============================================================================

void processSerialCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  // Check for two-character commands first (kp, ki, kd)
  if (cmd.length() >= 2) {
    String prefix = cmd.substring(0, 2);
    prefix.toLowerCase();

    if (prefix == "kp") {
      int32_t val = cmd.substring(2).toInt();
      speedPID_P = val;
      motorSetSpeedPID(speedPID_P, speedPID_I, speedPID_D);
      Serial.print(">> PID P set to: ");
      Serial.println(speedPID_P);
      return;
    }
    if (prefix == "ki") {
      int32_t val = cmd.substring(2).toInt();
      speedPID_I = val;
      motorSetSpeedPID(speedPID_P, speedPID_I, speedPID_D);
      Serial.print(">> PID I set to: ");
      Serial.println(speedPID_I);
      return;
    }
    if (prefix == "kd") {
      int32_t val = cmd.substring(2).toInt();
      speedPID_D = val;
      motorSetSpeedPID(speedPID_P, speedPID_I, speedPID_D);
      Serial.print(">> PID D set to: ");
      Serial.println(speedPID_D);
      return;
    }
  }

  char cmdType = cmd.charAt(0);
  String value = cmd.substring(1);

  switch (cmdType) {
    case 's':
    case 'S': {
      // Set target speed
      float newSpeed = value.toFloat();
      Serial.print(">> Setting target speed: ");
      Serial.print(newSpeed);
      Serial.println(" RPM");
      startTransition(newSpeed);
      break;
    }

    case 'c':
    case 'C': {
      // Set max current
      int32_t newCurrent = value.toInt();
      if (newCurrent < 100) newCurrent = 100;
      if (newCurrent > 2000) newCurrent = 2000;
      maxCurrentMA = newCurrent;
      motorSetMaxCurrent(maxCurrentMA);
      Serial.print(">> Max current set to: ");
      Serial.print(maxCurrentMA);
      Serial.println(" mA");
      break;
    }

    case 't':
    case 'T': {
      // Set transition time
      uint32_t newTime = value.toInt();
      if (newTime < 100) newTime = 100;
      if (newTime > 30000) newTime = 30000;
      transitionTimeMS = newTime;
      Serial.print(">> Transition time set to: ");
      Serial.print(transitionTimeMS);
      Serial.println(" ms");
      break;
    }

    case 'e':
    case 'E': {
      // Set easing mode
      int mode = value.toInt();
      if (mode >= 0 && mode <= 3) {
        easingMode = (EasingMode)mode;
        Serial.print(">> Easing mode set to: ");
        Serial.println(easingName(easingMode));
      } else {
        Serial.println(">> Invalid easing mode (0-3)");
      }
      break;
    }

    case 'm':
    case 'M':
      // Toggle control mode
      if (usePositionMode) {
        switchToSpeedMode();
        Serial.println(">> Switched to SPEED MODE");
      } else {
        switchToPositionMode();
        Serial.println(">> Switched to POSITION MODE (smoother)");
      }
      break;

    case 'k':
    case 'K':
      // Read PID from motor (was 'r' in 03; moved so 'r' can mean reposition).
      motorReadSpeedPID();
      Serial.println(">> PID values read from motor");
      printSettings();
      break;

    case 'p':
    case 'P':
      printSettings();
      break;

    case 'g':
    case 'G':
      // Spin in place: 1080° rotation, brakes at the start angle.
      // Requires 2 s of stillness before motion begins.
      startSpin1080();
      break;

    case 'r':
    case 'R':
      // Slow eased reposition to an absolute physical angle (degrees).
      startReposition(value.toFloat(), /*chainSpin=*/false);
      break;

    case 'q':
    case 'Q':
      // Reposition → 2 s stillness check → spin, in one command.
      startReposition(value.toFloat(), /*chainSpin=*/true);
      break;

    case 'B':
      // Set position-mode max current (used as the spin's brake ceiling).
      BRAKE_MAX_MA = value.toInt();
      if (BRAKE_MAX_MA < 100)  BRAKE_MAX_MA = 100;
      if (BRAKE_MAX_MA > 2500) BRAKE_MAX_MA = 2500;
      Serial.print(">> BRAKE_MAX_MA = ");
      Serial.println(BRAKE_MAX_MA);
      break;

    case 'x':
    case 'X':
      emergencyStop();
      break;

    default:
      Serial.print(">> Unknown command: ");
      Serial.println(cmd);
      break;
  }
}

// =============================================================================
// MOTION CONTROL
// =============================================================================

void startTransition(float newTarget) {
  if (newTarget == targetSpeedRPM && !inTransition) {
    return;  // Already at target
  }

  startSpeedRPM = currentSpeedRPM;
  targetSpeedRPM = newTarget;
  transitionStartTime = millis();
  inTransition = true;
}

void updateMotion() {
  if (inTransition) {
    unsigned long elapsed = millis() - transitionStartTime;

    if (elapsed >= transitionTimeMS) {
      // Transition complete
      currentSpeedRPM = targetSpeedRPM;
      inTransition = false;
    } else {
      // Calculate progress with easing
      float progress = (float)elapsed / transitionTimeMS;
      float easedProgress = applyEasing(progress, easingMode);

      // Interpolate speed
      currentSpeedRPM = startSpeedRPM + (targetSpeedRPM - startSpeedRPM) * easedProgress;
    }
  }

  if (usePositionMode) {
    // Position mode: we increment position ourselves for smooth motion
    // Convert RPM to position increment per loop iteration (10ms loop = 100Hz)
    // RPM * 360 degrees / 60 seconds / 100 Hz = degrees per iteration
    positionIncrement = currentSpeedRPM * 360.0f / 60.0f / 100.0f;

    // Accumulate position (using float for sub-degree precision)
    floatPosition += positionIncrement;

    // Convert to motor position units (assuming 1 unit = 0.01 degrees based on scaling)
    // The motor uses int32 position, scaled by 100
    int32_t targetPos = (int32_t)(floatPosition * 100.0f);

    motorSetPosition(targetPos);
  } else {
    // Speed mode: send speed command directly
    motorSetSpeed(currentSpeedRPM);
  }
}

void emergencyStop() {
  Serial.println(">> EMERGENCY STOP");
  inTransition = false;
  targetSpeedRPM = 0;
  currentSpeedRPM = 0;
  g_spinPhase = SPIN_IDLE;
  motorSetSpeed(0);
}

// =============================================================================
// 1080° SPIN-AND-BRAKE
// =============================================================================

void startSpin1080() {
  if (g_spinPhase != SPIN_IDLE) {
    Serial.println(">> Already busy. Use 'x' to abort.");
    return;
  }
  // Production position PID. Brake PID == cruise PID; one set, used throughout.
  writeReg96(REG_POS_PID, BRAKE_PID_P, BRAKE_PID_I, BRAKE_PID_D);
  delay(20);
  if (!usePositionMode) {
    switchToPositionMode();
    delay(300);
  }
  writeReg32(REG_POS_MAXCUR, BRAKE_MAX_MA * 100);
  delay(10);

  // Stillness gate: don't spin until the rotor has been stationary for 2 s.
  g_pendingSpin = true;
  enterStillWait();
}

void enterStillWait() {
  g_stillRefEnc      = motorGetEncoderDeg();
  g_stillStartMS     = millis();
  g_spinPhase        = STILL_WAIT;
  g_spinPhaseStartMS = millis();
  Serial.print(">> Stillness check (need ");
  Serial.print(STILL_HOLD_MS / 1000);
  Serial.println(" s static)...");
}

void enterSpinRampUp() {
  g_spinStartEnc     = floatPosition;
  g_spinTargetEndEnc = floatPosition + SPIN_TOTAL_DEG;
  Serial.print(">> SPIN1080 start=");
  Serial.print(g_spinStartEnc, 2);
  Serial.print(" target=");
  Serial.println(g_spinTargetEndEnc, 2);
  startTransition(SPIN_CRUISE_RPM);
  g_spinPhase        = SPIN_RAMP_UP;
  g_spinPhaseStartMS = millis();
}

void startReposition(float deg, bool chainSpin) {
  if (g_spinPhase != SPIN_IDLE) {
    Serial.println(">> Already busy. Use 'x' to abort.");
    return;
  }
  // Position-mode rate-integration drives the move. PID + max current set.
  writeReg96(REG_POS_PID, BRAKE_PID_P, BRAKE_PID_I, BRAKE_PID_D);
  delay(20);
  if (!usePositionMode) {
    switchToPositionMode();
    delay(300);
  }
  writeReg32(REG_POS_MAXCUR, BRAKE_MAX_MA * 100);
  delay(10);

  while (deg < 0)        deg += 360.0f;
  while (deg >= 360.0f)  deg -= 360.0f;
  float curMod = fmodf(floatPosition, 360.0f);
  if (curMod < 0) curMod += 360.0f;
  float delta = deg - curMod;
  if (delta > 180.0f)       delta -= 360.0f;
  else if (delta < -180.0f) delta += 360.0f;

  if (fabsf(delta) < 0.5f) {
    Serial.print(">> REPOS already at ");
    Serial.print(deg, 1);
    Serial.println(" deg");
    g_pendingSpin = chainSpin;
    enterStillWait();
    return;
  }

  g_reposTargetSetpoint = floatPosition + delta;
  float dist = fabsf(delta);
  float sign = (delta > 0) ? 1.0f : -1.0f;

  // Cosine 0 → peak → 0 over (2 × T_ramp) integrates to peak × 6 × T_ramp deg.
  // For long moves: peak = REPOS_PEAK_RPM, fixed 2 s ramps + cruise covers rest.
  // For short moves (dist < 48 deg at peak=4): shorten T_ramp so ramps cover dist.
  uint32_t rampMs = 2000;
  if (dist < 48.0f) {
    rampMs = (uint32_t)((dist / 48.0f) * 2000.0f);
    if (rampMs < 200) rampMs = 200;
  }
  g_reposPeakRPM      = REPOS_PEAK_RPM * sign;
  g_pendingSpin       = chainSpin;
  g_savedTransitionMS = transitionTimeMS;
  transitionTimeMS    = rampMs;

  Serial.print(">> REPOS to phys=");
  Serial.print(deg, 1);
  Serial.print("  delta=");
  Serial.print(delta, 1);
  Serial.print("  peak=");
  Serial.print(g_reposPeakRPM, 1);
  Serial.print(" RPM  rampMs=");
  Serial.println(rampMs);

  startTransition(g_reposPeakRPM);
  g_spinPhase        = REPOS_RAMP_UP;
  g_spinPhaseStartMS = millis();
}

void updateSpin() {
  if (g_spinPhase == SPIN_IDLE) return;

  float setpointMoved = floatPosition - g_spinStartEnc;

  switch (g_spinPhase) {
    case REPOS_RAMP_UP:
      if (!inTransition) {
        g_spinPhase        = REPOS_CRUISE;
        g_spinPhaseStartMS = millis();
      }
      break;

    case REPOS_CRUISE: {
      // Trigger ramp-down when remaining distance equals what the ramp-down
      // will cover (avg 0.5 × peak × 6 deg/s × rampMs/1000).
      float rampTravel = 3.0f * fabsf(g_reposPeakRPM) *
                         (transitionTimeMS / 1000.0f);
      float remaining = g_reposTargetSetpoint - floatPosition;
      bool reachedDir = (g_reposPeakRPM > 0) ? (remaining <= rampTravel)
                                              : (remaining >= -rampTravel);
      if (reachedDir) {
        startTransition(0.0f);
        g_spinPhase        = REPOS_RAMP_DOWN;
        g_spinPhaseStartMS = millis();
      }
      break;
    }

    case REPOS_RAMP_DOWN:
      if (!inTransition) {
        // Snap setpoint exactly to target (absorb integration drift).
        floatPosition = g_reposTargetSetpoint;
        motorSetPosition((int32_t)(g_reposTargetSetpoint * 100.0f));
        transitionTimeMS = g_savedTransitionMS;
        Serial.print(">> REPOS done  setpoint=");
        Serial.println(floatPosition, 2);
        enterStillWait();
      }
      break;

    case STILL_WAIT: {
      float curEnc = motorGetEncoderDeg();
      if (fabsf(curEnc - g_stillRefEnc) > STILL_THRESH_DEG) {
        // Movement detected; reset the still timer.
        g_stillRefEnc  = curEnc;
        g_stillStartMS = millis();
      } else if (millis() - g_stillStartMS >= STILL_HOLD_MS) {
        Serial.print(">> Still at p=");
        Serial.print(curEnc, 2);
        if (g_pendingSpin) {
          Serial.println(" - starting spin");
          g_pendingSpin = false;
          enterSpinRampUp();
        } else {
          Serial.println(" - idle");
          g_spinPhase = SPIN_IDLE;
        }
      }
      break;
    }

    case SPIN_RAMP_UP:
      // Wait for currentSpeedRPM to reach 8 (transition complete). The setpoint
      // (floatPosition) has been advancing eased during this phase.
      if (!inTransition) {
        Serial.print(">> SPIN cruise  setpoint=");
        Serial.println(floatPosition, 1);
        g_spinPhase        = SPIN_CRUISE;
        g_spinPhaseStartMS = millis();
      }
      break;

    case SPIN_CRUISE:
      // Setpoint advances at exactly 8 RPM = 48 deg/s. Cruise terminates when
      // the setpoint has moved 1080° from start. Then we BRAKE: freeze the
      // setpoint exactly at start + 1080° and zero the velocity instantly.
      // The motor was tracking 8 RPM; with the setpoint suddenly stopped, the
      // position PID outputs reverse current to halt the rotor and pull it
      // back to the target.
      if (setpointMoved >= SPIN_TOTAL_DEG) {
        currentSpeedRPM = 0;
        targetSpeedRPM  = 0;
        inTransition    = false;
        floatPosition   = g_spinTargetEndEnc;
        motorSetPosition((int32_t)(g_spinTargetEndEnc * 100.0f));

        g_spinBrakeEnc = motorGetEncoderDeg();
        g_spinBrakeRPM = motorGetSpeed() / 100.0f;
        g_spinPhase        = SPIN_BRAKE;
        g_spinPhaseStartMS = millis();

        Serial.print(">> BRAKE  enc=");
        Serial.print(g_spinBrakeEnc, 1);
        Serial.print("  rpm=");
        Serial.print(g_spinBrakeRPM, 1);
        Serial.print("  setpoint frozen at ");
        Serial.println(g_spinTargetEndEnc, 1);
      }
      break;

    case SPIN_BRAKE:
      // Small grace window for the brake to grab; the position PID does the
      // physical work. Move on to SETTLE for the longer hold.
      if (millis() - g_spinPhaseStartMS >= 300) {
        g_spinPhase        = SPIN_SETTLE;
        g_spinPhaseStartMS = millis();
      }
      break;

    case SPIN_SETTLE:
      if (millis() - g_spinPhaseStartMS >= SETTLE_MS) {
        float finalEnc = motorGetEncoderDeg();
        float err      = finalEnc - g_spinTargetEndEnc;
        float overshoot = g_spinBrakeEnc - g_spinTargetEndEnc;
        float endPhys  = fmodf(finalEnc - g_spinStartEnc, 360.0f);
        if (endPhys < 0) endPhys += 360.0f;
        Serial.print(">> SPIN done.  brakeRPM=");
        Serial.print(g_spinBrakeRPM, 1);
        Serial.print("  brakeOver=");
        Serial.print(overshoot, 1);
        Serial.print(" deg  final=");
        Serial.print(finalEnc, 2);
        Serial.print("  err=");
        Serial.print(err, 2);
        Serial.print("  phys-from-start=");
        Serial.println(endPhys, 2);
        g_spinPhase = SPIN_IDLE;
      }
      break;

    case SPIN_IDLE:
      break;
  }
}

void printSettings() {
  Serial.println();
  Serial.println("=== Current Settings ===");
  Serial.print("Control Mode:    ");
  Serial.println(usePositionMode ? "POSITION (smooth)" : "SPEED");
  Serial.print("Target Speed:    ");
  Serial.print(targetSpeedRPM);
  Serial.println(" RPM");
  Serial.print("Max Current:     ");
  Serial.print(maxCurrentMA);
  Serial.println(" mA");
  Serial.print("Transition Time: ");
  Serial.print(transitionTimeMS);
  Serial.println(" ms");
  Serial.print("Easing Mode:     ");
  Serial.println(easingName(easingMode));
  Serial.println("--- Speed PID ---");
  Serial.print("P (kp): ");
  Serial.println(speedPID_P);
  Serial.print("I (ki): ");
  Serial.println(speedPID_I);
  Serial.print("D (kd): ");
  Serial.println(speedPID_D);
  Serial.println("--- Live ---");
  Serial.print("Vin:             ");
  Serial.print(motorGetVinV(), 2);
  Serial.println(" V");
  Serial.print("Enc Position:    ");
  Serial.print(motorGetEncoderDeg(), 2);
  Serial.println(" deg");
  Serial.print("Motor Temp:      ");
  Serial.print(motorGetTempC());
  Serial.println(" C");
  Serial.println("========================");
}

// =============================================================================
// MOTOR FUNCTIONS
// =============================================================================

void motorInit() {
  // Disable output during setup
  writeReg8(REG_OUTPUT, 0);
  delay(50);

  // Disable stall protection
  writeReg8(REG_STALL_PROT, 0);
  delay(50);

  // Set speed mode
  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);

  // Set initial max current
  motorSetMaxCurrent(maxCurrentMA);
  delay(50);

  // Start at zero speed
  motorSetSpeed(0);
  delay(50);

  // Enable output
  writeReg8(REG_OUTPUT, 1);
  delay(50);
}

void motorSetSpeed(float rpm) {
  // Convert RPM to scaled value (multiply by 100)
  int32_t scaledSpeed = (int32_t)(rpm * 100.0f);
  writeReg32(REG_SPEED, scaledSpeed);
}

void motorSetMaxCurrent(int32_t mA) {
  // Convert mA to scaled value (multiply by 100)
  int32_t scaledCurrent = mA * 100;
  writeReg32(REG_SPEED_MAXCUR, scaledCurrent);
}

void motorEnable(bool enable) {
  writeReg8(REG_OUTPUT, enable ? 1 : 0);
}

float motorGetSpeed() {
  return (float)readReg32(REG_SPEED_READ);
}

void motorSetSpeedPID(int32_t p, int32_t i, int32_t d) {
  writeReg96(REG_SPEED_PID, p, i, d);
  Serial.print(">> PID written: P=");
  Serial.print(p);
  Serial.print(" I=");
  Serial.print(i);
  Serial.print(" D=");
  Serial.println(d);
}

void motorReadSpeedPID() {
  readReg96(REG_SPEED_PID, &speedPID_P, &speedPID_I, &speedPID_D);
  Serial.print(">> PID from motor: P=");
  Serial.print(speedPID_P);
  Serial.print(" I=");
  Serial.print(speedPID_I);
  Serial.print(" D=");
  Serial.println(speedPID_D);
}

void motorSetPosition(int32_t pos) {
  writeReg32(REG_POS, pos);
}

int32_t motorGetPosition() {
  return readReg32(REG_POS_READ);
}

float motorGetVinV() {
  return readReg32(REG_VIN) / 100.0f;
}

int32_t motorGetTempC() {
  return readReg32(REG_TEMP);
}

float motorGetEncoderDeg() {
  return readReg32(REG_POS_READ) / 100.0f;
}

void switchToSpeedMode() {
  motorEnable(false);
  delay(50);
  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);
  writeReg32(REG_SPEED_MAXCUR, maxCurrentMA * 100);
  delay(50);
  motorSetSpeed(0);
  delay(50);
  motorEnable(true);
  usePositionMode = false;
  currentSpeedRPM = 0;
  targetSpeedRPM = 0;
}

void switchToPositionMode() {
  motorEnable(false);
  delay(50);
  writeReg8(REG_MODE, MODE_POSITION);
  delay(50);
  writeReg32(REG_POS_MAXCUR, maxCurrentMA * 100);
  delay(50);
  // Read current position and use as starting point
  int32_t currentPos = motorGetPosition();
  floatPosition = currentPos / 100.0f;  // Convert to degrees
  motorSetPosition(currentPos);
  delay(50);
  motorEnable(true);
  usePositionMode = true;
  currentSpeedRPM = 0;
  targetSpeedRPM = 0;
  Serial.print(">> Starting position: ");
  Serial.print(floatPosition);
  Serial.println(" degrees");
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

void writeReg96(uint8_t reg, int32_t v1, int32_t v2, int32_t v3) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  // Write 3 x 32-bit values (12 bytes total)
  Wire.write((uint8_t)(v1 & 0xFF));
  Wire.write((uint8_t)((v1 >> 8) & 0xFF));
  Wire.write((uint8_t)((v1 >> 16) & 0xFF));
  Wire.write((uint8_t)((v1 >> 24) & 0xFF));
  Wire.write((uint8_t)(v2 & 0xFF));
  Wire.write((uint8_t)((v2 >> 8) & 0xFF));
  Wire.write((uint8_t)((v2 >> 16) & 0xFF));
  Wire.write((uint8_t)((v2 >> 24) & 0xFF));
  Wire.write((uint8_t)(v3 & 0xFF));
  Wire.write((uint8_t)((v3 >> 8) & 0xFF));
  Wire.write((uint8_t)((v3 >> 16) & 0xFF));
  Wire.write((uint8_t)((v3 >> 24) & 0xFF));
  Wire.endTransmission();
}

void readReg96(uint8_t reg, int32_t* v1, int32_t* v2, int32_t* v3) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)ROLLER_I2C_ADDR, (uint8_t)12);

  if (Wire.available() >= 12) {
    *v1 = Wire.read();
    *v1 |= (int32_t)Wire.read() << 8;
    *v1 |= (int32_t)Wire.read() << 16;
    *v1 |= (int32_t)Wire.read() << 24;

    *v2 = Wire.read();
    *v2 |= (int32_t)Wire.read() << 8;
    *v2 |= (int32_t)Wire.read() << 16;
    *v2 |= (int32_t)Wire.read() << 24;

    *v3 = Wire.read();
    *v3 |= (int32_t)Wire.read() << 8;
    *v3 |= (int32_t)Wire.read() << 16;
    *v3 |= (int32_t)Wire.read() << 24;
  }
}
