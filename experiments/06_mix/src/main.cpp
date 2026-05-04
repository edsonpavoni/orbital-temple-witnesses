// main.cpp — First Witness mix firmware (mode 1 + mode 2).
//
// Behavior:
//   1. Power on → wait 1 s → engage hold-here (latches whatever encoder
//      reading the user's hand-positioning produced as user-frame zero).
//   2. Pick a random angle in [0, 360°), move there with PreciseOperator
//      (Mode 1, ±0.5° accuracy), hold 3 s.
//   3. Smooth 360° CW rotation with SmoothOperator (Mode 2, pendulum-swing
//      feel). Hold 5 s.
//   4. Repeat the spin two more times (3 spins total per random target).
//   5. Pick a new random angle (forward of current target) and go to step 2.
//
// User physically positions the pointer at visual 0 before power-on; that
// becomes the firmware's user-frame zero.

#include "MotorIO.h"
#include "PreciseOperator.h"
#include "SmoothOperator.h"
#include <Arduino.h>
#include <Wire.h>

// ─── Components ──────────────────────────────────────────────────────────
PreciseOperator precise;
SmoothOperator  smooth(precise);

// ─── State machine ──────────────────────────────────────────────────────
enum class CycleState {
  WAIT_BOOT,
  ENGAGE_HOLD,
  PICK_RANDOM,
  MOVE_TO_RANDOM,
  HOLD_AT_RANDOM,
  SPIN,
  HOLD_AFTER_SPIN,
};

CycleState g_state            = CycleState::WAIT_BOOT;
uint32_t   g_stateEnteredMS   = 0;
int        g_rotationsRemain  = 0;
float      g_currentRandomDeg = 0.0f;

// ─── Timings ────────────────────────────────────────────────────────────
constexpr uint32_t BOOT_DELAY_MS         = 1000;
constexpr uint32_t HOLD_AT_RANDOM_MS     = 3000;
constexpr uint32_t HOLD_AFTER_SPIN_MS    = 5000;
constexpr int      ROTATIONS_PER_RANDOM  = 1;   // single 1080° spin

// ─── Voltage gate ───────────────────────────────────────────────────────
// Hard interlock: motor only engages / cycles when Vin meets this threshold.
// Below 15 V the motor lacks torque to move the unbalanced pointer reliably.
constexpr float    MIN_VIN_V             = 15.0f;
constexpr uint32_t VIN_CHECK_MS          = 500;   // poll cadence
constexpr uint32_t VIN_PRINT_MS          = 1000;  // wait-state status print
uint32_t g_lastVinCheckMS = 0;
uint32_t g_lastVinPrintMS = 0;
float    g_lastVin        = 0.0f;

// ─── Periodic delta log ─────────────────────────────────────────────────
constexpr uint32_t LOG_TICK_MS = 200;
uint32_t g_lastLogMS = 0;

// Carry-forward state for delta encoding.
struct LogPrev {
  bool init = false;
  char st;
  float Tg, sp, p, er, a, v;
  int   tr, tmp;
};
LogPrev g_logPrev;

static char stateChar() {
  switch (g_state) {
    case CycleState::WAIT_BOOT:        return 'B';
    case CycleState::ENGAGE_HOLD:      return 'E';
    case CycleState::PICK_RANDOM:      return 'P';
    case CycleState::MOVE_TO_RANDOM:   return 'M';
    case CycleState::HOLD_AT_RANDOM:   return 'H';
    case CycleState::SPIN:             return 'S';
    case CycleState::HOLD_AFTER_SPIN:  return 'h';
  }
  return '?';
}

static bool fchg(float a, float b, float eps) { return fabsf(a - b) >= eps; }

static void emitLogTick() {
  using namespace MotorIO;
  char  st  = stateChar();
  float Tg  = precise.currentTargetDeg();
  float sp  = precise.currentSetpointDeg();
  float p   = static_cast<float>(encoderRaw() - precise.zeroOffsetCounts()) /
              static_cast<float>(COUNTS_PER_DEG);
  float er  = sp - p;
  float a   = actualRPM();
  int   tr  = precise.progressPct();
  int   tmp = static_cast<int>(tempC());
  float v   = vinV();

  String d;
  d.reserve(120);
  bool first = !g_logPrev.init;

  if (first || g_logPrev.st  != st)               { d += ",st="; d += st;          g_logPrev.st  = st;  }
  if (first || fchg(g_logPrev.Tg,  Tg,  0.05f))   { d += ",Tg="; d += String(Tg, 2); g_logPrev.Tg  = Tg;  }
  if (first || fchg(g_logPrev.sp,  sp,  0.05f))   { d += ",sp="; d += String(sp, 2); g_logPrev.sp  = sp;  }
  if (first || fchg(g_logPrev.p,   p,   0.05f))   { d += ",p=";  d += String(p,  2); g_logPrev.p   = p;   }
  if (first || fchg(g_logPrev.er,  er,  0.05f))   { d += ",er="; d += String(er, 2); g_logPrev.er  = er;  }
  if (first || fchg(g_logPrev.a,   a,   0.1f))    { d += ",a=";  d += String(a,  1); g_logPrev.a   = a;   }
  if (first || g_logPrev.tr  != tr) {
    if (tr < 0) d += ",tr=-"; else { d += ",tr="; d += String(tr); }
    g_logPrev.tr = tr;
  }
  if (first || g_logPrev.tmp != tmp)              { d += ",tmp="; d += String(tmp); g_logPrev.tmp = tmp; }
  if (first || fchg(g_logPrev.v,   v,   0.05f))   { d += ",v=";  d += String(v,  2); g_logPrev.v   = v;   }

  g_logPrev.init = true;
  if (d.length() > 0) {
    Serial.print(F("t="));
    Serial.print(millis());
    Serial.println(d);
  }
}

// ─── Helpers for state-machine transitions ──────────────────────────────
static void enter(CycleState s) {
  g_state          = s;
  g_stateEnteredMS = millis();
}

static float pickRandomDelta() {
  // Random angle in [0, 360°), expressed as deg × 100 to avoid bias.
  long r = random(0, 36000);
  return static_cast<float>(r) / 100.0f;
}

// ─── Setup ───────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println(F("======================================"));
  Serial.println(F("  FIRST WITNESS — MIX (mode 1 + 2)"));
  Serial.println(F("======================================"));

  using namespace MotorIO;
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(50);

  Wire.beginTransmission(I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println(F("ERROR: motor not found at 0x64. Halting."));
    while (true) delay(1000);
  }
  Serial.println(F("Motor connected."));

  // Disable output and stall-protection at boot — pointer free for the user
  // to position by hand if they haven't already.
  writeReg8(REG_OUTPUT, 0);
  delay(20);
  writeReg8(REG_STALL_PROT, 0);
  delay(20);

  randomSeed(esp_random());

  Serial.println(F("# log schema: t=ms,st=B|E|P|M|H|S|h"));
  Serial.println(F("#   Tg=tgtDeg sp=setpDeg p=encDeg er=spErr a=RPM tr=movePct tmp=C v=V"));
  Serial.print(F(">> Boot: waiting for Vin >= "));
  Serial.print(MIN_VIN_V, 1);
  Serial.println(F(" V"));

  enter(CycleState::WAIT_BOOT);
}

// ─── Loop ────────────────────────────────────────────────────────────────
void loop() {
  // Drive the two operators every iteration. Each one no-ops when not
  // active in its own internal sense.
  precise.tick();
  smooth.tick();

  // ─── Voltage gate ──────────────────────────────────────────────────
  // Poll Vin at VIN_CHECK_MS cadence. If we ever drop below MIN_VIN_V while
  // operating, release motor + smooth state and fall back to WAIT_BOOT until
  // power is restored.
  if (millis() - g_lastVinCheckMS >= VIN_CHECK_MS) {
    g_lastVinCheckMS = millis();
    g_lastVin = MotorIO::vinV();
    if (g_state != CycleState::WAIT_BOOT && g_lastVin < MIN_VIN_V) {
      Serial.print(F(">> Vin dropped to "));
      Serial.print(g_lastVin, 2);
      Serial.print(F(" V (need >= "));
      Serial.print(MIN_VIN_V, 1);
      Serial.println(F(" V) — disabling motor"));
      precise.release();
      smooth.release();
      enter(CycleState::WAIT_BOOT);
    }
  }

  // Drive the cycle state machine.
  uint32_t elapsed = millis() - g_stateEnteredMS;

  switch (g_state) {

    case CycleState::WAIT_BOOT:
      if (millis() - g_lastVinPrintMS >= VIN_PRINT_MS) {
        g_lastVinPrintMS = millis();
        Serial.print(F(">> Waiting: Vin="));
        Serial.print(g_lastVin, 2);
        Serial.print(F(" V (need >= "));
        Serial.print(MIN_VIN_V, 1);
        Serial.println(F(" V)"));
      }
      if (elapsed >= BOOT_DELAY_MS && g_lastVin >= MIN_VIN_V) {
        Serial.print(F(">> Power OK (Vin="));
        Serial.print(g_lastVin, 2);
        Serial.println(F(" V); engaging hold-here (latching encoder zero)"));
        enter(CycleState::ENGAGE_HOLD);
      }
      break;

    case CycleState::ENGAGE_HOLD:
      precise.engageHoldHere();
      enter(CycleState::PICK_RANDOM);
      break;

    case CycleState::PICK_RANDOM: {
      float delta = pickRandomDelta();
      g_currentRandomDeg = precise.currentTargetDeg() + delta;
      Serial.print(F(">> Target +"));
      Serial.print(delta, 1);
      Serial.println(F(" deg"));
      precise.moveTo(g_currentRandomDeg);
      enter(CycleState::MOVE_TO_RANDOM);
      break;
    }

    case CycleState::MOVE_TO_RANDOM:
      if (!precise.isMoving()) {
        enter(CycleState::HOLD_AT_RANDOM);
      }
      break;

    case CycleState::HOLD_AT_RANDOM:
      if (elapsed >= HOLD_AT_RANDOM_MS) {
        g_rotationsRemain = ROTATIONS_PER_RANDOM;
        Serial.println(F(">> Spin 1080"));
        smooth.start360CW();
        enter(CycleState::SPIN);
      }
      break;

    case CycleState::SPIN:
      if (!smooth.isRotating()) {
        g_rotationsRemain--;
        Serial.println(F(">> Spin done"));
        enter(CycleState::HOLD_AFTER_SPIN);
      }
      break;

    case CycleState::HOLD_AFTER_SPIN:
      if (elapsed >= HOLD_AFTER_SPIN_MS) {
        if (g_rotationsRemain > 0) {
          Serial.println(F(">> Spin 1080"));
          smooth.start360CW();
          enter(CycleState::SPIN);
        } else {
          enter(CycleState::PICK_RANDOM);
        }
      }
      break;
  }

  // Periodic delta log
  if (millis() - g_lastLogMS >= LOG_TICK_MS) {
    g_lastLogMS = millis();
    emitLogTick();
  }

  delay(10);   // 100 Hz loop
}
