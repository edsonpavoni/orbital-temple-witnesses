/*
 * ============================================================================
 * ORBITAL TEMPLE — SENSORLESS GRAVITY-BASED HOMING (CALIBRATION)
 * ============================================================================
 *
 * Forked & stripped from 04_position_lab. Discovers the absolute
 * orientation of the unbalanced pointer on every power-up by analysing the
 * gravity signature in motor-current draw during a two-direction rotation,
 * then rotates the pointer to true visual vertical — fully autonomous, no
 * hand-positioning required.
 *
 * VALIDATED 2026-04-29 (final test, 3 power-cycles from random starts):
 *   Errors after auto-home: +2 deg, +1 deg, -2 deg.
 *   Mean error ~0.3 deg, accuracy +/- 2 deg from any starting position.
 *
 * ============================================================================
 * BOOT RITUAL
 * ============================================================================
 *
 *   1. Boot, init motor, load `mass_offset_deg` from NVS (default 10.5; we
 *      empirically fit 10.83 deg for this specific pointer).
 *   2. Wait 3 s ("Kick in 3 s..." printed) so the user can step back.
 *   3. enterHoldHere — latch whatever encoder reading exists right now as
 *      user-frame zero. (It does not matter what physical angle this is.)
 *   4. safeSwitchPid(true) — switch to the looser diagnostic PID so the
 *      gravity-induced lag becomes visible in current draw.
 *   5. Diag CW: spin 720 deg (2 full revolutions). The first revolution
 *      breaks static friction and accelerates the rotor into steady-state
 *      spin; samples from rev 1 are NOT fed to the sinusoid fit.
 *   6. Pause 1.5 s.
 *   7. Diag CCW: spin -720 deg back. Again only rev 2 samples feed the fit.
 *   8. End of CCW: solve the on-board sinusoid LS fit, recover gravity-up
 *      angle in working frame. Apply mass_offset_deg to compute visual-up.
 *      Re-anchor zeroOffsetCounts so user-frame 0 = visual-up. Switch back
 *      to production PID safely. Move to user 0 with the production recipe.
 *   9. State: ST_HOLDING at visual-up. Done. ~80 s total.
 *
 * ============================================================================
 * CONTROL RECIPES
 * ============================================================================
 *
 *   Production (move-to-target after homing):
 *     kp=30,000,000  ki=1,000  kd=40,000,000  mc=1000 mA
 *     easing=EASE_IN_OUT, mt=33.33 ms per deg of move distance
 *
 *   Diagnostic (used during the calibration sweeps only):
 *     kp=1,500,000   ki=30     kd=40,000,000  mc=1000 mA
 *     easing=EASE_IN_OUT, mt=50 ms/deg (~5 RPM avg trajectory speed)
 *
 * ============================================================================
 * COMMANDS (serial, 115200 8N1)
 * ============================================================================
 *
 *   release          Disable motor output (free-coast).
 *   hold             Latch current encoder as zero, hold in position mode.
 *   diag             Run a 2-rev sweep without applying offset (research only).
 *   home             Run the homing routine + move to visual-up.
 *   move <deg>       Move to user-frame angle (production recipe).
 *   report           Print live readbacks + state.
 *   cal              Print current mass_offset_deg.
 *   setcal <deg>     Set mass_offset_deg in RAM only.
 *   savecal          Persist current mass_offset_deg to NVS.
 *   loadcal          Reload mass_offset_deg from NVS.
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>

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
#define REG_CURRENT_READ 0xC0   // Motor current readback (int32, mA*100)

#define MODE_POSITION    2

#define COUNTS_PER_DEG   100    // 36 000 counts per 360 deg

// =============================================================================
// LOCKED RECIPE (production move-to-target)
// =============================================================================

const int32_t  REC_KP      = 30000000;
const int32_t  REC_KI      = 1000;
const int32_t  REC_KD      = 40000000;
const int32_t  REC_MC_MA   = 1000;
const float    REC_MT_PER_DEG_MS = 33.33f;

// =============================================================================
// DIAGNOSTIC RECIPE (Phase 0 — looser PID, slower trajectory)
// =============================================================================
//
// Rationale: with the production PID the motor tracks so tightly that the
// position error never exceeds ~5 deg, masking the gravity signature in
// the lag signal. For diagnostics we want visible asymmetry, so we run a
// looser PID at a slower trajectory — gravity-induced lag becomes much
// larger and the sinusoid becomes legible to the eye.

const int32_t  DIAG_KP      = 1500000;
const int32_t  DIAG_KI      = 30;
const int32_t  DIAG_KD      = 40000000;
const int32_t  DIAG_MC_MA   = 1000;
const float    DIAG_MT_PER_DEG_MS = 50.0f;   // ~5 RPM avg trajectory speed

const uint32_t DIAG_PAUSE_MS = 1500;          // settle between CW and CCW

// 2 full revolutions per direction; only the second revolution feeds the
// sinusoid fit. This eliminates start-of-move stick-slip transients which
// otherwise position-lock to the latching encoder and bias the fit.
const float DIAG_REVS_PER_DIRECTION = 2.0f;
const float DIAG_FIT_SKIP_FRACTION  = 0.5f;   // skip first rev (transient)

// =============================================================================
// MASS-OFFSET CALIBRATION (persisted in ESP32 NVS)
// =============================================================================
//
// The pointer's heavy mass is not perfectly aligned with the visible pointer
// tip. The diagnostic sweep finds the gravity-equilibrium "up" (where the
// heavy mass would naturally settle if straight up); the visual "up" (where
// the pointer tip points straight up) is offset from that by a fixed angle
// determined by the pointer's mass distribution.
//
// Default 10.5 deg measured from 3 averaged diag runs on 2026-04-29.
// Override via `setcal <deg>` and persist via `savecal`. Stored in NVS so
// it survives power cycles.

#define MASS_OFFSET_DEG_DEFAULT  10.5f

Preferences g_prefs;
float g_massOffsetDeg = MASS_OFFSET_DEG_DEFAULT;

// Set to true if you want the firmware to auto-home on boot (run diag,
// fit gravity, apply offset, move to visual up). Disabled by default during
// development so you can iterate without the boot ritual every flash.
const bool BOOT_HOME_ENABLED = true;

// =============================================================================
// EASING — only EASE_IN_OUT is needed
// =============================================================================

float easeInOut(float t) {
  return 0.5f * (1.0f - cos(t * PI));
}

// =============================================================================
// SINUSOID FIT — accumulators + 3x3 LS solver
// =============================================================================
//
// During the CW and CCW sweeps we accumulate samples (encoder_deg, cur_mA).
// At the end of CCW we solve a least-squares fit of:
//
//     cur(angle) = a*sin(angle) + b*cos(angle) + dc
//
// where `angle` is the encoder reading in radians (mod 2π). The friction
// component is direction-anti-symmetric and projects to ~zero on this basis
// when CW and CCW samples are roughly balanced (which they are by
// construction — same trajectory profile each direction). The gravity
// component projects directly onto sin/cos, giving us amplitude and phase.
//
// Then `up_rad = -atan2(b, a)` — the unstable equilibrium where the heavy
// mass is straight up against gravity.

float fit_Sxx, fit_Syy, fit_Sxy;
float fit_Sx, fit_Sy;
float fit_Sxz, fit_Syz, fit_Sz;
int   fit_N;

void fitReset() {
  fit_Sxx = fit_Syy = fit_Sxy = 0.0f;
  fit_Sx = fit_Sy = 0.0f;
  fit_Sxz = fit_Syz = fit_Sz = 0.0f;
  fit_N = 0;
}

void fitAccumulate(float angleDeg, float signal) {
  float a = fmodf(angleDeg, 360.0f);
  if (a < 0) a += 360.0f;
  float r = a * (PI / 180.0f);
  float s = sinf(r);
  float c = cosf(r);
  fit_Sxx += s * s;
  fit_Syy += c * c;
  fit_Sxy += s * c;
  fit_Sx  += s;
  fit_Sy  += c;
  fit_Sxz += s * signal;
  fit_Syz += c * signal;
  fit_Sz  += signal;
  fit_N   += 1;
}

bool fitSolve(float &a, float &b, float &dc) {
  // Solve  M * [a,b,dc]^T = r  via Gauss elimination.
  // M = [[Sxx,Sxy,Sx],[Sxy,Syy,Sy],[Sx,Sy,N]] ;  r = [Sxz,Syz,Sz]
  if (fit_N < 4) return false;
  float M[3][4] = {
    {fit_Sxx, fit_Sxy, fit_Sx,         fit_Sxz},
    {fit_Sxy, fit_Syy, fit_Sy,         fit_Syz},
    {fit_Sx,  fit_Sy,  (float)fit_N,   fit_Sz},
  };
  for (int i = 0; i < 3; i++) {
    int max_row = i;
    for (int k = i + 1; k < 3; k++) {
      if (fabsf(M[k][i]) > fabsf(M[max_row][i])) max_row = k;
    }
    if (fabsf(M[max_row][i]) < 1e-9f) return false;
    if (max_row != i) {
      for (int j = 0; j < 4; j++) {
        float tmp = M[i][j]; M[i][j] = M[max_row][j]; M[max_row][j] = tmp;
      }
    }
    for (int k = i + 1; k < 3; k++) {
      float factor = M[k][i] / M[i][i];
      for (int j = i; j < 4; j++) M[k][j] -= factor * M[i][j];
    }
  }
  float sol[3];
  for (int i = 2; i >= 0; i--) {
    float s = M[i][3];
    for (int j = i + 1; j < 3; j++) s -= M[i][j] * sol[j];
    sol[i] = s / M[i][i];
  }
  a = sol[0]; b = sol[1]; dc = sol[2];
  return true;
}

// Returns gravity-up angle in degrees (0-360), in the current user frame.
// Returns NAN on failure.
float fitGravityUpDeg() {
  float a, b, dc;
  if (!fitSolve(a, b, dc)) return NAN;
  float c = atan2f(b, a);
  float upRad = -c;
  float upDeg = upRad * (180.0f / PI);
  upDeg = fmodf(upDeg + 720.0f, 360.0f);
  return upDeg;
}

// =============================================================================
// STATE
// =============================================================================

enum State {
  ST_RELEASED = 0,
  ST_HOLDING,
  ST_MOVING,
  ST_DIAG_CW,        // diagnostic: spinning CW
  ST_DIAG_PAUSE,     // diagnostic: settle between CW and CCW
  ST_DIAG_CCW,       // diagnostic: spinning CCW
};

// One-letter codes for the log
char phaseCode(State s) {
  switch (s) {
    case ST_RELEASED:   return 'R';
    case ST_HOLDING:    return 'H';
    case ST_MOVING:     return 'M';
    case ST_DIAG_CW:    return 'C';
    case ST_DIAG_PAUSE: return 'P';
    case ST_DIAG_CCW:   return 'A';   // Anti-clockwise
  }
  return '?';
}

State    state            = ST_RELEASED;
bool     zeroSet          = false;
int32_t  zeroOffsetCounts = 0;       // Encoder raw counts mapping to user 0 deg

// Current move trajectory (used by both production move and diagnostic spin)
float    moveStartDeg     = 0.0f;
float    moveEndDeg       = 0.0f;
uint32_t moveStartMS      = 0;
uint32_t moveDurMS        = 0;
float    currentTargetDeg = 0.0f;    // logical target while holding

// Diagnostic context
uint32_t pauseStartMS     = 0;
uint32_t pauseDurMS       = 0;
float    diagStartDeg     = 0.0f;    // angle at start of diag (latched)
bool     usingDiagPid     = false;   // tracks which PID is currently in motor
bool     diagApplyHoming  = false;   // if true, end-of-CCW applies offset + moves to visual-up

String   inputBuffer;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

void motorBootInit();
void enterRelease();
void enterHoldHere();
void startMoveTo(float toDeg, uint32_t mt);
void startDiag();
void startHomeRitual();
void updateMotion();
void loadMassOffset();
void saveMassOffset();

void writeTargetDeg(float deg);
float readEncoderDeg();
float motorGetVinV();
int32_t motorGetTempC();
float motorGetSpeedRPM();
float motorGetCurrentMA();
void writePosMaxCurrent(int32_t mA);
void writePosPID(int32_t p, int32_t i, int32_t d);

void applyProductionPid();
void applyDiagnosticPid();
void safeSwitchPid(bool toDiag);

void writeReg8(uint8_t reg, uint8_t value);
void writeReg32(uint8_t reg, int32_t value);
int32_t readReg32(uint8_t reg);
void writeReg96(uint8_t reg, int32_t v1, int32_t v2, int32_t v3);

void processSerialCommand(String cmd);
void printReport();
void emitLogDelta();

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("============================================");
  Serial.println("  ORBITAL TEMPLE — CALIBRATION FW (Phase 0)");
  Serial.println("============================================");
  Serial.println("Commands: release | hold | diag | home | move <deg> | report");
  Serial.println();

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: Motor not found at 0x64. Halting.");
    while (1) delay(1000);
  }
  Serial.println("Motor connected.");

  motorBootInit();
  applyProductionPid();   // start in production mode; diag flips it temporarily

  // Load persisted mass-offset from ESP32 NVS (default if never saved)
  loadMassOffset();

  // Print compressed-log schema
  Serial.println();
  Serial.println("# log schema (delta-encoded; fields omitted = unchanged):");
  Serial.println("#   t=ms  ph=R|H|M|C|P|A  Tg=tgtDeg  sp=setptDeg");
  Serial.println("#   p=encDeg  er=spErr  a=actRPM  cur=mA  tmp=tempC  v=vinV");
  Serial.println("#   ph: R=released H=holding M=moving");
  Serial.println("#       C=diag_cw P=diag_pause A=diag_ccw");
  Serial.println("Ready.");

  // Boot-time auto-home ritual: take whatever the encoder reads now as the
  // working frame, run a diag sweep, fit gravity-up, apply the persisted
  // mass offset, and move to visual-up. The user does not need to touch
  // the pointer — the algorithm does it itself.
  if (BOOT_HOME_ENABLED) {
    Serial.println();
    Serial.println(">> Boot ritual: auto-home enabled. Kick in 3 s...");
    delay(3000);
    startHomeRitual();
  }
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

  updateMotion();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    emitLogDelta();
  }

  delay(10);
}

// =============================================================================
// COMMAND HANDLERS
// =============================================================================

void cmdRelease() {
  enterRelease();
  Serial.println(">> Released. Motor output OFF.");
}

void cmdHold() {
  enterHoldHere();
  Serial.print(">> Holding here. zeroOffset=");
  Serial.println(zeroOffsetCounts);
}

void cmdDiag() {
  if (!zeroSet) {
    Serial.println(">> ERROR: hold first (run `hold`) so we have a reference angle.");
    return;
  }
  Serial.println(">> Starting diagnostic two-direction sweep (no homing).");
  diagApplyHoming = false;
  startDiag();
}

void cmdHome() {
  Serial.println(">> Home: latching current encoder, running diag, applying offset.");
  startHomeRitual();
}

void cmdSetcal(float deg) {
  g_massOffsetDeg = deg;
  Serial.print(">> mass_offset_deg set to ");
  Serial.print(g_massOffsetDeg, 3);
  Serial.println(" (RAM only; run `savecal` to persist)");
}

void cmdSavecal() {
  saveMassOffset();
  Serial.print(">> mass_offset_deg saved to NVS: ");
  Serial.print(g_massOffsetDeg, 3);
  Serial.println(" deg");
}

void cmdLoadcal() {
  loadMassOffset();
  Serial.print(">> mass_offset_deg loaded from NVS: ");
  Serial.print(g_massOffsetDeg, 3);
  Serial.println(" deg");
}

void cmdCal() {
  Serial.print(">> mass_offset_deg current value: ");
  Serial.print(g_massOffsetDeg, 3);
  Serial.println(" deg");
}

// (Calibration workflow uses the existing primitives:
//   1. Position pointer precisely at visual up by hand.
//   2. `hold`     - latch encoder as user 0.
//   3. `diag`     - run sweep; firmware prints "gravity-up at user-frame X.XX".
//   4. `setcal X` - set mass_offset_deg in RAM to that value.
//   5. `savecal`  - persist to NVS.)

void cmdMove(float toDeg) {
  if (!zeroSet) {
    Serial.println(">> ERROR: hold first (run `hold`) so the zero is defined.");
    return;
  }
  // Only swap PIDs if we're not already on production. The swap cycles
  // the output, which is unnecessary and visible to the user otherwise.
  if (usingDiagPid) {
    safeSwitchPid(false);
  }
  float distance = fabsf(toDeg - currentTargetDeg);
  uint32_t mt = (uint32_t)(distance * REC_MT_PER_DEG_MS);
  if (mt < 200) mt = 200;
  Serial.print(">> move to "); Serial.print(toDeg, 2);
  Serial.print(" deg, mt="); Serial.print(mt); Serial.println(" ms");
  startMoveTo(toDeg, mt);
}

void cmdReport() {
  printReport();
}

// =============================================================================
// MOTION ENGINE
// =============================================================================

void motorBootInit() {
  writeReg8(REG_OUTPUT, 0);
  delay(20);
  writeReg8(REG_STALL_PROT, 0);
  delay(20);
  writePosMaxCurrent(REC_MC_MA);
  delay(20);
  state = ST_RELEASED;
  zeroSet = false;
}

void enterRelease() {
  writeReg8(REG_OUTPUT, 0);
  state = ST_RELEASED;
  zeroSet = false;
  currentTargetDeg = 0.0f;
}

void enterHoldHere() {
  writeReg8(REG_OUTPUT, 0);
  delay(20);
  writeReg8(REG_MODE, MODE_POSITION);
  delay(20);
  writePosMaxCurrent(REC_MC_MA);
  delay(20);

  zeroOffsetCounts = readReg32(REG_POS_READ);
  writeReg32(REG_POS, zeroOffsetCounts);
  delay(20);

  writeReg8(REG_OUTPUT, 1);

  zeroSet = true;
  currentTargetDeg = 0.0f;
  state = ST_HOLDING;
}

void startHomeRitual() {
  // Latch current encoder as user zero (the algorithm finds gravity-up
  // relative to it, then re-anchors zero to land at visual-up).
  enterHoldHere();
  delay(500);
  diagApplyHoming = true;
  Serial.println(">> Home ritual: latch -> 2-rev diag -> move to visual up.");
  startDiag();
}

// =============================================================================
// NVS persistence for mass_offset
// =============================================================================

void loadMassOffset() {
  g_prefs.begin("calib", true);   // read-only
  g_massOffsetDeg = g_prefs.getFloat("mass_offset", MASS_OFFSET_DEG_DEFAULT);
  g_prefs.end();
  Serial.print("Mass offset (NVS): ");
  Serial.print(g_massOffsetDeg, 3);
  Serial.println(" deg");
}

void saveMassOffset() {
  g_prefs.begin("calib", false);  // read-write
  g_prefs.putFloat("mass_offset", g_massOffsetDeg);
  g_prefs.end();
}

void startMoveTo(float toDeg, uint32_t mt) {
  moveStartDeg = currentTargetDeg;
  moveEndDeg = toDeg;
  moveStartMS = millis();
  moveDurMS = mt;
  state = ST_MOVING;
}

void startDiag() {
  // Switch to looser diagnostic PID without jolting (output cycle resets
  // any integrator wind-up from the production hold).
  safeSwitchPid(true);

  // Reset the on-board sinusoid-fit accumulators
  fitReset();

  // Latch the angle we are at right now as the starting point
  diagStartDeg = currentTargetDeg;

  // CW = sweep DIAG_REVS_PER_DIRECTION revolutions forward
  float sweepDeg = 360.0f * DIAG_REVS_PER_DIRECTION;
  moveStartDeg = diagStartDeg;
  moveEndDeg = diagStartDeg + sweepDeg;
  moveStartMS = millis();
  moveDurMS = (uint32_t)(sweepDeg * DIAG_MT_PER_DEG_MS);
  state = ST_DIAG_CW;
}

void updateMotion() {
  if (state == ST_RELEASED) {
    return;
  }
  if (state == ST_HOLDING) {
    writeTargetDeg(currentTargetDeg);
    return;
  }
  if (state == ST_DIAG_PAUSE) {
    // Hold at the end of CW for pauseDurMS, then start CCW
    writeTargetDeg(currentTargetDeg);
    if (millis() - pauseStartMS >= pauseDurMS) {
      // Begin CCW: sweep back to diagStartDeg through the same number of revs
      float sweepDeg = 360.0f * DIAG_REVS_PER_DIRECTION;
      moveStartDeg = currentTargetDeg;
      moveEndDeg = diagStartDeg;
      moveStartMS = millis();
      moveDurMS = (uint32_t)(sweepDeg * DIAG_MT_PER_DEG_MS);
      state = ST_DIAG_CCW;
    }
    return;
  }

  // ST_MOVING / ST_DIAG_CW / ST_DIAG_CCW : interpolate trajectory
  uint32_t elapsed = millis() - moveStartMS;
  if (elapsed >= moveDurMS) {
    // Move complete. Latch end target.
    currentTargetDeg = moveEndDeg;
    writeTargetDeg(currentTargetDeg);

    // State transitions at end of move
    if (state == ST_DIAG_CW) {
      pauseStartMS = millis();
      pauseDurMS = DIAG_PAUSE_MS;
      state = ST_DIAG_PAUSE;
    } else if (state == ST_DIAG_CCW) {
      // Diagnostic complete. Solve sinusoid fit, optionally apply offset
      // and move to visual-up. Always restore production PID safely.
      float gravUserDeg = fitGravityUpDeg();   // gravity-up in current user frame, 0..360

      if (isnan(gravUserDeg)) {
        Serial.println(">> Sinusoid fit FAILED (insufficient data).");
        safeSwitchPid(false);
        state = ST_HOLDING;
        return;
      }

      // Map to signed angle in [-180, 180] for legibility
      float gravSigned = gravUserDeg;
      if (gravSigned > 180.0f) gravSigned -= 360.0f;

      Serial.print(">> fit: gravity-up at user-frame ");
      Serial.print(gravSigned, 2);
      Serial.print(" deg ; mass_offset_deg ");
      Serial.print(g_massOffsetDeg, 2);
      Serial.print(" ; fit_N ");
      Serial.println(fit_N);

      if (diagApplyHoming) {
        // Visual-up (in current user frame) is gravity-up minus the stored
        // mass offset. Re-anchor zeroOffsetCounts so user-frame 0 is
        // visual-up. Then move to user 0 with the production recipe.
        float visualUpDegOldFrame = gravSigned - g_massOffsetDeg;
        Serial.print(">> visual-up at old user-frame ");
        Serial.print(visualUpDegOldFrame, 2);
        Serial.println(" deg ; re-anchoring zero and moving there.");

        // New zero in encoder counts:
        zeroOffsetCounts += (int32_t)(visualUpDegOldFrame * (float)COUNTS_PER_DEG);

        // Where we are now in the NEW user frame:
        float pointerDegNewFrame = readEncoderDeg();
        currentTargetDeg = pointerDegNewFrame;

        // Restore production PID (without jolt — also re-pins target)
        safeSwitchPid(false);
        diagApplyHoming = false;

        // Plan the move to user 0 (visual up). Distance is whatever short
        // path remains.
        float distance = fabsf(currentTargetDeg);
        if (distance > 180.0f) distance = 360.0f - distance;
        uint32_t mt = (uint32_t)(distance * REC_MT_PER_DEG_MS);
        if (mt < 200) mt = 200;
        Serial.print(">> moving to visual up (");
        Serial.print(currentTargetDeg, 2);
        Serial.print(" -> 0 deg, mt=");
        Serial.print(mt);
        Serial.println(" ms)");
        startMoveTo(0.0f, mt);
      } else {
        // Diagnostic only — just hold where we ended up.
        safeSwitchPid(false);
        state = ST_HOLDING;
        Serial.println(">> Diagnostic complete. Motor holding (no homing applied).");
      }
    } else {
      // Production move complete
      state = ST_HOLDING;
    }
  } else {
    float progress = (float)elapsed / (float)moveDurMS;
    float eased = easeInOut(progress);
    float sp = moveStartDeg + (moveEndDeg - moveStartDeg) * eased;
    writeTargetDeg(sp);
  }
}

// =============================================================================
// COMPRESSED DELTA LOG
// =============================================================================

struct LogState {
  bool initialized = false;
  char ph;
  float Tg;
  float sp;
  float p;
  float er;
  float a;
  float cur;
  int   tmp;
  float v;
};
static LogState g_log;

static inline bool fchg(float a, float b, float eps) { return fabsf(a - b) >= eps; }

void emitLogDelta() {
  // Snapshot
  char ph = phaseCode(state);
  float Tg = currentTargetDeg;
  float sp = currentTargetDeg;
  if (state == ST_MOVING || state == ST_DIAG_CW || state == ST_DIAG_CCW) {
    uint32_t elapsed = millis() - moveStartMS;
    float prog = (moveDurMS > 0) ? ((float)elapsed / (float)moveDurMS) : 1.0f;
    if (prog < 0) prog = 0;
    if (prog > 1) prog = 1;
    sp = moveStartDeg + (moveEndDeg - moveStartDeg) * easeInOut(prog);
  }
  float p = readEncoderDeg();
  float er = sp - p;
  float a = motorGetSpeedRPM();
  float cur = motorGetCurrentMA();
  int   tmp = (int)motorGetTempC();
  float v = motorGetVinV();

  // While spinning during the diagnostic, accumulate (encoder, current)
  // samples for the sinusoid fit. We use `cur` because it correlates most
  // strongly with gravity (Phase 0: r=0.87 for cur vs 0.85 for er).
  // Skip samples in the first DIAG_FIT_SKIP_FRACTION of each sweep — the
  // motor is breaking static friction and accelerating into steady-state
  // rotation; samples from that window have a position-locked bias that
  // contaminates the sinusoid fit.
  if (state == ST_DIAG_CW || state == ST_DIAG_CCW) {
    uint32_t elapsed = millis() - moveStartMS;
    float progress = (moveDurMS > 0) ? ((float)elapsed / (float)moveDurMS) : 1.0f;
    if (progress >= DIAG_FIT_SKIP_FRACTION) {
      fitAccumulate(p, cur);
    }
  }

  String d;
  d.reserve(140);
  bool first = !g_log.initialized;
  bool outputOn = (state != ST_RELEASED);

  if (first || g_log.ph != ph) {
    d += ",ph="; d += ph; g_log.ph = ph;
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
  if (outputOn && (first || fchg(g_log.cur, cur, 5.0f))) {
    // 5 mA epsilon; current readback is noisy
    d += ",cur="; d += String(cur, 0); g_log.cur = cur;
  }
  if (outputOn && (first || g_log.tmp != tmp)) {
    d += ",tmp="; d += String(tmp); g_log.tmp = tmp;
  }
  if (first || fchg(g_log.v, v, 0.05f)) {
    d += ",v="; d += String(v, 2); g_log.v = v;
  }

  g_log.initialized = true;
  if (d.length() > 0) {
    Serial.print("t="); Serial.print(millis());
    Serial.println(d);
  }
}

// =============================================================================
// REPORT (`report` command)
// =============================================================================

void printReport() {
  const char* stateName =
      (state == ST_RELEASED)   ? "RELEASED" :
      (state == ST_HOLDING)    ? "HOLDING"  :
      (state == ST_MOVING)     ? "MOVING"   :
      (state == ST_DIAG_CW)    ? "DIAG_CW"  :
      (state == ST_DIAG_PAUSE) ? "DIAG_PAUSE":
      (state == ST_DIAG_CCW)   ? "DIAG_CCW" : "?";
  Serial.println();
  Serial.println("=== Report ===");
  Serial.print("State:           "); Serial.print(stateName);
  Serial.print(" (zeroSet="); Serial.print(zeroSet ? "yes" : "no");
  Serial.println(")");
  Serial.print("PID profile:     ");
  Serial.println(usingDiagPid ? "diagnostic" : "production");
  Serial.print("Logical target:  "); Serial.print(currentTargetDeg, 2); Serial.println(" deg");
  Serial.print("Encoder raw:     "); Serial.print(readReg32(REG_POS_READ));
  Serial.print(" (zero offset "); Serial.print(zeroOffsetCounts); Serial.println(")");
  Serial.print("Encoder user:    "); Serial.print(readEncoderDeg(), 2); Serial.println(" deg");
  Serial.print("Vin:             "); Serial.print(motorGetVinV(), 2); Serial.println(" V");
  Serial.print("Motor temp:      "); Serial.print(motorGetTempC()); Serial.println(" C");
  Serial.print("Motor current:   "); Serial.print(motorGetCurrentMA(), 0); Serial.println(" mA");
  Serial.print("Actual speed:    "); Serial.print(motorGetSpeedRPM(), 1); Serial.println(" RPM");
  Serial.println("==============");
}

// =============================================================================
// SERIAL PARSING
// =============================================================================

void processSerialCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;
  String low = cmd; low.toLowerCase();

  if (low == "release") { cmdRelease(); return; }
  if (low == "hold")    { cmdHold();    return; }
  if (low == "diag")    { cmdDiag();    return; }
  if (low == "home")    { cmdHome();    return; }
  if (low == "report")  { cmdReport();  return; }
  if (low == "cal")     { cmdCal();     return; }
  if (low == "savecal") { cmdSavecal(); return; }
  if (low == "loadcal") { cmdLoadcal(); return; }
  if (low.startsWith("setcal")) {
    float v = cmd.substring(6).toFloat();
    cmdSetcal(v);
    return;
  }
  if (low.startsWith("move")) {
    float v = cmd.substring(4).toFloat();
    cmdMove(v);
    return;
  }
  Serial.print(">> Unknown command: ");
  Serial.println(cmd);
}

// =============================================================================
// MOTOR I/O HELPERS
// =============================================================================

void writeTargetDeg(float deg) {
  int32_t enc = zeroOffsetCounts + (int32_t)(deg * (float)COUNTS_PER_DEG);
  writeReg32(REG_POS, enc);
}

float readEncoderDeg() {
  int32_t raw = readReg32(REG_POS_READ);
  return (float)(raw - zeroOffsetCounts) / (float)COUNTS_PER_DEG;
}

float motorGetVinV()      { return readReg32(REG_VIN) / 100.0f; }
int32_t motorGetTempC()   { return readReg32(REG_TEMP); }
float motorGetSpeedRPM()  { return readReg32(REG_SPEED_READ) / 100.0f; }
float motorGetCurrentMA() { return readReg32(REG_CURRENT_READ) / 100.0f; }

void writePosMaxCurrent(int32_t mA) {
  writeReg32(REG_POS_MAXCUR, mA * 100);
}

void writePosPID(int32_t p, int32_t i, int32_t d) {
  writeReg96(REG_POS_PID, p, i, d);
}

void applyProductionPid() {
  // Used at boot only (output already off).
  writePosPID(REC_KP, REC_KI, REC_KD);
  writePosMaxCurrent(REC_MC_MA);
  usingDiagPid = false;
  delay(20);
}

void applyDiagnosticPid() {
  // Used at boot only (output already off).
  writePosPID(DIAG_KP, DIAG_KI, DIAG_KD);
  writePosMaxCurrent(DIAG_MC_MA);
  usingDiagPid = true;
  delay(20);
}

// Safely switch PID gains while the motor is energized. Without this, the
// motor's internal integral term carries over from the previous gain set
// and gets multiplied by the new (often much larger) gains, producing a
// massive instantaneous output, oscillation, and supply droop. Sequence:
//   1. Disable output (this resets the motor's internal integrator).
//   2. Pin the position target to the current encoder reading, so when we
//      re-enable, the motor sees zero error.
//   3. Write new PID + current cap.
//   4. Re-enable.
//   5. Update our software's currentTargetDeg to match where the pointer
//      actually is (otherwise updateMotion() would still try to drive to
//      the old target and re-introduce a jolt on the next tick).
void safeSwitchPid(bool toDiag) {
  writeReg8(REG_OUTPUT, 0);
  delay(20);
  int32_t cur_enc = readReg32(REG_POS_READ);
  writeReg32(REG_POS, cur_enc);
  delay(10);
  if (toDiag) {
    writePosPID(DIAG_KP, DIAG_KI, DIAG_KD);
    writePosMaxCurrent(DIAG_MC_MA);
    usingDiagPid = true;
  } else {
    writePosPID(REC_KP, REC_KI, REC_KD);
    writePosMaxCurrent(REC_MC_MA);
    usingDiagPid = false;
  }
  delay(20);
  writeReg8(REG_OUTPUT, 1);
  // Update software state so updateMotion() holds where we actually are.
  currentTargetDeg = (float)(cur_enc - zeroOffsetCounts) / (float)COUNTS_PER_DEG;
}

// =============================================================================
// I2C PRIMITIVES (verbatim)
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
