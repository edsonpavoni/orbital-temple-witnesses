// pointer_v2 — First Witness Series.
//
// Working baseline as of 2026-04-27 ~02:00 NYT. Validated on the bench motor
// and on the actual sculpture (off-balance pointer). Three sequence runs all
// landed within 1.5 deg of target. See LEARNINGS-2026-04-27.md for the full
// session writeup. Treat this file as a frozen reference — further polish
// happens in pointer_v3.
//
// The two-mode hybrid:
//   Phase A  position mode  — ramped slew to angle X (Apr 15 PID tuning)
//   Phase B  speed mode     — encoder-driven sine 360 with velocity floor
//   Phase B  brake          — position mode pinned at encTarget (no speed-mode brake)
//   Phase C  position mode  — ramped slew to X (inherits pos-mode from brake)
//
// Why two modes: position mode handles the off-balance pointer trivially
// because it uses encoder feedback directly. Speed mode handles smooth
// uniform rotation (position mode has a "pendulum" gravity artifact).
// Each mode does what it does best; the transitions are the work.
//
// Coordinate frame:
//   The pointer is intentionally off-balance. Powered down it falls to its
//   gravity rest position. We define that rest position as user-angle +50 deg.
//   So user-0 deg = (rest position) - 50 deg.
//
// Boot flow:
//   1. Power up. Motor output OFF. Wait 5 s for the pointer to settle at rest.
//   2. Read encoder. Subtract 50 deg to set zero_offset_counts.
//   3. Apply Apr 15 position-mode PID gains (P=15M, D=40M).
//   4. Print prompt. Wait for serial commands.
//
// Commands:
//   g            run sequence with X = 0 deg
//   g <deg>      run sequence with custom X
//   s            print current angle
//   power        print vin / current / power / temp / output
//   rezero       re-acquire the gravity rest reference (motor goes limp 5 s)
//   stop         disable output
//
// Hardware: Xiao ESP32-S3 + M5 Unit-Roller485 Lite over I2C (Grove), 12 V PD.

#include <Arduino.h>
#include <Wire.h>
#include "unit_rolleri2c.hpp"

// ---- pins / address ----
#define I2C_SDA      8
#define I2C_SCL      9
#define ROLLER_ADDR  0x64

// ---- coordinate frame ----
static const float   GRAVITY_REST_DEG = 50.0f;   // user angle of the natural rest
static const int32_t COUNTS_PER_DEG   = 100;     // motor: 36000 counts / 360 deg
static const int32_t COUNTS_PER_REV   = 36000;
static const uint32_t SETTLE_MS       = 5000;    // pointer fall + settle window

// ---- Phase A / C: position mode + ramped setpoint ----
// We don't slam setPos to the final target — that produces overshoot/oscillation
// (±2–3 deg unloaded). Instead we ramp the setpoint at SLEW_DEG_PER_S so the
// position controller is always tracking a slowly-moving target. This matches
// pointer_v1's proven approach.
//
// 600 mA current cap. 400 mA tracks the moving setpoint during the slew but
// can't *hold* the off-balance pointer against gravity 50 deg from rest — at
// small position errors the position PID outputs proportionally small current,
// gravity wins, motor drifts to force-equilibrium ~6 deg from target. 600 mA
// gives the position PID enough authority to hold near zero error.
// Motor spec: 0.5 A continuous, 1 A short-term — 600 mA is above continuous,
// fine for the seconds-long sequence; do not park here for hours.
static const int32_t  POS_MAX_CURRENT_x100 = 60000;   // 600 mA = 60000 (0.01 mA/count)
static const float    SLEW_DEG_PER_S       = 30.0f;   // setpoint slew rate
static const uint32_t SLEW_STEP_MS         = 10;      // setpoint update cadence
static const uint32_t SLEW_SETTLE_MS       = 800;     // hold after reaching final tgt
static const uint32_t SLEW_TIMEOUT_MS      = 15000;   // safety

// ---- Phase B: speed mode + encoder-driven sine velocity ----
// The velocity envelope is parameterized by encoder *progress* through the
// revolution, not by elapsed time. The loop exits when the encoder has
// advanced by exactly COUNTS_PER_REV. This guarantees a full 360 regardless
// of speed-tracking lag.
//
// Floor + peak: a pure sine 0 → peak → 0 stalls in the back tail because the
// commanded velocity falls into the BLDC cogging deadband (~5–11 RPM). The
// floor keeps every point of the envelope above that deadband, so the motor
// never stops mid-rev. Floor 15 RPM = comfortable margin. Peak 30 RPM = smooth
// dynamic. Stall protection must be off (firmware's auto-shutdown is itself
// the source of the "starts, stops, lurches" pattern at low RPM).
static const float    V_FLOOR_MU           = 1500.0f;  // 15 RPM (mu = RPM*100)
static const float    V_PEAK_MU            = 3000.0f;  // 30 RPM
static const uint32_t B_SAFETY_TIMEOUT_MS  = 8000;     // bail if motor never reaches target
static const uint32_t B_BRAKE_HOLD_MS      = 800;      // dwell after switching to
                                                       // position mode at encTarget,
                                                       // letting the position PID
                                                       // pull the rotor to rest
                                                       // before Phase C takes over
static const int32_t  SPEED_MAX_CURRENT_x100 = 100000; // 1 A = 100000 (0.01 mA/count)
static const uint32_t SPEED_UPDATE_MS      = 10;       // 100 Hz, matches Jan 24

// ---- telemetry ----
// Lightweight position log. Prints one CSV-ish row every TELEMETRY_MS during
// each phase. Format:
//   [<phase>] t=<ms> enc=<raw_count> deg=<user_deg> mode=<n> extra=<value>
// where mode is the motor's reported mode (1=speed, 2=pos) and `extra` is
// target encoder for A/C, commanded speed (motor units) for B.
static const uint32_t TELEMETRY_MS = 100;

// ---- state ----
UnitRollerI2C roller;
int32_t zeroOffsetCounts = 0;          // encoder count that = user 0 deg

// ---- helpers ----

// Find the encoder count nearest to `referenceEncoder` that corresponds to
// userDeg. Picks the short way around. Required because the encoder is
// cumulative — after one full rev, its raw value has advanced by 36000 counts
// even though the pointer is back where it started.
int32_t userDegToTargetEncoder(float userDeg, int32_t referenceEncoder) {
    int32_t naive = zeroOffsetCounts + (int32_t)lroundf(userDeg * COUNTS_PER_DEG);
    int32_t delta = naive - referenceEncoder;
    int32_t mod = ((delta % COUNTS_PER_REV) + COUNTS_PER_REV) % COUNTS_PER_REV;
    if (mod > COUNTS_PER_REV / 2) mod -= COUNTS_PER_REV;
    return referenceEncoder + mod;
}

float encoderToUserDeg(int32_t encoder) {
    return (encoder - zeroOffsetCounts) / (float)COUNTS_PER_DEG;
}

// Single telemetry row.
void logRow(const char* phase, uint32_t t_phase_ms, int32_t extra) {
    int32_t enc  = roller.getPosReadback();
    uint8_t mode = roller.getMotorMode();
    Serial.printf("[%s] t=%lu enc=%ld deg=%.2f mode=%u extra=%ld\n",
        phase, (unsigned long)t_phase_ms, (long)enc,
        encoderToUserDeg(enc), (unsigned)mode, (long)extra);
}

// ---- boot calibration ----

void captureGravityRest() {
    Serial.print("Waiting ");
    Serial.print(SETTLE_MS / 1000);
    Serial.print(" s for pointer to settle at gravity rest");
    roller.setOutput(0);
    delay(50);
    for (uint32_t t = 0; t < SETTLE_MS; t += 500) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    int32_t restEncoder = roller.getPosReadback();
    zeroOffsetCounts = restEncoder - (int32_t)(GRAVITY_REST_DEG * COUNTS_PER_DEG);
    Serial.printf("[boot] rest_enc=%ld  zero_off=%ld  (rest = user +%.1f deg)  mode=%u\n",
        (long)restEncoder, (long)zeroOffsetCounts, GRAVITY_REST_DEG,
        (unsigned)roller.getMotorMode());
}

// ---- mode entry helpers ----
// Both follow the M5 manufacturer pattern: output off, mode, params, output on.

// Mode-switch dead-time is dangerous: setOutput(0) freewheels the motor while
// the new mode/params are written. Gravity feeds momentum into the rotor
// during this window. Shrunk delays from 50 ms to 10 ms — at 30 RPM that's
// ~9 deg of coast instead of ~45 deg, which dramatically reduces the brake
// overshoot on the speed→position transition. 10 ms is well above the I2C
// transaction time (~1 ms at 400 kHz, 4-byte payload) so registers still
// commit cleanly.
void enterPositionMode(int32_t targetEncoder) {
    roller.setOutput(0);
    delay(10);
    roller.setMode(ROLLER_MODE_POSITION);
    delay(10);
    roller.setPosMaxCurrent(POS_MAX_CURRENT_x100);
    delay(10);
    roller.setPos(targetEncoder);
    delay(10);
    roller.setOutput(1);
}

void enterSpeedMode() {
    roller.setOutput(0);
    delay(10);
    roller.setStallProtection(0);   // critical — see RESEARCH-slow-smooth-motion.md
    delay(10);
    roller.setMode(ROLLER_MODE_SPEED);
    delay(10);
    roller.setSpeedMaxCurrent(SPEED_MAX_CURRENT_x100);
    delay(10);
    roller.setSpeed(0);
    delay(10);
    roller.setOutput(1);
}

// ---- the three phases ----

// Ramped slew to user angle xDeg in position mode. Used for both Phase A
// (homing from rest) and Phase C (locking after the smooth 360). The setpoint
// advances by SLEW_DEG_PER_S; the motor tracks it. No slam, no overshoot.
//
// `enterMode`: when true, calls enterPositionMode(currentEncoder) first
// (use for Phase A — coming from boot/idle). When false, assumes the motor
// is already in position mode (use for Phase C — comes from Phase B's brake,
// which already entered position mode). Skipping the mode-entry avoids a
// 50 ms freewheel that lets gravity feed fresh momentum into the rotor.
void slewToX(const char* phase, float xDeg, bool enterMode) {
    int32_t startEnc = roller.getPosReadback();
    int32_t finalTgt = userDegToTargetEncoder(xDeg, startEnc);
    int32_t move     = finalTgt - startEnc;
    Serial.printf("[%s start] from %.2f deg (enc=%ld) -> %.2f deg (target_enc=%ld)  move=%.2f deg\n",
        phase, encoderToUserDeg(startEnc), (long)startEnc,
        xDeg, (long)finalTgt, move / (float)COUNTS_PER_DEG);

    if (enterMode) {
        enterPositionMode(startEnc);
    } else {
        // Already in position mode from Phase B brake. Just update the setpoint
        // to the current encoder so the controller has a fresh, stable anchor
        // before we start ramping.
        roller.setPos(startEnc);
    }

    int32_t step = (int32_t)((SLEW_DEG_PER_S * COUNTS_PER_DEG * SLEW_STEP_MS) / 1000.0f);
    if (step < 1) step = 1;

    int32_t curTgt = startEnc;
    uint32_t start    = millis();
    uint32_t lastStep = 0;
    uint32_t lastLog  = 0;
    uint32_t reachedAt = 0;
    while (true) {
        uint32_t elapsed = millis() - start;
        if (elapsed - lastStep >= SLEW_STEP_MS) {
            lastStep = elapsed;
            int32_t diff = finalTgt - curTgt;
            if (labs(diff) <= step) curTgt = finalTgt;
            else                    curTgt += (diff > 0 ? step : -step);
            roller.setPos(curTgt);
        }
        if (elapsed - lastLog >= TELEMETRY_MS) {
            lastLog = elapsed;
            logRow(phase, elapsed, curTgt);
        }
        if (curTgt == finalTgt) {
            if (reachedAt == 0) reachedAt = elapsed;
            if (elapsed - reachedAt >= SLEW_SETTLE_MS) break;
        }
        if (elapsed > SLEW_TIMEOUT_MS) {
            Serial.printf("[%s SAFETY TIMEOUT]\n", phase);
            break;
        }
        delay(2);
    }

    int32_t endEnc = roller.getPosReadback();
    Serial.printf("[%s end] enc=%ld deg=%.2f err=%.2f\n",
        phase, (long)endEnc, encoderToUserDeg(endEnc),
        (endEnc - finalTgt) / (float)COUNTS_PER_DEG);
}

void smoothRevolution() {
    int32_t encStart  = roller.getPosReadback();
    int32_t encTarget = encStart + COUNTS_PER_REV;
    Serial.printf("[B start] enc=%ld (%.2f deg)  target=+360 deg  v_floor=%.0f mu  v_peak=%.0f mu\n",
        (long)encStart, encoderToUserDeg(encStart), V_FLOOR_MU, V_PEAK_MU);

    enterSpeedMode();

    int32_t  lastCmd    = 0;
    uint32_t start      = millis();
    uint32_t lastUpdate = 0;
    uint32_t lastLog    = 0;
    bool     timedOut   = false;
    while (true) {
        uint32_t elapsed = millis() - start;
        int32_t  enc     = roller.getPosReadback();

        if (enc >= encTarget) {
            roller.setSpeed(0);
            lastCmd = 0;
            // Brake by switching to position mode pinned at encTarget. The
            // speed-mode brake (setSpeed=0) is unreliable here — at low rotor
            // speed the speed estimator quantizes and the PID can't measure
            // residual motion, so motor coasts ~40 deg under gravity. Position
            // mode uses encoder feedback directly, so it pulls the rotor back
            // to encTarget regardless of momentum.
            enterPositionMode(encTarget);
            uint32_t brakeStart = millis();
            uint32_t brakeLastLog = 0;
            while (millis() - brakeStart < B_BRAKE_HOLD_MS) {
                uint32_t bel = millis() - brakeStart;
                if (bel - brakeLastLog >= TELEMETRY_MS) {
                    brakeLastLog = bel;
                    logRow("Bk", bel, encTarget);
                }
                delay(2);
            }
            break;
        }
        if (elapsed > B_SAFETY_TIMEOUT_MS) {
            roller.setSpeed(0);
            lastCmd  = 0;
            timedOut = true;
            break;
        }

        if (elapsed - lastUpdate >= SPEED_UPDATE_MS) {
            lastUpdate = elapsed;
            float progress = (float)(enc - encStart) / (float)COUNTS_PER_REV;
            if (progress < 0.0f) progress = 0.0f;
            if (progress > 1.0f) progress = 1.0f;
            // floor + sine envelope: v(p) = floor + (peak-floor) * (1-cos(2π·p))/2
            // at p=0: v=floor.  at p=0.5: v=peak.  at p=1: v=floor.
            float v = V_FLOOR_MU
                    + (V_PEAK_MU - V_FLOOR_MU) * (1.0f - cosf(2.0f * (float)PI * progress)) / 2.0f;
            lastCmd = (int32_t)v;
            roller.setSpeed(lastCmd);
        }
        if (elapsed - lastLog >= TELEMETRY_MS) {
            lastLog = elapsed;
            logRow("B", elapsed, lastCmd);
        }
    }

    uint32_t total_ms = millis() - start;
    int32_t  encEnd   = roller.getPosReadback();
    if (timedOut) Serial.println("[B SAFETY TIMEOUT — motor never reached target]");
    Serial.printf("[B end] enc=%ld (%.2f deg)  travelled=%ld counts (%.2f deg)  elapsed=%lu ms\n",
        (long)encEnd, encoderToUserDeg(encEnd),
        (long)(encEnd - encStart),
        (encEnd - encStart) / (float)COUNTS_PER_DEG,
        (unsigned long)total_ms);
}

void runSequence(float xDeg) {
    Serial.print("\n=== Sequence  X=");
    Serial.print(xDeg, 1);
    Serial.println(" deg ===");
    slewToX("A", xDeg, /*enterMode=*/true);
    delay(200);
    smoothRevolution();
    // Phase B's brake left us in position mode pinned at encTarget.
    // Phase C inherits that — no fresh mode entry, no freewheel.
    slewToX("C", xDeg, /*enterMode=*/false);
    Serial.println("=== done ===");
}

// ---- serial CLI ----

static char   line[64];
static size_t lineLen = 0;

void handleLine(const char* s) {
    if (!*s) { Serial.print("> "); return; }

    if (s[0] == 'g' && (s[1] == 0 || s[1] == ' ')) {
        float x = (s[1] == ' ') ? atof(s + 2) : 0.0f;
        runSequence(x);
    } else if (!strcmp(s, "s")) {
        int32_t enc = roller.getPosReadback();
        Serial.print("angle=");
        Serial.print(encoderToUserDeg(enc), 2);
        Serial.print(" deg  encoder=");
        Serial.println(enc);
    } else if (!strcmp(s, "power")) {
        float vin_V    = roller.getVin() / 100.0f;
        float curr_mA  = roller.getCurrentReadback() / 100.0f;
        float power_mW = vin_V * curr_mA;
        Serial.printf("vin=%.2f V  current=%.1f mA  power=%.0f mW  temp=%d C  output=%d\n",
            vin_V, curr_mA, power_mW,
            (int)roller.getTemp(), (int)roller.getOutputStatus());
    } else if (!strcmp(s, "rezero")) {
        captureGravityRest();
    } else if (!strcmp(s, "stop")) {
        roller.setOutput(0);
        Serial.println("output OFF");
    } else {
        Serial.print("unknown: '");
        Serial.print(s);
        Serial.println("'");
    }
    Serial.print("> ");
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println();
    Serial.println("pointer_v2 booting...");

    Wire.begin(I2C_SDA, I2C_SCL, 400000);

    bool ok = false;
    for (int i = 0; i < 5; i++) {
        if (roller.begin(&Wire, ROLLER_ADDR, I2C_SDA, I2C_SCL, 400000)) { ok = true; break; }
        Serial.print("handshake retry ");
        Serial.print(i + 1);
        Serial.println("/5");
        delay(200);
    }
    if (!ok) {
        Serial.println("ERROR: roller not found. Halting.");
        while (1) delay(1000);
    }

    roller.setOutput(0);
    roller.setStallProtection(0);   // safe default for both modes
    delay(50);

    // Stronger position-mode PID gains for hold against gravity. The firmware
    // defaults are too soft: at small position errors they output too little
    // current to overcome the off-balance pointer's gravity torque, so the
    // motor settles 6-7 deg from target. Higher P + much higher D gives both
    // tighter steady-state and the damping needed to avoid the brake-overshoot
    // ringing we saw with default gains. Values come from the Apr 15 bench
    // tuning ("P=15M, D=40M for stronger hold under gravity"); I left at
    // default (low — we don't want long-term wind-up).
    roller.setPosPID(15000000, 1000, 40000000);
    delay(50);

    captureGravityRest();

    Serial.println();
    Serial.println("Commands:");
    Serial.println("  g            run sequence with X = 0 deg");
    Serial.println("  g <deg>      run sequence with custom X");
    Serial.println("  s            print current angle");
    Serial.println("  power        print vin / current / power / temp / output");
    Serial.println("  rezero       re-acquire gravity rest");
    Serial.println("  stop         disable output");
    Serial.print("> ");
}

void loop() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            line[lineLen] = 0;
            handleLine(line);
            lineLen = 0;
        } else if (lineLen < sizeof(line) - 1) {
            line[lineLen++] = c;
        }
    }
}
