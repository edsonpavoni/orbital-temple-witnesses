/*
 * ============================================================================
 * ORBITAL TEMPLE - 09_speed_brake
 * ============================================================================
 *
 * Speed-mode cruise + position-mode brake hybrid for 1800 deg spin.
 *
 * Key difference from 07_smooth_1800:
 *   07 runs the ENTIRE journey in position mode. The setpoint integrates
 *   deterministically, so return accuracy is guaranteed but the motor never
 *   truly fights gravity -- the position PID counters any lean.
 *
 *   09 runs RAMP_UP and CRUISE in speed mode, so gravity acts freely on the
 *   rotor the whole time (this is the "gravity-alive cruise"). Only the final
 *   BRAKE phase switches to position mode for precision stop. The question
 *   this test answers: can a late position-mode brake absorb the accumulated
 *   displacement and still land within acceptable error?
 *
 * State machine for 'g' command:
 *   SPIN_RAMP_UP  -- speed mode, cosine ease 0 -> 8 RPM over 2000 ms.
 *                    Write REG_SPEED every tick. Poll encoder from
 *                    g_spinStartEnc to track cumulative displacement.
 *   SPIN_CRUISE   -- speed mode, hold 8 RPM. Continue polling encoder.
 *                    Transition when (curEnc - g_spinStartEnc) >= BRAKE_TRIGGER.
 *   SPIN_BRAKE    -- inline: write PID -> switch mode to POSITION -> write
 *                    brake max-current -> freeze setpoint at
 *                    g_spinStartEnc + SPIN_TOTAL_DEG. No motorEnable toggle.
 *   SPIN_SETTLE   -- hold 5 s, then print final report.
 *
 * Phase markers (SB prefix = Speed-Brake, distinguishable from 07's SPIN):
 *   >> SPIN_SB start enc=X.XX deg target=Y.YY deg
 *   >> SPIN_SB cruise (encMoved=X.XX)
 *   >> SPIN_SB brake fire (encMoved=X.XX, target=Y.YY, lead=Z.ZZ)
 *   >> SPIN_SB lock setpoint=Y.YY
 *   >> SPIN_SB done. final=A.AA  err=B.BB deg  phys-offset-from-start=C.CC deg
 *
 * CRITICAL DETAIL - brake register order:
 *   Write PID first, then mode, then max-current, then setpoint.
 *   Switching mode before PID is loaded causes the position loop to use
 *   stale (potentially zero) gains for the first control cycles.
 *
 * TEST PROCEDURE (identical workflow to 07):
 *   1. Press 'f'  -> motor releases, pointer falls to gravity rest (X).
 *   2. Wait a few seconds for the pointer to fully settle.
 *   3. Press 'g'  -> speed-mode 1800 deg spin with position-mode brake at end.
 *   4. Read final report: phys-offset-from-start shows landing accuracy.
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
 * f           Release motor (free-spin, no holding torque).
 *             Pointer falls to gravity rest. Use before 'g' to define X.
 *             Aborts any in-progress spin cleanly before releasing.
 *
 * g           Speed-mode 1800 deg spin (gravity-alive cruise),
 *             position-mode brake at target. Lock at start angle.
 *
 * j<deg>      Jog by offset (e.g., j90 = move +90 deg from current and hold).
 *             Use after 'f' to position pointer before 'g'.
 *
 * p           Print current settings
 *
 * x           Emergency stop (immediate)
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
#define MODE_CURRENT     3

// =============================================================================
// EASING FUNCTIONS
// =============================================================================

enum EasingMode {
  EASE_LINEAR = 0,
  EASE_IN_OUT = 1,
  EASE_IN = 2,
  EASE_OUT = 3
};

float applyEasing(float t, EasingMode mode) {
  switch (mode) {
    case EASE_LINEAR:  return t;
    case EASE_IN_OUT:  return 0.5f * (1.0f - cos(t * PI));
    case EASE_IN:      return t * t;
    case EASE_OUT:     return 1.0f - (1.0f - t) * (1.0f - t);
    default:           return t;
  }
}

const char* easingName(EasingMode mode) {
  switch (mode) {
    case EASE_LINEAR:  return "LINEAR";
    case EASE_IN_OUT:  return "EASE_IN_OUT";
    case EASE_IN:      return "EASE_IN";
    case EASE_OUT:     return "EASE_OUT";
    default:           return "UNKNOWN";
  }
}

// =============================================================================
// GLOBAL STATE
// =============================================================================

float targetSpeedRPM = 0;
float currentSpeedRPM = 0;
float startSpeedRPM = 0;
int32_t maxCurrentMA = 1000;
uint32_t transitionTimeMS = 2000;
EasingMode easingMode = EASE_IN_OUT;

int32_t speedPID_P = 150000;
int32_t speedPID_I = 15000;
int32_t speedPID_D = 200000;

// Position mode bookkeeping (used only during BRAKE/SETTLE via updateMotion)
bool usePositionMode = false;
float floatPosition = 0;
float positionIncrement = 0;

// Transition state
bool inTransition = false;
unsigned long transitionStartTime = 0;

// =============================================================================
// 09 SPEED-BRAKE SPIN STATE MACHINE
// =============================================================================
//
// RAMP_UP + CRUISE run in speed mode.
//   - 'g' captures the real encoder reading as g_spinStartEnc.
//   - updateMotion() integrates currentSpeedRPM and writes REG_SPEED each tick.
//   - Displacement tracking: (motorGetEncoderDeg() - g_spinStartEnc).
//     The encoder reports unwrapped multi-turn positions (confirmed: prior run
//     delta = 1802.8 deg as a single linear value), so plain subtraction works.
//
// BRAKE fires when displacement >= (SPIN_TOTAL_DEG - BRAKE_LEAD_DEG).
//   - Register write order: PID first, then mode, then max-current, then setpoint.
//     This guarantees the position loop starts with correct gains.
//   - No motorEnable toggle during the switch -- rotor never coasts.
//   - Setpoint frozen at exactly g_spinStartEnc + SPIN_TOTAL_DEG.
//
// SETTLE holds 5 s then prints the final report.

enum SpinPhase {
  SPIN_IDLE,
  SPIN_RAMP_UP,
  SPIN_CRUISE,
  SPIN_BRAKE,
  SPIN_SETTLE
};

SpinPhase g_spinPhase        = SPIN_IDLE;
float     g_spinStartEnc     = 0.0f;  // real encoder deg at 'g' press
float     g_spinTargetEnc    = 0.0f;  // g_spinStartEnc + SPIN_TOTAL_DEG
uint32_t  g_spinPhaseStartMS = 0;

// =============================================================================
// JOG STATE MACHINE
// =============================================================================
//
// JOG_SEEK  -- position mode is engaged with the target setpoint. Polls encoder
//              each tick. Transitions to JOG_SETTLE when |cur - target| <= 1.0 deg.
//              Timeout: 5 s. If elapsed without entering the window, prints
//              a timeout message and falls to JOG_IDLE.
//
// JOG_SETTLE -- confirms stability using an anchor-reset sliding window.
//              Each tick: if |cur - anchor| > 0.3 deg, reset anchor+timestamp.
//              Settled when anchor holds for >= 1500 ms continuously.
//              Overall timeout: 8 s from JOG_SETTLE entry. Falls to JOG_IDLE
//              on either outcome.
//
// JOG_IDLE  -- no state-machine activity. Position-mode hold remains active
//              (floatPosition + usePositionMode keep the rotor at the target).
//              The next command (g, f, etc.) can fire immediately.

enum JogPhase {
  JOG_IDLE,
  JOG_SEEK,
  JOG_SETTLE
};

JogPhase g_jogPhase             = JOG_IDLE;
float    g_jogTargetEnc         = 0.0f;   // absolute target in encoder deg
uint32_t g_jogPhaseStartMS      = 0;      // entry time of current jog phase
float    g_jogAnchorEnc         = 0.0f;   // stability window anchor reading
uint32_t g_jogAnchorMS          = 0;      // timestamp when anchor was last reset

// Tunable jog constants
const float    JOG_SEEK_TOL_DEG   = 1.0f;    // window to enter JOG_SETTLE (deg)
const float    JOG_SETTLE_TOL_DEG = 0.3f;    // stability band around anchor (deg)
const uint32_t JOG_SETTLE_WIN_MS  = 1500;    // anchor must hold this long to declare settled
const uint32_t JOG_SEEK_TIMEOUT_MS   = 5000; // SEEK phase hard timeout (ms)
const uint32_t JOG_SETTLE_TIMEOUT_MS = 8000; // SETTLE phase hard timeout (ms)

// Tunable constants
const float    SPIN_TOTAL_DEG  = 1800.0f;
const float    SPIN_CRUISE_RPM = 8.0f;
const uint32_t SPIN_RAMP_MS    = 2000;
// Brake fires BRAKE_LEAD_DEG before the target so the position PID has room
// to absorb momentum. Start with 3 deg; tune from logs.
const float    BRAKE_LEAD_DEG  = 3.0f;
// Threshold at which the CRUISE -> BRAKE transition fires
const float    BRAKE_TRIGGER   = SPIN_TOTAL_DEG - BRAKE_LEAD_DEG;  // 1797.0 deg
// Position PID gains (validated by 04_position_lab, same as 08_brake1080)
const int32_t  BRAKE_PID_P     = 30000000;
const int32_t  BRAKE_PID_I     = 1000;
const int32_t  BRAKE_PID_D     = 40000000;
const uint32_t BRAKE_MAX_MA    = 1500;
const uint32_t SETTLE_MS       = 5000;

// Serial input
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
void updateJog();
void startSpeedBrakeSpin();
void releaseMotor();
void jogMotor(float offsetDeg);
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
  Serial.println("  ORBITAL TEMPLE - 09_speed_brake");
  Serial.println("============================================");
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  s<rpm>   Set target speed (e.g., s50, s-30, s0)");
  Serial.println("  c<mA>    Set max current (e.g., c500, c1000)");
  Serial.println("  t<ms>    Set transition time (e.g., t3000)");
  Serial.println("  e<0-3>   Set easing (0=linear, 1=smooth, 2=ease-in, 3=ease-out)");
  Serial.println();
  Serial.println("  kp<val>  Set speed-PID P gain");
  Serial.println("  ki<val>  Set speed-PID I gain");
  Serial.println("  kd<val>  Set speed-PID D gain");
  Serial.println("  r        Read current PID values from motor");
  Serial.println("  p        Print settings");
  Serial.println();
  Serial.println("  f        Release motor (free-spin). Pointer falls to gravity rest.");
  Serial.println("           Wait for pointer to fully settle, then press 'g'.");
  Serial.println("  g        Speed-mode 1800 deg spin (gravity-alive cruise),");
  Serial.println("           position-mode brake at target. Lock at start angle.");
  Serial.println("  j<deg>   Jog by offset (e.g., j90 = move +90 deg from current and hold).");
  Serial.println("           Use after 'f' to position pointer before 'g'.");
  Serial.println("  x        Emergency stop");
  Serial.println();

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: Motor not found at 0x64!");
    Serial.println("Check wiring: SDA=D9(GPIO8), SCL=D10(GPIO9)");
    while (1) delay(1000);
  }

  Serial.println("Motor connected!");
  motorInit();

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

  updateSpin();
  updateJog();
  updateMotion();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    emitLogDelta();
  }

  delay(10);  // 100 Hz update rate
}

// =============================================================================
// COMPRESSED DELTA LOG (identical schema to 07)
// =============================================================================

struct LogState {
  bool initialized = false;
  bool m_pos;
  float T;
  float c;
  float a;
  float p;
  int   tr;
  int   tmp;
  float v;
  int32_t mc;
  uint32_t tt;
  int   e;
  int32_t kP, kI, kD;
};
static LogState g_log;

static inline bool fchg(float a, float b, float eps) {
  return fabsf(a - b) >= eps;
}

void emitLogDelta() {
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

  if (cmd.length() >= 2) {
    String prefix = cmd.substring(0, 2);
    prefix.toLowerCase();

    if (prefix == "kp") {
      speedPID_P = cmd.substring(2).toInt();
      motorSetSpeedPID(speedPID_P, speedPID_I, speedPID_D);
      Serial.print(">> PID P set to: "); Serial.println(speedPID_P);
      return;
    }
    if (prefix == "ki") {
      speedPID_I = cmd.substring(2).toInt();
      motorSetSpeedPID(speedPID_P, speedPID_I, speedPID_D);
      Serial.print(">> PID I set to: "); Serial.println(speedPID_I);
      return;
    }
    if (prefix == "kd") {
      speedPID_D = cmd.substring(2).toInt();
      motorSetSpeedPID(speedPID_P, speedPID_I, speedPID_D);
      Serial.print(">> PID D set to: "); Serial.println(speedPID_D);
      return;
    }
  }

  char cmdType = cmd.charAt(0);
  String value = cmd.substring(1);

  switch (cmdType) {
    case 's':
    case 'S': {
      float newSpeed = value.toFloat();
      Serial.print(">> Setting target speed: ");
      Serial.print(newSpeed);
      Serial.println(" RPM");
      startTransition(newSpeed);
      break;
    }

    case 'c':
    case 'C': {
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

    case 'r':
    case 'R':
      motorReadSpeedPID();
      Serial.println(">> PID values read from motor");
      printSettings();
      break;

    case 'p':
    case 'P':
      printSettings();
      break;

    case 'f':
    case 'F':
      releaseMotor();
      break;

    case 'g':
    case 'G':
      startSpeedBrakeSpin();
      break;

    case 'j':
    case 'J': {
      float offsetDeg = value.toFloat();
      jogMotor(offsetDeg);
      break;
    }

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
  if (newTarget == targetSpeedRPM && !inTransition) return;
  startSpeedRPM = currentSpeedRPM;
  targetSpeedRPM = newTarget;
  transitionStartTime = millis();
  inTransition = true;
}

void updateMotion() {
  if (inTransition) {
    unsigned long elapsed = millis() - transitionStartTime;
    if (elapsed >= transitionTimeMS) {
      currentSpeedRPM = targetSpeedRPM;
      inTransition = false;
    } else {
      float progress = (float)elapsed / transitionTimeMS;
      float easedProgress = applyEasing(progress, easingMode);
      currentSpeedRPM = startSpeedRPM + (targetSpeedRPM - startSpeedRPM) * easedProgress;
    }
  }

  if (usePositionMode) {
    // BRAKE/SETTLE phase: re-write the frozen setpoint each tick.
    // floatPosition was set to the exact target when BRAKE fired; this
    // just reinforces it so the position PID never wanders.
    motorSetPosition((int32_t)(floatPosition * 100.0f));
  } else {
    // RAMP_UP and CRUISE: write REG_SPEED directly.
    motorSetSpeed(currentSpeedRPM);
  }
}

void emergencyStop() {
  Serial.println(">> EMERGENCY STOP");
  inTransition = false;
  targetSpeedRPM = 0;
  currentSpeedRPM = 0;
  g_spinPhase = SPIN_IDLE;
  g_jogPhase  = JOG_IDLE;
  // Return to speed mode for safety
  writeReg8(REG_MODE, MODE_SPEED);
  usePositionMode = false;
  motorSetSpeed(0);
}

// =============================================================================
// MOTOR RELEASE (free-spin)
// =============================================================================

void releaseMotor() {
  if (g_spinPhase != SPIN_IDLE) {
    Serial.println(">> Aborting spin before release.");
    g_spinPhase = SPIN_IDLE;
  }
  g_jogPhase      = JOG_IDLE;
  inTransition    = false;
  targetSpeedRPM  = 0;
  currentSpeedRPM = 0;
  usePositionMode = false;

  motorEnable(false);
  Serial.println(">> MOTOR RELEASED. Pointer free to settle.");
  Serial.println(">> Wait for pointer to stop moving, then press 'g'.");
}

// =============================================================================
// JOG COMMAND
// =============================================================================
//
// Moves the rotor by offsetDeg from its current encoder position and holds it
// using the same position-PID gains as the BRAKE phase. A held jog is
// mechanically the same problem as a brake hold: resist gravity at a fixed
// angle. The register write order mirrors BRAKE exactly (PID, mode, max-cur,
// setpoint) so the PID loop starts with correct gains.
//
// After a jog, pressing 'g' works correctly because startSpeedBrakeSpin()
// explicitly sets REG_MODE = MODE_SPEED and re-enables the output before
// capturing the start encoder reading. No special handling needed.

void jogMotor(float offsetDeg) {
  // Abort any in-progress spin cleanly.
  if (g_spinPhase != SPIN_IDLE) {
    Serial.println(">> JOG: aborting in-progress spin.");
    g_spinPhase = SPIN_IDLE;
  }
  // Abort any in-progress jog cleanly.
  g_jogPhase = JOG_IDLE;

  // Stop any speed-mode transition.
  inTransition    = false;
  currentSpeedRPM = 0;
  targetSpeedRPM  = 0;

  // Read current encoder BEFORE any I2C mode-switch writes.
  float currentEnc = motorGetEncoderDeg();
  float targetEnc  = currentEnc + offsetDeg;

  // Register write order (same as BRAKE phase -- critical):
  // 1. Load position PID first so the loop starts with correct gains.
  // 2. Switch mode to POSITION.
  // 3. Write max current.
  // 4. Write setpoint.
  writeReg96(REG_POS_PID, BRAKE_PID_P, BRAKE_PID_I, BRAKE_PID_D);
  delay(5);
  writeReg8(REG_MODE, MODE_POSITION);
  delay(5);
  writeReg32(REG_POS_MAXCUR, (int32_t)(BRAKE_MAX_MA * 100));
  delay(5);
  writeReg32(REG_POS, (int32_t)(targetEnc * 100.0f));

  // Re-enable output -- 'f' may have left it disabled.
  motorEnable(true);

  // Sync floatPosition so updateMotion() reinforces the setpoint every tick.
  floatPosition   = targetEnc;
  usePositionMode = true;

  // Record target for the state machine and launch SEEK.
  g_jogTargetEnc    = targetEnc;
  g_jogPhaseStartMS = millis();
  g_jogPhase        = JOG_SEEK;

  Serial.print(">> JOG start enc=");
  Serial.print(currentEnc, 2);
  Serial.print(" deg  offset=");
  if (offsetDeg >= 0) Serial.print("+");
  Serial.print(offsetDeg, 2);
  Serial.print(" deg  target=");
  Serial.print(targetEnc, 2);
  Serial.println(" deg");
}

// =============================================================================
// SPEED-BRAKE SPIN
// =============================================================================

void startSpeedBrakeSpin() {
  if (g_spinPhase != SPIN_IDLE) {
    Serial.println(">> Spin already in progress. Use 'x' to abort.");
    return;
  }
  // Abort any in-progress jog -- 'g' takes over motor control.
  g_jogPhase = JOG_IDLE;

  // Re-enable output in speed mode (in case 'f' left it disabled).
  writeReg8(REG_MODE, MODE_SPEED);
  delay(20);
  writeReg32(REG_SPEED_MAXCUR, maxCurrentMA * 100);
  delay(20);
  motorSetSpeed(0);
  delay(20);
  motorEnable(true);
  usePositionMode = false;
  delay(50);

  // Capture real encoder reading as the spin origin.
  // After 'f', the rotor settled to gravity rest; this is the true X.
  // We do NOT use floatPosition here -- it would be stale from the
  // last position-mode session.
  g_spinStartEnc  = motorGetEncoderDeg();
  g_spinTargetEnc = g_spinStartEnc + SPIN_TOTAL_DEG;

  Serial.print(">> SPIN_SB start enc=");
  Serial.print(g_spinStartEnc, 2);
  Serial.print(" deg target=");
  Serial.print(g_spinTargetEnc, 2);
  Serial.println(" deg");

  // Ramp up 0 -> 8 RPM with cosine ease over SPIN_RAMP_MS.
  transitionTimeMS = SPIN_RAMP_MS;
  easingMode = EASE_IN_OUT;
  startTransition(SPIN_CRUISE_RPM);

  g_spinPhase        = SPIN_RAMP_UP;
  g_spinPhaseStartMS = millis();
}

void updateSpin() {
  if (g_spinPhase == SPIN_IDLE) return;

  float curEnc     = motorGetEncoderDeg();
  float encMoved   = curEnc - g_spinStartEnc;

  switch (g_spinPhase) {

    case SPIN_RAMP_UP:
      // Wait for the eased speed ramp to complete.
      if (!inTransition) {
        Serial.print(">> SPIN_SB cruise (encMoved=");
        Serial.print(encMoved, 2);
        Serial.println(")");
        g_spinPhase        = SPIN_CRUISE;
        g_spinPhaseStartMS = millis();
      }
      break;

    case SPIN_CRUISE:
      // Speed mode holding 8 RPM. Fire brake when encoder displacement
      // reaches BRAKE_TRIGGER = SPIN_TOTAL_DEG - BRAKE_LEAD_DEG.
      if (encMoved >= BRAKE_TRIGGER) {
        Serial.print(">> SPIN_SB brake fire (encMoved=");
        Serial.print(encMoved, 2);
        Serial.print(", target=");
        Serial.print(g_spinTargetEnc, 2);
        Serial.print(", lead=");
        Serial.print(BRAKE_LEAD_DEG, 2);
        Serial.println(")");

        // BRAKE register write order (critical):
        // 1. Load position PID into motor flash FIRST.
        // 2. Switch mode to POSITION -- loop starts with correct gains.
        // 3. Write brake max-current.
        // 4. Write frozen setpoint = start + 1800 deg exactly.
        // No motorEnable toggle -- rotor never coasts during the switch.
        writeReg96(REG_POS_PID, BRAKE_PID_P, BRAKE_PID_I, BRAKE_PID_D);
        delay(5);
        writeReg8(REG_MODE, MODE_POSITION);
        delay(5);
        writeReg32(REG_POS_MAXCUR, (int32_t)(BRAKE_MAX_MA * 100));
        delay(5);
        writeReg32(REG_POS, (int32_t)(g_spinTargetEnc * 100.0f));

        // Sync floatPosition so updateMotion() reinforces the setpoint.
        floatPosition   = g_spinTargetEnc;
        usePositionMode = true;
        // Stop speed-mode integration
        inTransition    = false;
        currentSpeedRPM = 0;
        targetSpeedRPM  = 0;

        Serial.print(">> SPIN_SB lock setpoint=");
        Serial.println(g_spinTargetEnc, 2);

        g_spinPhase        = SPIN_BRAKE;
        g_spinPhaseStartMS = millis();
      }
      break;

    case SPIN_BRAKE:
      // Immediately transition to SETTLE; BRAKE is a one-shot register write.
      // The position PID is now running; SETTLE gives it time to converge.
      g_spinPhase        = SPIN_SETTLE;
      g_spinPhaseStartMS = millis();
      break;

    case SPIN_SETTLE:
      if (millis() - g_spinPhaseStartMS >= SETTLE_MS) {
        float finalEnc = motorGetEncoderDeg();
        float err      = finalEnc - g_spinTargetEnc;
        // Physical offset from start: how many degrees from X did we land?
        // 1800 deg = 5 full revolutions so mod 360 gives the angular residual.
        float physOffset = fmodf(finalEnc - g_spinStartEnc, 360.0f);
        if (physOffset < 0) physOffset += 360.0f;
        // Normalize to [-180, 180] for signed offset reporting
        if (physOffset > 180.0f) physOffset -= 360.0f;

        Serial.print(">> SPIN_SB done. final=");
        Serial.print(finalEnc, 2);
        Serial.print("  err=");
        Serial.print(err, 2);
        Serial.print(" deg  phys-offset-from-start=");
        Serial.print(physOffset, 2);
        Serial.println(" deg");

        g_spinPhase = SPIN_IDLE;
      }
      break;

    case SPIN_IDLE:
      break;
  }
}

// =============================================================================
// JOG STATE MACHINE TICK
// =============================================================================
//
// Called every loop iteration. Non-blocking: reads encoder, decides phase
// transitions, prints status lines, then returns immediately. The position-mode
// hold (floatPosition + usePositionMode) runs in updateMotion() independently.

void updateJog() {
  if (g_jogPhase == JOG_IDLE) return;

  float curEnc = motorGetEncoderDeg();
  uint32_t now = millis();

  switch (g_jogPhase) {

    case JOG_SEEK: {
      float err = fabsf(curEnc - g_jogTargetEnc);

      // Reached target window: transition to SETTLE.
      if (err <= JOG_SEEK_TOL_DEG) {
        // Initialize the stability anchor for the SETTLE phase.
        g_jogAnchorEnc    = curEnc;
        g_jogAnchorMS     = now;
        g_jogPhaseStartMS = now;
        g_jogPhase        = JOG_SETTLE;
        break;
      }

      // Timeout: give up seeking, transition to IDLE anyway.
      if (now - g_jogPhaseStartMS >= JOG_SEEK_TIMEOUT_MS) {
        Serial.print(">> JOG timeout — could not reach target (cur=");
        Serial.print(curEnc, 2);
        Serial.print(" target=");
        Serial.print(g_jogTargetEnc, 2);
        Serial.println(")");
        g_jogPhase = JOG_IDLE;
      }
      break;
    }

    case JOG_SETTLE: {
      // Anchor-reset: if the rotor drifted more than the settle tolerance from
      // the anchor, restart the stability window from the current position.
      if (fabsf(curEnc - g_jogAnchorEnc) > JOG_SETTLE_TOL_DEG) {
        g_jogAnchorEnc = curEnc;
        g_jogAnchorMS  = now;
      }

      // Settled: anchor has held for the required continuous window.
      if (now - g_jogAnchorMS >= JOG_SETTLE_WIN_MS) {
        float finalErr = curEnc - g_jogTargetEnc;
        Serial.print(">> JOG settled at ");
        Serial.print(curEnc, 2);
        Serial.print(" deg (target was ");
        Serial.print(g_jogTargetEnc, 2);
        Serial.print(" deg, err=");
        Serial.print(finalErr, 2);
        Serial.println(")");
        g_jogPhase = JOG_IDLE;
        break;
      }

      // Overall SETTLE timeout: declare done regardless.
      if (now - g_jogPhaseStartMS >= JOG_SETTLE_TIMEOUT_MS) {
        Serial.print(">> JOG settle timeout (last enc=");
        Serial.print(curEnc, 2);
        Serial.print(", target=");
        Serial.print(g_jogTargetEnc, 2);
        Serial.println(")");
        g_jogPhase = JOG_IDLE;
      }
      break;
    }

    case JOG_IDLE:
      break;
  }
}

void printSettings() {
  Serial.println();
  Serial.println("=== Current Settings ===");
  Serial.print("Control Mode:    ");
  Serial.println(usePositionMode ? "POSITION (brake active)" : "SPEED");
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
  Serial.print("P (kp): "); Serial.println(speedPID_P);
  Serial.print("I (ki): "); Serial.println(speedPID_I);
  Serial.print("D (kd): "); Serial.println(speedPID_D);
  Serial.println("--- Spin Constants ---");
  Serial.print("SPIN_TOTAL_DEG:  "); Serial.println(SPIN_TOTAL_DEG);
  Serial.print("BRAKE_TRIGGER:   "); Serial.println(BRAKE_TRIGGER);
  Serial.print("BRAKE_LEAD_DEG:  "); Serial.println(BRAKE_LEAD_DEG);
  Serial.print("BRAKE_MAX_MA:    "); Serial.println(BRAKE_MAX_MA);
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
  writeReg8(REG_OUTPUT, 0);
  delay(50);
  writeReg8(REG_STALL_PROT, 0);
  delay(50);
  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);
  motorSetMaxCurrent(maxCurrentMA);
  delay(50);
  motorSetSpeed(0);
  delay(50);
  writeReg8(REG_OUTPUT, 1);
  delay(50);
}

void motorSetSpeed(float rpm) {
  int32_t scaledSpeed = (int32_t)(rpm * 100.0f);
  writeReg32(REG_SPEED, scaledSpeed);
}

void motorSetMaxCurrent(int32_t mA) {
  writeReg32(REG_SPEED_MAXCUR, mA * 100);
}

void motorEnable(bool enable) {
  writeReg8(REG_OUTPUT, enable ? 1 : 0);
}

float motorGetSpeed() {
  return (float)readReg32(REG_SPEED_READ);
}

void motorSetSpeedPID(int32_t p, int32_t i, int32_t d) {
  writeReg96(REG_SPEED_PID, p, i, d);
  Serial.print(">> PID written: P="); Serial.print(p);
  Serial.print(" I="); Serial.print(i);
  Serial.print(" D="); Serial.println(d);
}

void motorReadSpeedPID() {
  readReg96(REG_SPEED_PID, &speedPID_P, &speedPID_I, &speedPID_D);
  Serial.print(">> PID from motor: P="); Serial.print(speedPID_P);
  Serial.print(" I="); Serial.print(speedPID_I);
  Serial.print(" D="); Serial.println(speedPID_D);
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
