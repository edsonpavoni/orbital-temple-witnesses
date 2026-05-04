/*
 * ============================================================================
 * ORBITAL TEMPLE - POSITION LAB (APRIL)
 * ============================================================================
 *
 * Lab firmware for finding the right recipe to move the pointer to a specific
 * angle and stop there. Companion to 03_motion_test, which is the
 * smooth-rotation lab. Different control regime: closed-loop position mode
 * with a shaped trajectory.
 *
 * WORKFLOW
 * --------
 *   On boot the motor is RELEASED — output disabled, pointer hand-free.
 *   1. Hand-position the pointer at visual zero. Press `0` + Enter.
 *      The firmware records the current encoder reading as the "zero offset"
 *      and engages position-mode hold at that angle.
 *   2. Press `r` to start the ritual. The pointer steps through angles
 *      `step°`, `2·step°`, `3·step°`, ... holding `holdMS` at each. Targets
 *      keep climbing past 360° (encoder is cumulative — visually identical
 *      to wrapping).
 *   3. Press `x` at any time to release the motor. The encoder zero is
 *      cleared; press `0` again to re-engage.
 *
 * TRAJECTORY
 * ----------
 *   Each step is interpolated on the position setpoint side: every 10 ms
 *   the firmware computes `setpoint = start + (end - start) * easing(t)` and
 *   writes that as the position target. The motor's internal position PID
 *   tracks the slowly-moving target. With easing OFF (`eoff`), the final
 *   target is written at move start and we just wait `mt` ms for settling.
 *
 * COMMANDS
 * --------
 *   0              Set software zero at current encoder, engage hold
 *   r              Start / resume ritual (refused if `0` not pressed yet)
 *   x              Disable output, return to RELEASED, clear zero
 *
 *   step<deg>      Ritual step size                       (default 45)
 *   hold<ms>       Hold duration at each ritual position  (default 2000)
 *   mt<ms>         Move (trajectory) duration             (default 1500)
 *   e<0-3>         Easing curve (0=lin 1=in_out 2=in 3=out, default 1)
 *   eon / eoff     Easing on / off                        (default on)
 *   c<mA>          Position-mode max current  (reg 0x20)  (default 600)
 *   kp/ki/kd<v>    Position PID gains         (reg 0xA0)
 *   pidread        Read position PID back from motor
 *   goto<deg>      One-shot move to absolute angle (no auto-advance)
 *   p              Print full settings + live state
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

// Register addresses (M5Stack Unit-Roller485 Lite, I2C protocol)
#define REG_OUTPUT       0x00
#define REG_MODE         0x01
#define REG_STALL_PROT   0x0F
#define REG_POS_MAXCUR   0x20   // Position-mode max current (int32, mA*100)
#define REG_VIN          0x34   // Supply voltage (int32, V*100)
#define REG_TEMP         0x38   // Motor temperature (int32, deg C)
#define REG_SPEED_READ   0x60   // Speed readback (int32, RPM*100)
#define REG_POS          0x80   // Position target (int32, deg*100)
#define REG_POS_READ     0x90   // Position readback (int32, deg*100)
#define REG_POS_PID      0xA0   // Position PID (12 bytes, P/I/D as int32)

#define MODE_SPEED       1
#define MODE_POSITION    2
#define MODE_CURRENT     3

#define COUNTS_PER_DEG   100    // 36 000 counts = 360 deg

// =============================================================================
// EASING FUNCTIONS (verbatim from 03_motion_test)
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
// STATE
// =============================================================================

enum State {
  ST_RELEASED = 0,  // Output off; user can hand-position the pointer.
  ST_HOLDING,       // Output on, target locked. May or may not auto-advance.
  ST_MOVING         // Output on, trajectory advancing setpoint toward end.
};

// Tunables (live-editable over serial)
float    stepDeg          = 45.0f;
uint32_t holdMS           = 2000;
uint32_t moveTimeMS       = 1500;
EasingMode easingMode     = EASE_IN_OUT;
bool     easingOn         = true;
int32_t  posMaxCurrentMA  = 600;
int32_t  posPID_P         = 15000000;
int32_t  posPID_I         = 1000;
int32_t  posPID_D         = 40000000;

// Runtime state
State    state            = ST_RELEASED;
bool     zeroSet          = false;
bool     ritualActive     = false;       // Holding will auto-advance when true.
int32_t  zeroOffsetCounts = 0;            // Encoder raw counts that map to 0 deg.
float    currentTargetDeg = 0.0f;         // The angle we are at / heading toward.
float    moveStartDeg     = 0.0f;
float    moveEndDeg       = 0.0f;
uint32_t moveStartMS      = 0;
uint32_t holdStartMS      = 0;

String   inputBuffer;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

void motorInitForRelease();
void enterHoldFromZero();
void releaseMotor();
void startMoveTo(float toDeg);
void updateRitual();

void writeTargetDeg(float deg);
float readEncoderDeg();
float motorGetVinV();
int32_t motorGetTempC();
float motorGetSpeedRPM();
void motorWritePosMaxCurrent(int32_t mA);
void motorWritePosPID(int32_t p, int32_t i, int32_t d);
void motorReadPosPID();

void writeReg8(uint8_t reg, uint8_t value);
void writeReg32(uint8_t reg, int32_t value);
int32_t readReg32(uint8_t reg);
void writeReg96(uint8_t reg, int32_t v1, int32_t v2, int32_t v3);
void readReg96(uint8_t reg, int32_t* v1, int32_t* v2, int32_t* v3);

void processSerialCommand(String cmd);
void printSettings();
void emitLogDelta();

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("============================================");
  Serial.println("  ORBITAL TEMPLE - POSITION LAB (APRIL)");
  Serial.println("============================================");
  Serial.println();
  Serial.println("Workflow:");
  Serial.println("  1. Hand-position pointer at zero, press 0");
  Serial.println("  2. Press r to start ritual (steps of step deg)");
  Serial.println("  3. Press x to release motor");
  Serial.println();
  Serial.println("Tunables: step<deg> hold<ms> mt<ms> e<0-3> eon/eoff");
  Serial.println("          c<mA> kp/ki/kd<v> pidread goto<deg> p");
  Serial.println();

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: Motor not found at 0x64!");
    while (1) delay(1000);
  }
  Serial.println("Motor connected.");

  motorInitForRelease();

  // Read whatever PID the motor currently has (factory or previously tuned)
  // and use it as the live default. The compile-time defaults above are only
  // a fallback if the motor flash has been wiped.
  Serial.println("Reading motor position PID...");
  motorReadPosPID();

  // Push our position-max-current default onto the motor.
  motorWritePosMaxCurrent(posMaxCurrentMA);

  printSettings();
  Serial.println();
  Serial.println("# log schema (delta-encoded; fields omitted = unchanged):");
  Serial.println("#   t=ms  st=R|H|M  ra=0|1  Tg=tgtDeg  sp=setptDeg");
  Serial.println("#   p=encDeg  er=spErr  a=actRPM  tr=movePct");
  Serial.println("#   tmp=tempC  v=vinV  mc=maxCurMA");
  Serial.println("#   e=ease(0-3)  eon=0|1  mt=moveMS  hold=holdMS  step=stepDeg");
  Serial.println("#   kP/kI/kD=PID");
  Serial.println("Ready.");
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

  updateRitual();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    emitLogDelta();
  }

  delay(10);
}

// =============================================================================
// MOTOR / RITUAL CONTROL
// =============================================================================

void motorInitForRelease() {
  // Boot state: output OFF so the pointer can be hand-positioned freely.
  writeReg8(REG_OUTPUT, 0);
  delay(20);
  writeReg8(REG_STALL_PROT, 0);  // No motor-side jam protection in the lab.
  delay(20);
  // Pre-load position max current so reg 0x20 is sane when we eventually enable.
  writeReg32(REG_POS_MAXCUR, posMaxCurrentMA * 100);
  delay(20);
  state = ST_RELEASED;
  zeroSet = false;
  ritualActive = false;
}

void enterHoldFromZero() {
  // Atomic-ish sequence: stop -> position mode -> set target = current encoder
  // -> enable. Done with output off so the motor doesn't jolt while we change
  // mode. Position max current was already set in setup().
  writeReg8(REG_OUTPUT, 0);
  delay(20);
  writeReg8(REG_MODE, MODE_POSITION);
  delay(20);
  motorWritePosMaxCurrent(posMaxCurrentMA);
  delay(20);

  zeroOffsetCounts = readReg32(REG_POS_READ);
  writeReg32(REG_POS, zeroOffsetCounts);
  delay(20);

  writeReg8(REG_OUTPUT, 1);

  zeroSet = true;
  currentTargetDeg = 0.0f;
  state = ST_HOLDING;
  ritualActive = false;
  holdStartMS = millis();

  Serial.print(">> Zero set at encoder ");
  Serial.print(zeroOffsetCounts);
  Serial.println(". Holding at 0 deg.");
}

void releaseMotor() {
  writeReg8(REG_OUTPUT, 0);
  state = ST_RELEASED;
  zeroSet = false;
  ritualActive = false;
  currentTargetDeg = 0.0f;
  Serial.println(">> Released. Press 0 to re-zero.");
}

void startMoveTo(float toDeg) {
  moveStartDeg = currentTargetDeg;
  moveEndDeg = toDeg;
  moveStartMS = millis();
  state = ST_MOVING;

  // For the easing-off case, write the final target immediately so the motor
  // starts chasing it right away. updateRitual() will keep writing it each
  // tick, which is harmless.
  if (!easingOn) {
    writeTargetDeg(moveEndDeg);
  }
}

void updateRitual() {
  if (state == ST_MOVING) {
    uint32_t elapsed = millis() - moveStartMS;
    if (elapsed >= moveTimeMS) {
      // Move complete -> latch end target and transition to HOLDING.
      currentTargetDeg = moveEndDeg;
      writeTargetDeg(currentTargetDeg);
      state = ST_HOLDING;
      holdStartMS = millis();
    } else {
      float sp;
      if (easingOn) {
        float progress = (float)elapsed / moveTimeMS;
        float eased = applyEasing(progress, easingMode);
        sp = moveStartDeg + (moveEndDeg - moveStartDeg) * eased;
      } else {
        sp = moveEndDeg;
      }
      writeTargetDeg(sp);
    }
  } else if (state == ST_HOLDING) {
    // Re-write target every tick to be safe (in case of single-byte glitches).
    writeTargetDeg(currentTargetDeg);
    if (ritualActive) {
      uint32_t held = millis() - holdStartMS;
      if (held >= holdMS) {
        startMoveTo(currentTargetDeg + stepDeg);
      }
    }
  }
  // ST_RELEASED: no writes; motor coasts.
}

// =============================================================================
// COMMAND HANDLERS
// =============================================================================

void handleZero() {
  enterHoldFromZero();
}

void handleR() {
  if (!zeroSet) {
    Serial.println(">> ERROR: press 0 first to set zero");
    return;
  }
  ritualActive = true;
  if (state == ST_HOLDING) {
    Serial.print(">> Ritual: stepping to ");
    Serial.print(currentTargetDeg + stepDeg, 1);
    Serial.println(" deg");
    startMoveTo(currentTargetDeg + stepDeg);
  } else if (state == ST_MOVING) {
    Serial.println(">> Ritual armed; will auto-advance after current move");
  } else {
    Serial.println(">> ERROR: unexpected state");
  }
}

void handleX() {
  releaseMotor();
}

void handleGoto(float deg) {
  if (!zeroSet) {
    Serial.println(">> ERROR: press 0 first to set zero");
    return;
  }
  ritualActive = false;
  Serial.print(">> Goto ");
  Serial.print(deg, 1);
  Serial.println(" deg (one-shot, no auto-advance)");
  startMoveTo(deg);
}

// =============================================================================
// SERIAL COMMAND PROCESSING
// =============================================================================

void processSerialCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  String low = cmd;
  low.toLowerCase();

  // Single-character commands first
  if (low == "0") { handleZero(); return; }
  if (low == "r") { handleR(); return; }
  if (low == "x") { handleX(); return; }
  if (low == "p") { printSettings(); return; }
  if (low == "eon") {
    easingOn = true;
    Serial.println(">> Easing ON");
    return;
  }
  if (low == "eoff") {
    easingOn = false;
    Serial.println(">> Easing OFF (target written instantly each move)");
    return;
  }
  if (low == "pidread") {
    motorReadPosPID();
    Serial.println(">> Position PID read from motor");
    return;
  }

  // Multi-character prefix commands
  if (low.startsWith("step")) {
    float v = cmd.substring(4).toFloat();
    if (v <= 0 || v > 360) {
      Serial.println(">> Invalid step (must be 0 < step <= 360)");
    } else {
      stepDeg = v;
      Serial.print(">> step = "); Serial.print(stepDeg, 2); Serial.println(" deg");
    }
    return;
  }
  if (low.startsWith("hold")) {
    int32_t v = cmd.substring(4).toInt();
    if (v < 0 || v > 60000) {
      Serial.println(">> Invalid hold (0 - 60000 ms)");
    } else {
      holdMS = (uint32_t)v;
      Serial.print(">> hold = "); Serial.print(holdMS); Serial.println(" ms");
    }
    return;
  }
  if (low.startsWith("mt")) {
    int32_t v = cmd.substring(2).toInt();
    if (v < 50 || v > 60000) {
      Serial.println(">> Invalid mt (50 - 60000 ms)");
    } else {
      moveTimeMS = (uint32_t)v;
      Serial.print(">> mt = "); Serial.print(moveTimeMS); Serial.println(" ms");
    }
    return;
  }
  if (low.startsWith("goto")) {
    float v = cmd.substring(4).toFloat();
    handleGoto(v);
    return;
  }
  if (low.startsWith("kp")) {
    int32_t v = cmd.substring(2).toInt();
    posPID_P = v;
    motorWritePosPID(posPID_P, posPID_I, posPID_D);
    Serial.print(">> kP = "); Serial.println(posPID_P);
    return;
  }
  if (low.startsWith("ki")) {
    int32_t v = cmd.substring(2).toInt();
    posPID_I = v;
    motorWritePosPID(posPID_P, posPID_I, posPID_D);
    Serial.print(">> kI = "); Serial.println(posPID_I);
    return;
  }
  if (low.startsWith("kd")) {
    int32_t v = cmd.substring(2).toInt();
    posPID_D = v;
    motorWritePosPID(posPID_P, posPID_I, posPID_D);
    Serial.print(">> kD = "); Serial.println(posPID_D);
    return;
  }

  // Single-letter prefix commands (e<n>, c<mA>) — checked after multi-char to
  // avoid eating "eon"/"eoff".
  char head = cmd.charAt(0);
  String rest = cmd.substring(1);
  switch (head) {
    case 'e':
    case 'E': {
      int m = rest.toInt();
      if (m < 0 || m > 3) {
        Serial.println(">> Invalid easing (0-3)");
      } else {
        easingMode = (EasingMode)m;
        Serial.print(">> easing = "); Serial.println(easingName(easingMode));
      }
      return;
    }
    case 'c':
    case 'C': {
      int32_t v = rest.toInt();
      if (v < 100) v = 100;
      if (v > 2000) v = 2000;
      posMaxCurrentMA = v;
      motorWritePosMaxCurrent(posMaxCurrentMA);
      Serial.print(">> position max current = ");
      Serial.print(posMaxCurrentMA);
      Serial.println(" mA");
      return;
    }
  }

  Serial.print(">> Unknown command: ");
  Serial.println(cmd);
}

// =============================================================================
// PRINT SETTINGS
// =============================================================================

void printSettings() {
  const char* stateName = (state == ST_RELEASED) ? "RELEASED"
                        : (state == ST_HOLDING)  ? "HOLDING"
                        : "MOVING";
  Serial.println();
  Serial.println("=== Settings ===");
  Serial.print("State:           "); Serial.print(stateName);
  Serial.print(" (zeroSet="); Serial.print(zeroSet ? "yes" : "no");
  Serial.print(", ritual="); Serial.print(ritualActive ? "on" : "off");
  Serial.println(")");
  Serial.print("Step:            "); Serial.print(stepDeg, 2); Serial.println(" deg");
  Serial.print("Hold:            "); Serial.print(holdMS); Serial.println(" ms");
  Serial.print("Move time:       "); Serial.print(moveTimeMS); Serial.println(" ms");
  Serial.print("Easing:          ");
  Serial.print(easingOn ? "ON " : "OFF ");
  Serial.print("("); Serial.print(easingName(easingMode)); Serial.println(")");
  Serial.print("Pos max current: "); Serial.print(posMaxCurrentMA); Serial.println(" mA");
  Serial.println("--- Position PID ---");
  Serial.print("P (kp): "); Serial.println(posPID_P);
  Serial.print("I (ki): "); Serial.println(posPID_I);
  Serial.print("D (kd): "); Serial.println(posPID_D);
  Serial.println("--- Live ---");
  Serial.print("Vin:             "); Serial.print(motorGetVinV(), 2); Serial.println(" V");
  Serial.print("Enc raw:         "); Serial.print(readReg32(REG_POS_READ));
  Serial.print("  (zero offset "); Serial.print(zeroOffsetCounts); Serial.println(")");
  Serial.print("Enc user-deg:    "); Serial.print(readEncoderDeg(), 2); Serial.println(" deg");
  Serial.print("Logical target:  "); Serial.print(currentTargetDeg, 2); Serial.println(" deg");
  Serial.print("Motor temp:      "); Serial.print(motorGetTempC()); Serial.println(" C");
  Serial.println("====================");
}

// =============================================================================
// COMPRESSED DELTA LOG
// =============================================================================

struct LogState {
  bool initialized = false;
  char st;          // 'R' / 'H' / 'M'
  int  ra;          // 0 / 1
  float Tg;         // logical target deg
  float sp;         // current setpoint deg
  float p;          // encoder deg (user-frame)
  float er;         // setpoint - encoder
  float a;          // actual RPM
  int  tr;          // move % (-1 = idle)
  int  tmp;         // temp C
  float v;          // vin V
  int32_t mc;       // pos max current mA
  int  e;           // easing curve
  int  eon;         // easing on/off
  uint32_t mt;
  uint32_t hold;
  float step;
  int32_t kP, kI, kD;
};
static LogState g_log;

static inline bool fchg(float a, float b, float eps) { return fabsf(a - b) >= eps; }

void emitLogDelta() {
  // Snapshot
  char st = (state == ST_RELEASED) ? 'R' : (state == ST_HOLDING) ? 'H' : 'M';
  int ra = ritualActive ? 1 : 0;
  float Tg = currentTargetDeg;
  float sp;
  int tr = -1;
  if (state == ST_MOVING) {
    uint32_t elapsed = millis() - moveStartMS;
    float prog = (moveTimeMS > 0) ? ((float)elapsed / moveTimeMS) : 1.0f;
    if (prog < 0) prog = 0;
    if (prog > 1) prog = 1;
    tr = (int)(prog * 100);
    if (easingOn) {
      sp = moveStartDeg + (moveEndDeg - moveStartDeg) * applyEasing(prog, easingMode);
    } else {
      sp = moveEndDeg;
    }
  } else {
    sp = currentTargetDeg;
  }
  float p = readEncoderDeg();
  float er = sp - p;
  float a = motorGetSpeedRPM();
  int tmp = (int)motorGetTempC();
  float v = motorGetVinV();

  String d;
  d.reserve(128);
  bool first = !g_log.initialized;

  if (first || g_log.st != st) {
    d += ",st="; d += st; g_log.st = st;
  }
  if (first || g_log.ra != ra) {
    d += ",ra="; d += ra; g_log.ra = ra;
  }
  if (first || fchg(g_log.Tg, Tg, 0.05f)) {
    d += ",Tg="; d += String(Tg, 2); g_log.Tg = Tg;
  }
  if (first || fchg(g_log.sp, sp, 0.05f)) {
    d += ",sp="; d += String(sp, 2); g_log.sp = sp;
  }
  if (first || fchg(g_log.p, p, 0.05f)) {
    d += ",p="; d += String(p, 2); g_log.p = p;
  }
  if (first || fchg(g_log.er, er, 0.05f)) {
    d += ",er="; d += String(er, 2); g_log.er = er;
  }
  if (first || fchg(g_log.a, a, 0.1f)) {
    d += ",a="; d += String(a, 1); g_log.a = a;
  }
  if (first || g_log.tr != tr) {
    if (tr < 0) d += ",tr=-";
    else { d += ",tr="; d += String(tr); }
    g_log.tr = tr;
  }
  // Temp gating: print whenever output is on (state != RELEASED). On the
  // very first line we always include it as a baseline anchor.
  bool outputOn = (state != ST_RELEASED);
  if (first || (outputOn && g_log.tmp != tmp)) {
    d += ",tmp="; d += String(tmp); g_log.tmp = tmp;
  }
  if (first || fchg(g_log.v, v, 0.05f)) {
    d += ",v="; d += String(v, 2); g_log.v = v;
  }
  if (first || g_log.mc != posMaxCurrentMA) {
    d += ",mc="; d += String(posMaxCurrentMA); g_log.mc = posMaxCurrentMA;
  }
  if (first || g_log.e != (int)easingMode) {
    d += ",e="; d += String((int)easingMode); g_log.e = (int)easingMode;
  }
  if (first || g_log.eon != (easingOn ? 1 : 0)) {
    d += ",eon="; d += (easingOn ? 1 : 0); g_log.eon = easingOn ? 1 : 0;
  }
  if (first || g_log.mt != moveTimeMS) {
    d += ",mt="; d += String(moveTimeMS); g_log.mt = moveTimeMS;
  }
  if (first || g_log.hold != holdMS) {
    d += ",hold="; d += String(holdMS); g_log.hold = holdMS;
  }
  if (first || fchg(g_log.step, stepDeg, 0.01f)) {
    d += ",step="; d += String(stepDeg, 2); g_log.step = stepDeg;
  }
  if (first || g_log.kP != posPID_P) {
    d += ",kP="; d += String(posPID_P); g_log.kP = posPID_P;
  }
  if (first || g_log.kI != posPID_I) {
    d += ",kI="; d += String(posPID_I); g_log.kI = posPID_I;
  }
  if (first || g_log.kD != posPID_D) {
    d += ",kD="; d += String(posPID_D); g_log.kD = posPID_D;
  }

  g_log.initialized = true;

  if (d.length() > 0) {
    Serial.print("t="); Serial.print(millis());
    Serial.println(d);
  }
}

// =============================================================================
// MOTOR I/O HELPERS
// =============================================================================

void writeTargetDeg(float deg) {
  int32_t enc = zeroOffsetCounts + (int32_t)(deg * (float)COUNTS_PER_DEG);
  writeReg32(REG_POS, enc);
}

float readEncoderDeg() {
  // User-frame degrees: (raw - zeroOffset) / 100.
  int32_t raw = readReg32(REG_POS_READ);
  return (float)(raw - zeroOffsetCounts) / (float)COUNTS_PER_DEG;
}

float motorGetVinV()      { return readReg32(REG_VIN) / 100.0f; }
int32_t motorGetTempC()   { return readReg32(REG_TEMP); }
float motorGetSpeedRPM()  { return readReg32(REG_SPEED_READ) / 100.0f; }

void motorWritePosMaxCurrent(int32_t mA) {
  writeReg32(REG_POS_MAXCUR, mA * 100);
}

void motorWritePosPID(int32_t p, int32_t i, int32_t d) {
  writeReg96(REG_POS_PID, p, i, d);
}

void motorReadPosPID() {
  readReg96(REG_POS_PID, &posPID_P, &posPID_I, &posPID_D);
  Serial.print(">> pos PID from motor: P=");
  Serial.print(posPID_P);
  Serial.print(" I="); Serial.print(posPID_I);
  Serial.print(" D="); Serial.println(posPID_D);
}

// =============================================================================
// I2C PRIMITIVES (verbatim from 03_motion_test)
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
