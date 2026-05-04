/*
 * ============================================================================
 * FIRST WITNESS — modular firmware (refactor of 09_witness)
 * ============================================================================
 *
 * Same behaviour as 09_witness, split into focused modules so the
 * tracking-phase work (Wi-Fi, schedule fetch, satellite-az tracker) doesn't
 * have to reopen the validated calibration / move / spin code.
 *
 * Module layout:
 *   MotorIO.h        Hardware abstraction: I2C primitives, register defines,
 *                    readbacks, PID switching. Header-only namespace.
 *   Recipes.h        Locked PID + trajectory constants (production + diag).
 *   MotorState.h     Shared State enum + MotorState struct.
 *   SinusoidFit.*    Pure math: 3×3 LS sinusoid fit for gravity-up.
 *   Calibration.*    NVS persistence of mass_offset_deg.
 *   Calibrator.*     Homing state machine.
 *   MoveOperator.*   Production move-to-target with cosine ease.
 *   SpinOperator.*   1080° rate-integrated spin + brake.
 *   Logger.*         Delta-encoded compressed log.
 *   main.cpp         Boot ritual orchestration + serial command dispatch.
 *
 * Boot ritual (≈ 110 s, identical to 09):
 *   3 s pause → homing → move-to-zero → 1080 spin → hold at visual zero.
 * ============================================================================
 */

#include <Arduino.h>

#include "Calibration.h"
#include "Calibrator.h"
#include "Logger.h"
#include "MotorIO.h"
#include "MotorState.h"
#include "MoveOperator.h"
#include "Recipes.h"
#include "SpinOperator.h"

static const bool BOOT_HOME_ENABLED = true;

// Power gate (validated 06_mix): the motor needs ≥ 15 V on PWR-485 to
// break static friction with the unbalanced pointer. At 11 V (PD trigger
// half-negotiated) the motor accepts speed commands but the rotor doesn't
// move.
//
// Two thresholds with hysteresis + debounce — without these, transient
// supply sag during spin-start (the brake-cap current bump can pull Vin
// momentarily from 15.03 → 14.93 V) trips the gate and aborts an
// otherwise-fine ritual.
//
//   ENGAGE_VIN  ≥ 15.0 V  required to start (or restart) the ritual.
//   DROP_VIN    < 13.5 V  for N consecutive checks → real power loss,
//                         release motor and wait for ENGAGE_VIN again.
static const float    ENGAGE_VIN_V      = 15.0f;
static const float    DROP_VIN_V        = 13.5f;
static const uint32_t VIN_PRINT_MS      = 1000;
static const uint32_t VIN_CHECK_MS      = 500;
static const int      DROP_DEBOUNCE_N   = 3;     // ~1.5 s of low readings
static uint32_t g_lastVinCheckMS = 0;
static int      g_dropStreak     = 0;
static float    g_lastVinV       = 0.0f;
static bool     g_powerOK        = false;

void waitForPower();
void monitorPower();

// ─── Components ───────────────────────────────────────────────────────────
MotorState   motor;
Calibration  calibration;
MoveOperator moveOp(motor);
SpinOperator spinOp(motor);
Calibrator   calibrator(motor, calibration, moveOp);
Logger       logger(motor);

String inputBuffer;

void processSerialCommand(const String& cmd);
void printReport();

// ─── Setup ────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("============================================");
  Serial.println("  FIRST WITNESS — modular (calibrate + spin)");
  Serial.println("============================================");
  Serial.println("On boot: 3 s pause -> sensorless homing -> move to visual up");
  Serial.println("         -> 1080 deg spin (brake at zero) -> hold.");
  Serial.println("Commands: release | hold | diag | home | spin |");
  Serial.println("          move <deg> | cal | setcal <deg> | savecal | report");
  Serial.println();

  if (!MotorIO::begin()) {
    Serial.println("ERROR: Motor not found at 0x64. Halting.");
    while (true) delay(1000);
  }
  Serial.println("Motor connected.");
  MotorIO::applyProductionPID();

  calibration.begin();
  Serial.print("Mass offset (NVS): ");
  Serial.print(calibration.massOffsetDeg(), 3);
  Serial.println(" deg");

  Serial.println();
  Serial.println("# log schema (delta-encoded; fields omitted = unchanged):");
  Serial.println("#   t=ms  ph=R|H|M|C|P|A|u|S|B|W  Tg=tgtDeg  sp=setptDeg");
  Serial.println("#   p=encDeg  er=spErr  a=actRPM  cur=mA  tmp=tempC  v=vinV");
  Serial.println("Ready.");

  if (BOOT_HOME_ENABLED) {
    Serial.println();
    waitForPower();
    Serial.println(">> Boot ritual: auto-home enabled. Kick in 3 s...");
    delay(3000);
    calibrator.startHomeRitual();
  }
}

// Block in setup until Vin reads ≥ ENGAGE_VIN_V. Prints status once per
// second so a watcher (lab.py) sees what the firmware is waiting for.
void waitForPower() {
  Serial.print(">> Boot: waiting for Vin >= ");
  Serial.print(ENGAGE_VIN_V, 1);
  Serial.println(" V");
  uint32_t lastPrint = 0;
  while (true) {
    g_lastVinV = MotorIO::vinV();
    if (g_lastVinV >= ENGAGE_VIN_V) break;
    if (millis() - lastPrint >= VIN_PRINT_MS) {
      lastPrint = millis();
      Serial.print(">> Waiting: Vin=");
      Serial.print(g_lastVinV, 2);
      Serial.print(" V (need >= ");
      Serial.print(ENGAGE_VIN_V, 1);
      Serial.println(" V)");
    }
    delay(50);
  }
  Serial.print(">> Power OK (Vin=");
  Serial.print(g_lastVinV, 2);
  Serial.println(" V)");
  g_powerOK     = true;
  g_dropStreak  = 0;
}

// Called every loop iteration. Real power loss = Vin below DROP_VIN_V for
// DROP_DEBOUNCE_N consecutive checks (~1.5 s). Transient sags during
// current bumps are absorbed.
void monitorPower() {
  if (millis() - g_lastVinCheckMS < VIN_CHECK_MS) return;
  g_lastVinCheckMS = millis();
  g_lastVinV = MotorIO::vinV();

  if (!g_powerOK) return;                  // we're already in waitForPower()

  if (g_lastVinV < DROP_VIN_V) {
    g_dropStreak++;
    if (g_dropStreak >= DROP_DEBOUNCE_N) {
      Serial.print(">> Vin dropped to ");
      Serial.print(g_lastVinV, 2);
      Serial.print(" V (< ");
      Serial.print(DROP_VIN_V, 1);
      Serial.print(" V for ");
      Serial.print(DROP_DEBOUNCE_N);
      Serial.println(" checks) — releasing motor.");
      calibrator.enterRelease();
      g_powerOK = false;
      waitForPower();
      Serial.println(">> Power restored. Re-running boot ritual.");
      delay(3000);
      calibrator.startHomeRitual();
    }
  } else {
    g_dropStreak = 0;
  }
}

// ─── Loop ─────────────────────────────────────────────────────────────────
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

  // Hardware safety: voltage gate. Disengage motor on low-V; re-home on return.
  monitorPower();

  // Drive the active operator. Each tick no-ops when its phase isn't active.
  calibrator.tick();
  moveOp.tick();
  spinOp.tick();

  // Chain: when a move-to-zero finishes after homing, auto-trigger the spin.
  if (motor.state == ST_HOLDING && moveOp.consumePendingSpin()) {
    spinOp.start();
  }

  logger.emit();
  delay(10);   // 100 Hz loop
}

// ─── Serial commands ──────────────────────────────────────────────────────
void processSerialCommand(const String& cmd) {
  String low = cmd; low.trim(); low.toLowerCase();
  if (low.length() == 0) return;

  if (low == "release") {
    calibrator.enterRelease();
    Serial.println(">> Released. Motor output OFF.");
    return;
  }
  if (low == "hold") {
    calibrator.enterHoldHere();
    Serial.print(">> Holding here. zeroOffset=");
    Serial.println(motor.zeroOffsetCounts);
    return;
  }
  if (low == "diag") {
    calibrator.startDiagOnly();
    return;
  }
  if (low == "home") {
    Serial.println(">> Home: latching current encoder, running diag, applying offset.");
    calibrator.startHomeRitual();
    return;
  }
  if (low == "spin") {
    if (motor.state != ST_HOLDING) {
      Serial.println(">> ERROR: motor must be holding (run `home` or `hold` first).");
      return;
    }
    spinOp.start();
    return;
  }
  if (low == "report")  { printReport(); return; }
  if (low == "cal") {
    Serial.print(">> mass_offset_deg current value: ");
    Serial.print(calibration.massOffsetDeg(), 3);
    Serial.println(" deg");
    return;
  }
  if (low == "savecal") {
    calibration.save();
    Serial.print(">> mass_offset_deg saved to NVS: ");
    Serial.print(calibration.massOffsetDeg(), 3);
    Serial.println(" deg");
    return;
  }
  if (low == "loadcal") {
    calibration.load();
    Serial.print(">> mass_offset_deg loaded from NVS: ");
    Serial.print(calibration.massOffsetDeg(), 3);
    Serial.println(" deg");
    return;
  }
  if (low.startsWith("setcal")) {
    float v = cmd.substring(6).toFloat();
    calibration.setMassOffsetDeg(v);
    Serial.print(">> mass_offset_deg set to ");
    Serial.print(calibration.massOffsetDeg(), 3);
    Serial.println(" (RAM only; run `savecal` to persist)");
    return;
  }
  if (low.startsWith("move")) {
    if (!motor.zeroSet) {
      Serial.println(">> ERROR: hold first (run `hold`) so the zero is defined.");
      return;
    }
    float v = cmd.substring(4).toFloat();
    if (motor.usingDiagPid) {
      MotorIO::safeSwitchToProductionPID();
      motor.usingDiagPid = false;
    }
    float distance = fabsf(v - motor.currentTargetDeg);
    uint32_t mt = static_cast<uint32_t>(distance * Recipes::PROD_MT_PER_DEG_MS);
    if (mt < 200) mt = 200;
    Serial.print(">> move to "); Serial.print(v, 2);
    Serial.print(" deg, mt="); Serial.print(mt); Serial.println(" ms");
    moveOp.moveTo(v, mt);
    return;
  }

  Serial.print(">> Unknown command: ");
  Serial.println(cmd);
}

// ─── Report ───────────────────────────────────────────────────────────────
void printReport() {
  const char* stateName =
      (motor.state == ST_RELEASED)     ? "RELEASED"     :
      (motor.state == ST_HOLDING)      ? "HOLDING"      :
      (motor.state == ST_MOVING)       ? "MOVING"       :
      (motor.state == ST_DIAG_CW)      ? "DIAG_CW"      :
      (motor.state == ST_DIAG_PAUSE)   ? "DIAG_PAUSE"   :
      (motor.state == ST_DIAG_CCW)     ? "DIAG_CCW"     :
      (motor.state == ST_SPIN_RAMP_UP) ? "SPIN_RAMP_UP" :
      (motor.state == ST_SPIN_CRUISE)  ? "SPIN_CRUISE"  :
      (motor.state == ST_SPIN_BRAKE)   ? "SPIN_BRAKE"   :
      (motor.state == ST_SPIN_SETTLE)  ? "SPIN_SETTLE"  : "?";
  Serial.println();
  Serial.println("=== Report ===");
  Serial.print("State:           "); Serial.print(stateName);
  Serial.print(" (zeroSet="); Serial.print(motor.zeroSet ? "yes" : "no");
  Serial.println(")");
  Serial.print("PID profile:     ");
  Serial.println(motor.usingDiagPid ? "diagnostic" : "production");
  Serial.print("Logical target:  "); Serial.print(motor.currentTargetDeg, 2); Serial.println(" deg");
  Serial.print("Encoder raw:     "); Serial.print(MotorIO::encoderCounts());
  Serial.print(" (zero offset "); Serial.print(motor.zeroOffsetCounts); Serial.println(")");
  Serial.print("Encoder user:    "); Serial.print(motor.readEncoderDeg(), 2); Serial.println(" deg");
  Serial.print("Vin:             "); Serial.print(MotorIO::vinV(), 2); Serial.println(" V");
  Serial.print("Motor temp:      "); Serial.print(MotorIO::tempC()); Serial.println(" C");
  Serial.print("Motor current:   "); Serial.print(MotorIO::currentMA(), 0); Serial.println(" mA");
  Serial.print("Actual speed:    "); Serial.print(MotorIO::actualRPM(), 1); Serial.println(" RPM");
  Serial.print("Mass offset:     "); Serial.print(calibration.massOffsetDeg(), 3); Serial.println(" deg");
  Serial.println("==============");
}
