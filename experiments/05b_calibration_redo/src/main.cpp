/*
 * ============================================================================
 * FIRST WITNESS — calibration sketch (no auto-home, no network)
 * ============================================================================
 *
 * Purpose: find a fresh `mass_offset_deg` for a fully-mounted sculpture.
 *
 * Boots into IDLE. The motor stays released until you tell it what to do
 * over serial. No auto-home, no auto-spin, no Wi-Fi, no NTP, no schedule
 * fetch — just the calibration tooling.
 *
 * NVS namespace is identical to 14_witness_provision, so `savecal` here
 * persists into the same key the production firmware reads on its next
 * boot.
 *
 * Recipe (run in serial monitor):
 *   release            -> motor coasts; hand-position pointer at the
 *                         engraved visual-vertical mark on the brass lid
 *   hold               -> latch this position as user 0
 *   diag               -> 2-rev sweep, fits sinusoid, prints
 *                         "fit: gravity-up at user-frame X.XX deg"
 *                         AND amplitude / DC offset for fit quality
 *   setcal X.XX        -> use the printed gravity-up value as the new
 *                         mass_offset_deg (RAM only)
 *   savecal            -> persist to NVS (carries to production firmware)
 *   home               -> verify: runs full ritual, should land at visual
 *                         zero (the engraved mark)
 *
 * Repeat `diag` 2–3 times before `setcal` to confirm fit stability — if
 * three runs give gravity-up within ~0.5° of each other, the fit is solid.
 * ============================================================================
 */

#include <Arduino.h>

#include "Calibration.h"
#include "Calibrator.h"
#include "Logger.h"
#include "MotorIO.h"
#include "MotorState.h"
#include "MoveOperator.h"
#include "PowerGate.h"
#include "Recipes.h"
#include "SpinOperator.h"

static const char* SCULPTURE_NAME = "Orbital Witness 1/12 — CALIBRATION";

// ─── Components ───────────────────────────────────────────────────────────
MotorState     motor;
Calibration    calibration;
MoveOperator   moveOp(motor);
SpinOperator   spinOp(motor);
Calibrator     calibrator(motor, calibration, moveOp);
Logger         logger(motor);
PowerGate      power(calibrator);

String inputBuffer;

void processSerialCommand(const String& cmd);
void printReport();
void printBanner();

// ─── Setup ────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2500);

  printBanner();

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
  Serial.println("--- READY ---");
  Serial.println("Motor is RELEASED (free to hand-position).");
  Serial.println("Type `release | hold | diag | setcal X.XX | savecal | home`.");
  Serial.println("Type `report` for full state. `cal` to re-print mass_offset_deg.");
  Serial.println();

  // Critical difference vs production: motor stays RELEASED.
  // No power gate wait, no auto-home, no auto-spin.
  motor.state    = ST_RELEASED;
  motor.zeroSet  = false;
  MotorIO::setOutput(false);
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

  // Power monitor: if Vin sags, release the motor. We do NOT auto-restart
  // the homing ritual on recovery (calibration mode wants manual control).
  if (power.powerOK()) {
    static uint32_t lastVinCheck = 0;
    if (millis() - lastVinCheck > 500) {
      lastVinCheck = millis();
      float vin = MotorIO::vinV();
      if (vin < 13.0f) {
        Serial.print(">> Vin sagged to ");
        Serial.print(vin, 2);
        Serial.println(" V — releasing motor. Fix power, then `hold` to resume.");
        calibrator.enterRelease();
      }
    }
  }

  calibrator.tick();
  moveOp.tick();
  spinOp.tick();

  // Boot chain: when the post-homing move-to-zero completes, auto-spin
  // (only used by `home` command — diag-only doesn't set pendingSpin).
  if (motor.state == ST_HOLDING && moveOp.consumePendingSpin()) {
    spinOp.start();
  }

  logger.emit();
  delay(10);
}

// ─── Banner / status ──────────────────────────────────────────────────────
void printBanner() {
  Serial.println();
  Serial.println("============================================");
  Serial.print("  ");
  Serial.println(SCULPTURE_NAME);
  Serial.println("============================================");
  Serial.print("Build: ");
  Serial.print(__DATE__);
  Serial.print(" ");
  Serial.println(__TIME__);
  Serial.println("Calibration sketch — boots IDLE, no auto-home.");
  Serial.println("Use this to find mass_offset_deg, then flash production.");
  Serial.println();
  Serial.println("Recipe:");
  Serial.println("  1. release");
  Serial.println("  2. hand-position pointer at engraved visual-up");
  Serial.println("  3. hold");
  Serial.println("  4. diag           (run 2-3 times, check stability)");
  Serial.println("  5. setcal X.XX    (X.XX = printed gravity-up value)");
  Serial.println("  6. savecal");
  Serial.println("  7. home           (verify lands at visual zero)");
  Serial.println();
}

// ─── Serial commands ──────────────────────────────────────────────────────
void processSerialCommand(const String& cmd) {
  String low = cmd; low.trim(); low.toLowerCase();
  if (low.length() == 0) return;

  if (low == "release") {
    calibrator.enterRelease();
    Serial.println(">> Released. Motor output OFF — free to hand-position.");
    return;
  }
  if (low == "hold") {
    calibrator.enterHoldHere();
    Serial.print(">> Holding here. zeroOffsetCounts=");
    Serial.println(motor.zeroOffsetCounts);
    return;
  }
  if (low == "diag") {
    calibrator.startDiagOnly();
    return;
  }
  if (low == "home") {
    Serial.println(">> Home: latching current encoder, running diag, applying offset, spinning.");
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
    Serial.println(" deg  (carries to production firmware)");
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
  Serial.print("Encoder user:    "); Serial.print(motor.readEncoderDeg(), 2); Serial.println(" deg");
  Serial.print("Vin:             "); Serial.print(MotorIO::vinV(), 2); Serial.println(" V");
  Serial.print("Motor temp:      "); Serial.print(MotorIO::tempC()); Serial.println(" C");
  Serial.print("Mass offset:     "); Serial.print(calibration.massOffsetDeg(), 3); Serial.println(" deg");
  Serial.println("==============");
}
