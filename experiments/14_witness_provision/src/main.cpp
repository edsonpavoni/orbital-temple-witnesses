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
#include "Geolocation.h"
#include "Logger.h"
#include "MotorIO.h"
#include "MotorState.h"
#include "MoveOperator.h"
#include "Network.h"
#include "PowerGate.h"
#include "Provisioning.h"
#include "Recipes.h"
#include "ScheduleClient.h"
#include "ScheduleStore.h"
#include "SpinOperator.h"
#include "Tracker.h"
#include "WifiCreds.h"
#include "secrets.h"

static const bool BOOT_HOME_ENABLED  = true;
static const bool BOOT_TRACK_ENABLED = true;

// Observer location used by the schedule fetch. Three-step priority:
//   1. NVS (set on first online boot via IP geolocation; persists forever).
//   2. IP geolocation (one-time, on first boot with Wi-Fi).
//   3. Hardcoded fallback in secrets.h (only if both 1 and 2 fail).
// Once persisted to NVS the sculpture knows where it is even if Wi-Fi
// never comes back — exactly like the schedule cache.
static float g_observerLat   = OBSERVER_LAT;
static float g_observerLon   = OBSERVER_LON;
static bool  g_locationKnown = false;

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
void forceFetchScheduleNow();

// ─── Setup ────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("============================================");
  Serial.println("  " SCULPTURE_NAME " — calibrate + spin + track");
  Serial.println("============================================");
  Serial.println("Boot: power gate -> wifi+ntp -> daily fetch -> homing");
  Serial.println("      -> 1080 spin -> tracking (follow the satellite).");
  Serial.println("Commands: release | hold | diag | home | spin |");
  Serial.println("          move <deg> | track | untrack | fetch |");
  Serial.println("          geo | regeo | forget_wifi |");
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

  // Load persisted observer location (set by IP geolocation on a previous
  // boot). On a brand-new device this is empty and we'll fetch via geo-IP
  // after Wi-Fi is up; until then the hardcoded default in secrets.h is
  // used as the fallback.
  if (Geolocation::loadFromNVS(g_observerLat, g_observerLon)) {
    g_locationKnown = true;
    Serial.print(">> Observer (NVS): lat=");
    Serial.print(g_observerLat, 4);
    Serial.print(" lon=");
    Serial.println(g_observerLon, 4);
  } else {
    Serial.print(">> Observer: using fallback lat=");
    Serial.print(g_observerLat, 4);
    Serial.print(" lon=");
    Serial.print(g_observerLon, 4);
    Serial.println(" (will geolocate once Wi-Fi is up)");
  }

  Serial.println();
  Serial.println("# log schema (delta-encoded; fields omitted = unchanged):");
  Serial.println("#   t=ms  ph=R|H|M|C|P|A|u|S|B|W|T  Tg=tgtDeg  sp=setptDeg");
  Serial.println("#   p=encDeg  er=spErr  a=actRPM  cur=mA  tmp=tempC  v=vinV");
  Serial.println("Ready.");

  // ─── Wi-Fi credentials: NVS first, secrets.h fallback, else portal ──────
  // Production deployment: secrets.h has WIFI_SSID = "PROVISION_ME" so the
  // first boot raises the captive portal. The user enters their Wi-Fi creds
  // through a webpage on the SoftAP, we save them in NVS, and every
  // subsequent boot loads from NVS without ever consulting secrets.h.
  // Developer convenience: if secrets.h has real creds, they're used as the
  // fallback so a reflashed dev board doesn't force the portal each time.
  String wifiSSID, wifiPW;
  const char* wifiCredsSource = "unknown";
  bool   credsLoaded = WifiCreds::loadFromNVS(wifiSSID, wifiPW);
  if (credsLoaded) {
    wifiCredsSource = "NVS";
    Serial.print(">> WiFi creds: from NVS, SSID='");
    Serial.print(wifiSSID);
    Serial.println("'");
  } else if (strcmp(WIFI_SSID, "PROVISION_ME") != 0 && strlen(WIFI_SSID) > 0) {
    wifiSSID    = WIFI_SSID;
    wifiPW      = WIFI_PASSWORD;
    credsLoaded = true;
    wifiCredsSource = "secrets.h";
    Serial.println(">> WiFi creds: from secrets.h fallback (no NVS yet)");
  } else {
    Serial.println(">> WiFi creds: none — entering captive portal");
    Provisioning::runPortal();   // [[noreturn]] — reboots after save
  }

  // Wi-Fi + NTP. Boot tries once; if either fails we fall through with
  // whatever we have cached. Background hourly retries handle eventual
  // recovery.
  if (Network::connect(wifiSSID.c_str(), wifiPW.c_str())) {
    Network::syncTime();
  } else {
    Serial.println(">> Continuing without network. Will retry hourly in background.");
  }

  // First-online IP geolocation. If we don't have a persisted location AND
  // we're connected, ask ipapi.co where this Wi-Fi gateway is and persist.
  // After this point, Geolocation::loadFromNVS will succeed on every
  // future boot — even if Wi-Fi never comes back.
  if (!g_locationKnown && Network::isConnected()) {
    if (Geolocation::fetchAndStore(g_observerLat, g_observerLon, Network::nowUTC())) {
      g_locationKnown = true;
    }
  }
  tryFetchScheduleIfDue();

  // Replay-mode anchor. If NTP never synced AND we have a cached schedule,
  // pretend "now" is the schedule's valid_from. The tracker will replay
  // that day's motion in real-time, looping forever (the ScheduleStore
  // wraps lookups modulo the schedule's span). When NTP eventually
  // succeeds via the background SNTP poll, real UTC silently overrides.
  if (!Network::isTimeSynced() && schedule.hasData()) {
    Network::anchorVirtualUTC(schedule.header().valid_from_utc);
    Serial.print(">> Replay mode: looping cached schedule ");
    Serial.print(schedule.header().valid_from_utc);
    Serial.print(" .. ");
    Serial.print(schedule.header().valid_until_utc);
    Serial.println(" until NTP returns.");
  }

  // ─── Consolidated boot status ─────────────────────────────────────────
  // Single snapshot of every subsystem state, printed BEFORE the power
  // gate blocks. If anything looks wrong here, the boot ritual won't help.
  Serial.println();
  Serial.println("============================================");
  Serial.println("  >>> SYSTEM STATUS (pre-boot ritual) <<<");
  Serial.println("============================================");
  Serial.print("Build:           "); Serial.print(__DATE__); Serial.print(" "); Serial.println(__TIME__);
  Serial.print("Sculpture:       "); Serial.println(SCULPTURE_NAME);
  Serial.print("Free heap:       "); Serial.print(ESP.getFreeHeap()); Serial.println(" bytes");
  Serial.println();
  Serial.println("-- Motor --");
  Serial.println("I2C:             [OK] ACK at 0x64 (SDA=GPIO8, SCL=GPIO9)");
  Serial.print("Vin:             "); Serial.print(MotorIO::vinV(), 2);
    Serial.print(" V  (need >= "); Serial.print(PowerGate::ENGAGE_VIN_V, 1);
    Serial.println(" V to start)");
  Serial.print("Temp:            "); Serial.print(MotorIO::tempC()); Serial.println(" C");
  Serial.println();
  Serial.println("-- Calibration --");
  Serial.print("mass_offset_deg: "); Serial.print(calibration.massOffsetDeg(), 3);
    Serial.println(" deg (NVS)");
  Serial.println();
  Serial.println("-- Schedule --");
  if (schedule.hasData()) {
    Serial.print("Cache:           [OK] ");
    Serial.print(schedule.header().samples_count);
    Serial.println(" samples");
    Serial.print("Valid:           ");
    Serial.print(schedule.header().valid_from_utc);
    Serial.print(" .. ");
    Serial.println(schedule.header().valid_until_utc);
    Serial.print("Fetched at:      ");
    Serial.println(schedule.header().fetched_at_utc);
  } else {
    Serial.println("Cache:           [EMPTY] no schedule cached");
  }
  Serial.println();
  Serial.println("-- Observer --");
  Serial.print("Location:        lat=");
  Serial.print(g_observerLat, 4);
  Serial.print(" lon=");
  Serial.print(g_observerLon, 4);
  Serial.println(g_locationKnown ? " (NVS)" : " (fallback default)");
  Serial.println();
  Serial.println("-- Network --");
  Serial.print("WiFi creds src:  "); Serial.println(wifiCredsSource);
  Serial.print("WiFi SSID:       "); Serial.println(wifiSSID);
  Serial.print("WiFi state:      "); Serial.println(Network::isConnected() ? "[OK] connected" : "[--] disconnected");
  Serial.print("NTP synced:      "); Serial.println(Network::isTimeSynced() ? "[OK] yes" : "[--] no");
  Serial.print("Clock mode:      ");
  Serial.println(Network::isTimeSynced() ? "real (NTP)"
                : Network::isVirtualClock() ? "virtual (replay)"
                : "none");
  Serial.print("UTC now:         "); Serial.println(Network::nowUTC());
  Serial.println();
  Serial.println("-- Boot flags --");
  Serial.print("BOOT_HOME:       "); Serial.println(BOOT_HOME_ENABLED ? "true" : "false");
  Serial.print("BOOT_TRACK:      "); Serial.println(BOOT_TRACK_ENABLED ? "true" : "false");
  Serial.println("============================================");
  Serial.println();

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
  // have a schedule + a clock — real NTP or virtual anchor, doesn't matter
  // for the tracker which one).
  static bool s_trackingArmed = BOOT_TRACK_ENABLED;
  if (s_trackingArmed && motor.state == ST_HOLDING &&
      schedule.hasData() && Network::nowUTC() != 0) {
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
  if (!scheduleClient.fetch(schedule, g_observerLat, g_observerLon, now)) {
    if (schedule.hasData()) {
      Serial.println(">> Schedule fetch failed; keeping cached schedule.");
    } else {
      Serial.println(">> Schedule fetch failed and no cache. Tracking unavailable.");
    }
  }
}

// Bypass the 24 h fetchDue policy — pull a fresh schedule right now even
// if the cached one isn't expired. Useful when sculpture and visualization
// disagree because the server's TLE has moved on. Same network/time gates
// as the regular fetch.
void forceFetchScheduleNow() {
  if (!Network::isConnected() || !Network::isTimeSynced()) {
    Serial.println(">> forcefetch: network/time not ready, can't fetch.");
    return;
  }
  uint32_t now = Network::nowUTC();
  Serial.println(">> forcefetch: bypassing fetchDue, pulling fresh schedule...");
  if (scheduleClient.fetch(schedule, g_observerLat, g_observerLon, now)) {
    Serial.println(">> forcefetch: OK.");
  } else {
    Serial.println(">> forcefetch: FAILED (keeping previous cache).");
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
    if (Network::nowUTC() == 0) {
      Serial.println(">> ERROR: no clock (no NTP, no virtual anchor). Cannot track.");
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
  if (low == "forcefetch") {
    forceFetchScheduleNow();
    return;
  }
  if (low == "geo") {
    Serial.print(">> Observer: lat=");
    Serial.print(g_observerLat, 4);
    Serial.print(" lon=");
    Serial.print(g_observerLon, 4);
    Serial.println(g_locationKnown ? " (NVS)" : " (fallback default)");
    return;
  }
  if (low == "regeo") {
    // Wipe NVS + force a fresh IP geolocation lookup. Useful when the
    // sculpture is moved to a new physical site.
    Geolocation::forget();
    g_locationKnown = false;
    if (Network::isConnected()) {
      if (Geolocation::fetchAndStore(g_observerLat, g_observerLon, Network::nowUTC())) {
        g_locationKnown = true;
      }
    } else {
      Serial.println(">> regeo: WiFi not connected — location will be re-fetched on next online boot.");
    }
    return;
  }
  if (low == "forget_wifi") {
    // Wipe stored credentials and reboot. Next boot, with secrets.h still
    // set to PROVISION_ME (production) the captive portal will run; with
    // dev creds in secrets.h, those will be used as fallback.
    WifiCreds::forget();
    Serial.println(">> forget_wifi: rebooting in 1 s.");
    delay(1000);
    ESP.restart();
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
  Serial.print("Observer:        ");
  Serial.print(g_observerLat, 4);
  Serial.print(", ");
  Serial.print(g_observerLon, 4);
  Serial.println(g_locationKnown ? " (NVS)" : " (fallback default)");
  Serial.print("WiFi:            "); Serial.println(Network::isConnected() ? "connected" : "disconnected");
  Serial.print("NTP synced:      "); Serial.println(Network::isTimeSynced() ? "yes" : "no");
  Serial.print("Clock mode:      ");
  Serial.println(Network::isTimeSynced() ? "real (NTP)"
                : Network::isVirtualClock() ? "virtual (replay)"
                : "none");
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
