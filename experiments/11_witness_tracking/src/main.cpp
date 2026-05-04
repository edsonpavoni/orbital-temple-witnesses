/*
 * ============================================================================
 * FIRST WITNESS — modular firmware + Wi-Fi tracking (Stage 1)
 * ============================================================================
 *
 * On every power-on:
 *   1. Power gate (Vin ≥ 15 V).
 *   2. Wi-Fi connect + NTP sync.
 *   3. ScheduleClient: if cached schedule is stale or missing, fetch a
 *      fresh 24 h schedule from /api/schedule. If the network is down
 *      AND we have cached data, fall back to cached (per Edson's spec).
 *   4. 3-second pre-delay.
 *   5. Sensorless homing (Calibrator) → re-anchor zero = visual vertical.
 *   6. 1080° spin (SpinOperator) → land at visual zero.
 *   7. Tracker: continuously update the position setpoint from the
 *      cached schedule + current UTC. Pointer follows the satellite's
 *      azimuth.
 *
 * Module map:
 *   MotorIO / Recipes / MotorState     — hardware abstraction
 *   SinusoidFit / Calibration          — math + NVS
 *   Calibrator / MoveOperator / SpinOperator   — motion subsystems
 *   Logger                              — delta-encoded serial log
 *   PowerGate                          — voltage interlock
 *   Network                            — Wi-Fi + NTP
 *   ScheduleStore / ScheduleClient     — schedule fetch + cache
 *   Tracker                            — drives setpoint from schedule
 *   main.cpp                           — orchestration only
 * ============================================================================
 */

#include <Arduino.h>

#include "Calibration.h"
#include "Calibrator.h"
#include "Logger.h"
#include "MotorIO.h"
#include "MotorState.h"
#include "MoveOperator.h"
#include "Network.h"
#include "PowerGate.h"
#include "Recipes.h"
#include "ScheduleClient.h"
#include "ScheduleStore.h"
#include "SpinOperator.h"
#include "Tracker.h"
#include "secrets.h"

static const bool BOOT_HOME_ENABLED  = true;
static const bool BOOT_TRACK_ENABLED = true;

// ─── Components ───────────────────────────────────────────────────────────
MotorState     motor;
Calibration    calibration;
MoveOperator   moveOp(motor);
SpinOperator   spinOp(motor);
Calibrator     calibrator(motor, calibration, moveOp);
Logger         logger(motor);
PowerGate      power(calibrator);
ScheduleStore  schedule;
ScheduleClient scheduleClient;
Tracker        tracker(motor, moveOp, schedule);

String inputBuffer;

void processSerialCommand(const String& cmd);
void printReport();
void tryFetchScheduleIfDue();

// ─── Setup ────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("============================================");
  Serial.println("  FIRST WITNESS — calibrate + spin + track");
  Serial.println("============================================");
  Serial.println("Boot: power gate -> wifi+ntp -> daily fetch -> homing");
  Serial.println("      -> 1080 spin -> tracking (follow the satellite).");
  Serial.println("Commands: release | hold | diag | home | spin |");
  Serial.println("          move <deg> | track | untrack | fetch |");
  Serial.println("          cal | setcal <deg> | savecal | report");
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

  schedule.begin();
  if (schedule.hasData()) {
    Serial.print(">> Schedule (NVS): ");
    Serial.print(schedule.header().samples_count);
    Serial.print(" samples, valid_from=");
    Serial.print(schedule.header().valid_from_utc);
    Serial.print(", fetched_at=");
    Serial.println(schedule.header().fetched_at_utc);
  } else {
    Serial.println(">> Schedule (NVS): none cached.");
  }

  Serial.println();
  Serial.println("# log schema (delta-encoded; fields omitted = unchanged):");
  Serial.println("#   t=ms  ph=R|H|M|C|P|A|u|S|B|W|T  Tg=tgtDeg  sp=setptDeg");
  Serial.println("#   p=encDeg  er=spErr  a=actRPM  cur=mA  tmp=tempC  v=vinV");
  Serial.println("Ready.");

  // Wi-Fi + NTP. We try once at boot; if either fails, we'll fall back to
  // cached schedule (if any) and keep retrying in the background.
  if (Network::connect()) {
    Network::syncTime();
  } else {
    Serial.println(">> Continuing without network. Will retry in background.");
  }
  tryFetchScheduleIfDue();

  if (BOOT_HOME_ENABLED) {
    Serial.println();
    power.waitForPower();
    Serial.println(">> Boot ritual: auto-home enabled. Kick in 3 s...");
    delay(3000);
    calibrator.startHomeRitual();
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

  // Hardware safety: voltage gate. Disengage motor on real low-V; re-home
  // on return.
  power.monitorPower();

  // Drive the active operator. Each tick no-ops outside its own phases.
  calibrator.tick();
  moveOp.tick();
  spinOp.tick();
  tracker.tick(Network::nowUTC());

  // Boot chain: when the post-homing move-to-zero completes, auto-spin.
  if (motor.state == ST_HOLDING && moveOp.consumePendingSpin()) {
    spinOp.start();
  }

  // After the boot spin lands, switch to tracking mode (if enabled and we
  // have a schedule). MoveOperator's tick already wrote currentTargetDeg=0
  // when the spin's settle phase ended (via SpinOperator → state=ST_HOLDING).
  static bool s_trackingArmed = BOOT_TRACK_ENABLED;
  if (s_trackingArmed && motor.state == ST_HOLDING &&
      Network::isTimeSynced() && schedule.hasData()) {
    s_trackingArmed = false;
    tracker.enable();
  }

  // Background: every loop, give the network a chance to reconnect.
  Network::tick();

  // Once a day, re-fetch the schedule. Cheap to call: returns immediately
  // if not due yet.
  static uint32_t lastFetchAttemptMS = 0;
  if (millis() - lastFetchAttemptMS > 60UL * 60UL * 1000UL) {
    lastFetchAttemptMS = millis();
    tryFetchScheduleIfDue();
  }

  logger.emit();
  delay(10);
}

// ─── Schedule fetch helper ────────────────────────────────────────────────
void tryFetchScheduleIfDue() {
  if (!Network::isConnected() || !Network::isTimeSynced()) {
    Serial.println(">> Schedule fetch: network/time not ready, skipping.");
    return;
  }
  uint32_t now = Network::nowUTC();
  if (!schedule.fetchDue(now)) {
    Serial.println(">> Schedule fetch: not due yet.");
    return;
  }
  if (!scheduleClient.fetch(schedule, OBSERVER_LAT, OBSERVER_LON, now)) {
    if (schedule.hasData()) {
      Serial.println(">> Schedule fetch failed; keeping cached schedule.");
    } else {
      Serial.println(">> Schedule fetch failed and no cache. Tracking unavailable.");
    }
  }
}

// ─── Serial commands ──────────────────────────────────────────────────────
void processSerialCommand(const String& cmd) {
  String low = cmd; low.trim(); low.toLowerCase();
  if (low.length() == 0) return;

  if (low == "release") {
    tracker.disable();
    calibrator.enterRelease();
    Serial.println(">> Released. Motor output OFF.");
    return;
  }
  if (low == "hold") {
    tracker.disable();
    calibrator.enterHoldHere();
    Serial.print(">> Holding here. zeroOffset=");
    Serial.println(motor.zeroOffsetCounts);
    return;
  }
  if (low == "diag") {
    tracker.disable();
    calibrator.startDiagOnly();
    return;
  }
  if (low == "home") {
    tracker.disable();
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
  if (low == "track") {
    if (!schedule.hasData()) {
      Serial.println(">> ERROR: no cached schedule. Run `fetch` first.");
      return;
    }
    if (!Network::isTimeSynced()) {
      Serial.println(">> ERROR: time not synced (NTP). Cannot track.");
      return;
    }
    if (motor.state != ST_HOLDING) {
      Serial.println(">> ERROR: motor must be holding.");
      return;
    }
    tracker.enable();
    return;
  }
  if (low == "untrack") {
    tracker.disable();
    return;
  }
  if (low == "fetch") {
    tryFetchScheduleIfDue();
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
    tracker.disable();
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
      (motor.state == ST_SPIN_SETTLE)  ? "SPIN_SETTLE"  :
      (motor.state == ST_TRACKING)     ? "TRACKING"     : "?";
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
  Serial.print("WiFi:            "); Serial.println(Network::isConnected() ? "connected" : "disconnected");
  Serial.print("NTP synced:      "); Serial.println(Network::isTimeSynced() ? "yes" : "no");
  Serial.print("UTC now:         "); Serial.println(Network::nowUTC());
  if (schedule.hasData()) {
    Serial.print("Schedule:        valid ");
    Serial.print(schedule.header().valid_from_utc);
    Serial.print(" .. ");
    Serial.print(schedule.header().valid_until_utc);
    Serial.print(", fetched_at=");
    Serial.println(schedule.header().fetched_at_utc);
    if (motor.state == ST_TRACKING) {
      Serial.print("Tracking az/el:  ");
      Serial.print(tracker.lastAz(), 2);
      Serial.print(" / ");
      Serial.print(tracker.lastEl(), 2);
      Serial.println(" deg");
    }
  } else {
    Serial.println("Schedule:        (none cached)");
  }
  Serial.println("==============");
}
