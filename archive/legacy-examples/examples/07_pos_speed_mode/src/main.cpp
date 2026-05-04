// ============================================================
//  Witness · 07_pos_speed_mode
//
//  Tests the HIDDEN MODE_POS_SPEED (value 5) on the M5 Roller485-Lite.
//  This mode is not exposed by the library's `roller_mode_t` enum (1-4)
//  but IS implemented in the motor's internal firmware and reachable by
//  writing the raw value 5 to the mode register (0x01).
//
//  Why this matters for slow smooth rotation:
//   - MODE_POS_SPEED is a CASCADED controller: position PID generates a
//     velocity setpoint, low-pass filtered at ~2 Hz (80 ms time constant),
//     fed into the speed PID, which commands current to the FOC loop.
//   - Unlike MODE_POS, it does NOT trigger an internal move-to-target
//     with its own accel/decel plan — so no "step-stutter" when we
//     stream position targets.
//   - Unlike MODE_SPEED, it uses position feedback directly — not the
//     quantized speed estimate that fails at slow RPM (known firmware
//     limitation: at 60 s/rev, speed estimate has ~0.2 counts/sample,
//     binary noise dominates).
//   - No stall-protection check in this mode's case handler in the motor
//     firmware — so no "stops-then-moves-quick" cycles.
//
//  Internal default PIDs for MODE_POS_SPEED (from firmware mysys.c):
//     pos_pid_plus:   P=12.0, I=0.01,   D=2000.0 → {1200000, 100000, 200000000}
//     speed_pid_plus: P=15.0, I=0.0001, D=400.0  → {1500000, 1000,   40000000}
//  These are automatically loaded when entering mode 5.
//
//  Commands (line + Enter):
//   cal              run encoder calibration (safe with pointer — no 2400 RPM)
//   enter            enter MODE_POS_SPEED, set output on, target = current pos
//   leave            set output off
//   stall0           setStallProtection(0) — disable stall detection
//   stall1           setStallProtection(1) — re-enable
//   maxc <mA>        set POS and SPEED max current (mA, e.g. 500 = 0.5 A)
//   pospid <p> <i> <d>   set position PID (integer units)
//   spdpid <p> <i> <d>   set speed PID (integer units)
//   pid?             print both PIDs
//   info             print target, actual, vin, temp, mode, err
//   pos <deg>        set POS target to <deg> absolute (raw encoder frame)
//   rel <deg>        set POS target to current_actual + <deg>
//   go               start the slow-sweep test (see below)
//   stop             stop the slow-sweep test, hold current position
//   abort            any running long command — press Enter alone to abort
//
//  Slow-sweep test (`go`):
//   Streams a position target from current actual, advancing by DEG_PER_SEC
//   every STEP_MS milliseconds, for DURATION_MS. Configurable at top of this
//   file. Default: 12 °/s = 30 s per revolution. 5 ms period (200 Hz).
//   The motor's built-in 2 Hz LPF smooths our discrete steps into continuous
//   velocity — in theory, no jerk regardless of how small each step is.
// ============================================================

#include <Arduino.h>
#include "unit_rolleri2c.hpp"

// ---------- Hardware ----------
#define I2C_SDA 8
#define I2C_SCL 9
#define ROLLER_ADDR 0x64
#define STEPS_PER_REV 36000

// Mode 5 is hidden — not in roller_mode_t enum
#define MODE_POS_SPEED_VALUE 5

// MODE_SPEED is the library's built-in pure-velocity mode. In this mode the
// motor's internal speed PID drives current directly — no position cascade,
// no overshoot, no position-loop limit cycle. Correct control structure for
// slow smooth constant rotation.
//
// Library definition (lib/M5UnitRoller/unit_roller_common.hpp:35):
//   typedef enum { ROLLER_MODE_SPEED = 1, ... } roller_mode_t;
#define MODE_SPEED_VALUE 1

// ---------- gos (pure speed mode) defaults ----------
#define DEFAULT_GOS_DEG_PER_SEC   12.0f
#define DEFAULT_GOS_DURATION_MS   60000

// ---------- Slow-sweep test parameters ----------
#define SWEEP_DEG_PER_SEC   12.0f    // 30 sec per revolution
#define SWEEP_DURATION_MS   60000    // 60 seconds = 2 revolutions
#define SWEEP_STEP_MS       5        // 200 Hz target updates

// ---------- MODE_POS_SPEED cascaded-controller speed cap ----------
// The position PID output drives the speed PID setpoint. Without an explicit
// cap, the position loop can command a huge velocity setpoint on any small
// error, causing slam/overshoot/limit-cycle at slow sweep rates.
//
// The M5 library exposes `setSpeed(int32_t count)` which writes the speed
// register. Units are 0.01 RPM per count (100 counts = 1 RPM). 1 RPM = 6°/s,
// so 1°/s = ~16.67 counts. We write this before setOutput(1) on mode entry.
//
// 30°/s default = 5 RPM = 500 counts. 2.5× margin over the 12°/s sweep rate.
#define DEFAULT_POS_SPEED_DEG_PER_SEC  30.0f

UnitRollerI2C roller;
bool rollerReady = false;

// ---------- Helpers ----------
int32_t degToSteps(float deg) { return (int32_t)(deg * STEPS_PER_REV / 360.0f); }
float   stepsToDeg(int32_t s) { return s * 360.0f / STEPS_PER_REV; }

// Convert deg/s into the library's native speed count.
// 1 RPM = 6 deg/s = 100 counts  ->  1 deg/s = 100/6 ≈ 16.6667 counts.
int32_t degPerSecToSpeedCount(float dps) {
  return (int32_t)(dps * (100.0f / 6.0f));
}
float speedCountToDegPerSec(int32_t c) {
  return (float)c * (6.0f / 100.0f);
}

String rxBuf;
void printPrompt() { Serial.print("> "); }

// ---------- Mode 5 state ----------
int32_t  satRef     = 0;
bool     satRefSet  = false;
bool     mode5on    = false;

// Cached speed cap last pushed to the roller (in 0.01 RPM counts).
// Updated by cmdEnter() and by the `maxspeed` command.
int32_t  posSpeedCapCount = 0;

// ---------- Debug / diagnosis state ----------
// Streaming CSV-ish debug during `go` — toggle via "debug on" / "debug off".
bool     dbgStream  = false;
// Sign applied to the commanded position DELTA inside the sweep loop.
// Note: the encoder lives INSIDE the M5 Roller and we can't flip its sign
// from the host. The only thing we can invert here is the direction of the
// target ramp. If flipping this kills the oscillation, that strongly points
// to a commanded-direction / encoder-sign mismatch on the motor side.
int8_t   sweepDir   = 1;

void ensureMode5() {
  if (mode5on) return;
  int32_t here = roller.getPosReadback();
  roller.setOutput(0); delay(20);
  roller.setStallProtection(0); delay(20);
  roller.setMode((roller_mode_t)MODE_POS_SPEED_VALUE); delay(20);
  roller.setPosMaxCurrent(100000); delay(20);
  roller.setSpeedMaxCurrent(100000); delay(20);
  // Push speed-loop setpoint cap BEFORE re-enabling output, so the position
  // PID cannot command a runaway velocity on the first sample.
  posSpeedCapCount = degPerSecToSpeedCount(DEFAULT_POS_SPEED_DEG_PER_SEC);
  roller.setSpeed(posSpeedCapCount); delay(20);
  roller.setPos(here); delay(20);
  roller.setOutput(1);
  mode5on = true;
}

void streamPos(int32_t from, int32_t to, uint32_t durationMs) {
  ensureMode5();
  roller.setPos(from);
  delay(10);
  int32_t delta = to - from;
  uint32_t t0 = millis();
  while (true) {
    uint32_t t = millis() - t0;
    if (t >= durationMs) break;
    if (Serial.available()) {
      while (Serial.available()) Serial.read();
      Serial.println("aborted.");
      break;
    }
    float u = (float)t / (float)durationMs;
    int32_t target = from + (int32_t)(delta * u);
    roller.setPos(target);
    delay(SWEEP_STEP_MS);
  }
  roller.setPos(to);
}

// ---------- Single-key actions ----------
void keyR() {
  Serial.println("[R] releasing...");
  roller.setOutput(0);
  mode5on = false;
  delay(2500);
  int32_t rest = roller.getPosReadback();
  satRef = rest + degToSteps(190.0f);
  satRefSet = true;
  Serial.printf("[R] rest=%d (%.1f°) sat=%d (+190°)\n",
    rest, stepsToDeg(rest), satRef);
}

void key0() {
  if (!satRefSet) {
    int32_t rest = roller.getPosReadback();
    satRef = rest + degToSteps(190.0f);
    satRefSet = true;
    Serial.printf("[0] auto-lock sat=%d\n", satRef);
  }
  int32_t here = roller.getPosReadback();
  Serial.printf("[0] slow move to sat=%d over 3s\n", satRef);
  streamPos(here, satRef, 3000);
  Serial.println("[0] at sat.");
}

void key1() {
  int32_t here = roller.getPosReadback();
  int32_t target = here + STEPS_PER_REV;
  Serial.printf("[1] slow 360° (%d -> %d) over 30s\n", here, target);
  streamPos(here, target, 30000);
  if (satRefSet) satRef += STEPS_PER_REV;
  Serial.println("[1] done.");
}

void cmdInfo() {
  uint8_t mode = roller.getMotorMode();
  uint8_t out  = roller.getOutputStatus();
  uint8_t err  = roller.getErrorCode();
  int32_t target = roller.getPos();
  int32_t actual = roller.getPosReadback();
  int32_t vin    = roller.getVin();
  int32_t temp   = roller.getTemp();
  int32_t currA  = roller.getCurrentReadback();
  const char *modeName = "?";
  switch (mode) {
    case 1: modeName = "SPEED"; break;
    case 2: modeName = "POS"; break;
    case 3: modeName = "CURRENT"; break;
    case 4: modeName = "DIAL/ENCODER"; break;
    case 5: modeName = "POS_SPEED (hidden)"; break;
    case 6: modeName = "MAX(invalid)"; break;
    case 7: modeName = "SPEED_ERR_PROTECT"; break;
    case 8: modeName = "POS_ERR_PROTECT"; break;
  }
  Serial.printf("mode=%u (%s)  output=%u  err=%u\n", mode, modeName, out, err);
  Serial.printf("  target=%d (%.2f°)  actual=%d (%.2f°)  err=%+.2f°\n",
    target, stepsToDeg(target), actual, stepsToDeg(actual),
    stepsToDeg(target - actual));
  Serial.printf("  vin=%d  temp=%d  current_readback=%d (%.2f A)\n",
    vin, temp, currA, currA / 100000.0f);
  // Speed-loop setpoint cap used by MODE_POS_SPEED.
  int32_t spdReg = roller.getSpeed();
  Serial.printf("  speed cap: cached=%d (%.1f°/s)  live=%d (%.1f°/s)\n",
    posSpeedCapCount, speedCountToDegPerSec(posSpeedCapCount),
    spdReg,          speedCountToDegPerSec(spdReg));
}

void cmdEnter() {
  // Enter MODE_POS_SPEED at current position (no snap).
  int32_t here = roller.getPosReadback();
  roller.setOutput(0);
  delay(30);
  // Raw write of value 5 — library enum doesn't include it, cast through
  roller.setMode((roller_mode_t)MODE_POS_SPEED_VALUE);
  delay(30);
  // Set both max currents (POS PID and SPEED PID both active in this mode)
  roller.setPosMaxCurrent(100000);    // 1 A
  delay(20);
  roller.setSpeedMaxCurrent(100000);  // 1 A
  delay(20);
  // Push a conservative speed-loop setpoint cap BEFORE enabling output.
  // In MODE_POS_SPEED the position PID feeds a velocity setpoint into the
  // speed PID. Without a cap, small errors produce huge commanded speed
  // spikes -> limit-cycle. 30°/s default = 2.5× margin over 12°/s sweep.
  posSpeedCapCount = degPerSecToSpeedCount(DEFAULT_POS_SPEED_DEG_PER_SEC);
  roller.setSpeed(posSpeedCapCount);
  delay(20);
  roller.setPos(here);
  delay(20);
  roller.setOutput(1);
  mode5on = true;
  Serial.printf("entered MODE_POS_SPEED at encoder %d (%.2f°)\n",
    here, stepsToDeg(here));
  Serial.printf("speed cap = %d counts (%.1f°/s = %.2f RPM)\n",
    posSpeedCapCount,
    speedCountToDegPerSec(posSpeedCapCount),
    posSpeedCapCount / 100.0f);
  // Readback mode to verify
  delay(50);
  uint8_t m = roller.getMotorMode();
  Serial.printf("mode readback: %u %s\n", m,
    (m == MODE_POS_SPEED_VALUE) ? "✓ (mode 5 accepted)"
                                : "✗ (firmware did not accept 5)");
}

void cmdLeave() {
  roller.setOutput(0);
  Serial.println("output off.");
}

void cmdPIDQuery() {
  uint32_t p, i, d;
  roller.getPosPID(&p, &i, &d);
  Serial.printf("  pos PID:   P=%u  I=%u  D=%u\n", p, i, d);
  roller.getSpeedPID(&p, &i, &d);
  Serial.printf("  speed PID: P=%u  I=%u  D=%u\n", p, i, d);
}

// ---------- Slow sweep test ----------
// Streams position target from current actual, advancing by deg/sec for the
// configured duration. Motor's internal LPF smooths steps.
//
// Live commands accepted while `go` is running (Enter to submit):
//   <empty line>       abort sweep
//   invert             flip sweepDir in RAM, continue sweep
//   debug on / off     toggle CSV debug stream, continue sweep
//   anything else      brief note, sweep continues
void cmdGoSweep() {
  int32_t here = roller.getPosReadback();
  Serial.printf("slow sweep: %.2f°/s for %u ms (step every %u ms) dir=%+d\n",
    SWEEP_DEG_PER_SEC, SWEEP_DURATION_MS, SWEEP_STEP_MS, (int)sweepDir);
  Serial.printf("starting from encoder %d (%.2f°).\n",
    here, stepsToDeg(here));
  Serial.println("live cmds: <Enter>=abort  invert  debug on  debug off");

  // Ensure mode is 5 and output is on
  uint8_t m = roller.getMotorMode();
  if (m != MODE_POS_SPEED_VALUE) {
    Serial.println("not in POS_SPEED mode; run `enter` first.");
    return;
  }
  uint8_t out = roller.getOutputStatus();
  if (!out) {
    Serial.println("output not enabled; run `enter` first.");
    return;
  }

  int32_t startTarget = here;
  uint32_t t0 = millis();
  uint32_t lastDbg = 0;
  String   liveBuf;

  while (true) {
    uint32_t t = millis() - t0;
    if (t >= SWEEP_DURATION_MS) break;

    // ---- Live command handling (does NOT abort on any input) ----
    bool abortSweep = false;
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        String cmd = liveBuf;
        cmd.trim();
        liveBuf = "";
        if (cmd.length() == 0) {
          Serial.println("sweep aborted.");
          abortSweep = true;
          break;
        }
        cmd.toLowerCase();
        if (cmd == "invert") {
          sweepDir = -sweepDir;
          Serial.printf("[live] sweepDir = %+d\n", (int)sweepDir);
        } else if (cmd == "debug on") {
          dbgStream = true;
          Serial.println("[live] debug stream ON");
        } else if (cmd == "debug off") {
          dbgStream = false;
          Serial.println("[live] debug stream OFF");
        } else {
          Serial.printf("[live] ignored: '%s' (sweep continues)\n", cmd.c_str());
        }
      } else if (liveBuf.length() < 60) {
        liveBuf += c;
      }
    }
    if (abortSweep) break;

    // ---- Compute + send target ----
    float seconds = t / 1000.0f;
    int32_t targetOffset = degToSteps(SWEEP_DEG_PER_SEC * seconds);
    int32_t target = startTarget + (int32_t)sweepDir * targetOffset;
    roller.setPos(target);

    // ---- CSV-ish debug stream (gated ~20 Hz) ----
    if (dbgStream && (millis() - lastDbg >= 50)) {
      lastDbg = millis();
      int32_t actual = roller.getPosReadback();
      int32_t err    = target - actual;
      int32_t currA  = roller.getCurrentReadback();
      uint8_t eCode  = roller.getErrorCode();
      Serial.printf(
        "t=%lu pos=%d (%.2f°) tgt=%d (%.2f°) err=%+d (%+.2f°) dir=%+d iA=%d err_code=%u\n",
        (unsigned long)t,
        actual, stepsToDeg(actual),
        target, stepsToDeg(target),
        err,    stepsToDeg(err),
        (int)sweepDir, currA, eCode);
    }

    delay(SWEEP_STEP_MS);
  }
  // Hold at final target (motor continues to hold in POS_SPEED mode)
  Serial.println("sweep done. motor continues to hold final target.");
}

// ---------- MODE_SPEED (pure speed) sweep ----------
// Commands a constant speed setpoint via the library's native MODE_SPEED
// (ROLLER_MODE_SPEED = 1). No position cascade, no overshoot. Streams a
// debug CSV of pos/spd_sp/spd_actual/current while running. Exits on:
//   - <Enter> (empty line)
//   - "stop"
//   - duration elapsed
// On exit: speed = 0, then hop back to MODE_POS_SPEED to hold the final
// position so the arm doesn't free-fall.
void cmdGoSpeed(float dps, uint32_t durationMs) {
  int32_t spdCount = degPerSecToSpeedCount(dps);
  Serial.printf("gos: MODE_SPEED @ %.2f°/s (%d counts, %.2f RPM) for %u ms\n",
    dps, spdCount, spdCount / 100.0f, durationMs);

  // Switch into MODE_SPEED cleanly: output off, mode, current cap, speed, output on.
  roller.setOutput(0); delay(20);
  roller.setStallProtection(0); delay(20);
  roller.setMode((roller_mode_t)MODE_SPEED_VALUE); delay(20);
  roller.setSpeedMaxCurrent(100000);   // 1 A cap (same as mode-5 path)
  delay(20);
  roller.setSpeed(spdCount); delay(20);
  roller.setOutput(1);
  mode5on = false;   // we are NO LONGER in MODE_POS_SPEED

  // Verify mode landed
  delay(50);
  uint8_t m = roller.getMotorMode();
  Serial.printf("mode readback: %u %s\n", m,
    (m == MODE_SPEED_VALUE) ? "✓ (MODE_SPEED accepted)"
                            : "✗ (firmware did not accept mode 1)");

  Serial.println("live cmds: <Enter>=stop  'stop'=stop");

  uint32_t t0       = millis();
  uint32_t lastDbg  = 0;
  int32_t  prevPos  = roller.getPosReadback();
  uint32_t prevT    = t0;
  String   liveBuf;

  while (true) {
    uint32_t t = millis() - t0;
    if (t >= durationMs) break;

    // ---- Live command handling ----
    bool bailOut = false;
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        String cmd = liveBuf;
        cmd.trim();
        cmd.toLowerCase();
        liveBuf = "";
        if (cmd.length() == 0 || cmd == "stop") {
          Serial.println("gos stopped.");
          bailOut = true;
          break;
        } else if (cmd == "debug on") {
          dbgStream = true;
          Serial.println("[live] debug stream ON");
        } else if (cmd == "debug off") {
          dbgStream = false;
          Serial.println("[live] debug stream OFF");
        } else {
          Serial.printf("[live] ignored: '%s' (gos continues)\n", cmd.c_str());
        }
      } else if (liveBuf.length() < 60) {
        liveBuf += c;
      }
    }
    if (bailOut) break;

    // ---- Debug stream (~20 Hz) ----
    // spd_sp is what we commanded (stable, cached).
    // spd_actual is derived from position delta over wall-clock dt, since
    // getSpeedReadback() quantizes harshly at slow RPM.
    if (millis() - lastDbg >= 50) {
      uint32_t tNow   = millis();
      int32_t  pos    = roller.getPosReadback();
      int32_t  currA  = roller.getCurrentReadback();
      uint8_t  eCode  = roller.getErrorCode();
      int32_t  spdRb  = roller.getSpeedReadback();
      float    dt     = (tNow - prevT) / 1000.0f;
      float    spdAct = (dt > 0.0f) ? ((pos - prevPos) / dt) * (360.0f / STEPS_PER_REV)
                                    : 0.0f;
      Serial.printf(
        "t=%lu pos=%d (%.2f°) spd_sp=%.2f°/s spd_actual=%.2f°/s spd_rb=%d iA=%d err_code=%u\n",
        (unsigned long)t,
        pos, stepsToDeg(pos),
        dps, spdAct, spdRb, currA, eCode);
      prevPos = pos;
      prevT   = tNow;
      lastDbg = tNow;
    }
    delay(SWEEP_STEP_MS);
  }

  // ---- Clean exit: zero speed, hop to MODE_POS_SPEED to hold current pos ----
  roller.setSpeed(0); delay(20);
  int32_t here = roller.getPosReadback();
  roller.setOutput(0); delay(20);
  roller.setMode((roller_mode_t)MODE_POS_SPEED_VALUE); delay(20);
  roller.setPosMaxCurrent(100000); delay(20);
  roller.setSpeedMaxCurrent(100000); delay(20);
  posSpeedCapCount = degPerSecToSpeedCount(DEFAULT_POS_SPEED_DEG_PER_SEC);
  roller.setSpeed(posSpeedCapCount); delay(20);
  roller.setPos(here); delay(20);
  roller.setOutput(1);
  mode5on = true;
  Serial.printf("gos done. holding at %d (%.2f°) in MODE_POS_SPEED.\n",
    here, stepsToDeg(here));
}

// ---------- PID persistence test ----------
// Question being answered: in MODE_POS_SPEED, does writing PID gains via the
// I2C library actually stick, or does the M5 firmware re-install the
// auto-loaded "plus" bank on every control tick (~1.3 kHz) and clobber our
// writes? The MOTOR-NOTES §5 open question #2 — the Path A prerequisite.
//
// Method:
//   A. Read current gains (should be the "plus" defaults after `enter`).
//   B. Write a dramatically different low-gain set (P=100000=float 1.0,
//      I=0, D=0). Integer units per library API (uint32_t).
//   C. Wait 100 ms, read back.
//   D. Wait another 1000 ms (a full second of ~1.3 kHz control ticks —
//      plenty of time for any firmware clobber to fire), read back.
//   E. Restore original gains.
//   F. Print single-line VERDICT.
//
// `which` = 0 for position PID (default), 1 for speed PID.
void cmdPidTest(int which) {
  uint8_t m = roller.getMotorMode();
  if (m != MODE_POS_SPEED_VALUE) {
    Serial.printf("not in MODE_POS_SPEED (current mode=%u). run `stall0` then `enter` first.\n", m);
    return;
  }

  const char *label = (which == 1) ? "SPEED" : "POS";
  Serial.printf("pidtest %s: testing whether PID writes stick in MODE_POS_SPEED\n", label);

  uint32_t p0 = 0, i0 = 0, d0 = 0;
  if (which == 1) roller.getSpeedPID(&p0, &i0, &d0);
  else            roller.getPosPID  (&p0, &i0, &d0);
  Serial.printf("BEFORE: P=%u I=%u D=%u\n", p0, i0, d0);

  // Test values: integer equivalent of float P=1.0, I=0, D=0.
  // "plus" P=12.0 = integer 1200000, so float 1.0 = integer 100000.
  const uint32_t TEST_P = 100000;
  const uint32_t TEST_I = 0;
  const uint32_t TEST_D = 0;
  if (which == 1) roller.setSpeedPID(TEST_P, TEST_I, TEST_D);
  else            roller.setPosPID  (TEST_P, TEST_I, TEST_D);
  Serial.printf("WROTE: P=%u I=%u D=%u\n", TEST_P, TEST_I, TEST_D);

  delay(100);
  uint32_t p1 = 0, i1 = 0, d1 = 0;
  if (which == 1) roller.getSpeedPID(&p1, &i1, &d1);
  else            roller.getPosPID  (&p1, &i1, &d1);
  Serial.printf("READBACK (100ms): P=%u I=%u D=%u\n", p1, i1, d1);

  delay(1000);
  uint32_t p2 = 0, i2 = 0, d2 = 0;
  if (which == 1) roller.getSpeedPID(&p2, &i2, &d2);
  else            roller.getPosPID  (&p2, &i2, &d2);
  Serial.printf("READBACK (1100ms): P=%u I=%u D=%u\n", p2, i2, d2);

  // Restore originals — leave the motor in the state we found it.
  if (which == 1) roller.setSpeedPID(p0, i0, d0);
  else            roller.setPosPID  (p0, i0, d0);
  delay(20);
  Serial.printf("RESTORED: P=%u I=%u D=%u\n", p0, i0, d0);

  // Strict match — any single field reverting is a clobber.
  bool stuck100  = (p1 == TEST_P && i1 == TEST_I && d1 == TEST_D);
  bool stuck1100 = (p2 == TEST_P && i2 == TEST_I && d2 == TEST_D);
  if (stuck100 && stuck1100) {
    Serial.printf("VERDICT: PID_STICKS — %s-PID writes persist in MODE_POS_SPEED. Path A is alive.\n", label);
  } else {
    Serial.printf("VERDICT: PID_CLOBBERED — %s-PID writes were reverted by firmware (100ms=%s, 1100ms=%s). Path A is dead.\n",
      label,
      stuck100  ? "stuck"   : "clobbered",
      stuck1100 ? "stuck"   : "clobbered");
  }
}

void cmdStallOff() {
  roller.setStallProtection(0);
  delay(20);
  Serial.println("stall protection DISABLED.");
}

void cmdStallOn() {
  roller.setStallProtection(1);
  delay(20);
  Serial.println("stall protection ENABLED.");
}

// ---------- Command parser ----------
void handleLine(String line) {
  line.trim();
  if (line.length() == 0) { printPrompt(); return; }
  int sp = line.indexOf(' ');
  String cmd = (sp < 0) ? line : line.substring(0, sp);
  String arg = (sp < 0) ? ""   : line.substring(sp + 1);
  cmd.toLowerCase();

  if      (cmd == "help")   {
    Serial.println("commands:");
    Serial.println("  cal, enter, leave, info, pid?");
    Serial.println("  stall0, stall1, maxc <mA>");
    Serial.println("  maxspeed <deg_per_sec>   set speed-loop cap for MODE_POS_SPEED");
    Serial.printf ("                           (default %.1f°/s on `enter`; try 15 if oscillates)\n",
      DEFAULT_POS_SPEED_DEG_PER_SEC);
    Serial.println("  pospid <p> <i> <d>, spdpid <p> <i> <d>");
    Serial.println("  pidtest [speed]   verify PID writes stick in MODE_POS_SPEED (default: pos)");
    Serial.println("  pos <deg>, rel <deg>, go, stop");
    Serial.printf ("  gos [<deg_per_sec>] [<duration_s>]   MODE_SPEED pure-velocity sweep\n");
    Serial.printf ("                                        (defaults: %.1f°/s, %u s)\n",
      DEFAULT_GOS_DEG_PER_SEC, DEFAULT_GOS_DURATION_MS / 1000);
    Serial.println("  debug on|off   stream CSV-ish pos/tgt/err during go");
    Serial.println("  invert         flip commanded sweep direction (RAM)");
    Serial.println("  (during go: Enter=abort, or 'invert' / 'debug on|off')");
  }
  else if (cmd == "cal")   {
    Serial.println("calibrating (safe — no 2400 RPM post-spin)");
    roller.setOutput(0); delay(100);
    roller.startAngleCal(); delay(100);
    while (roller.getCalBusyStatus()) { delay(10); }
    roller.updateAngleCal(); delay(500);
    Serial.println("cal done.");
  }
  else if (cmd == "enter")  cmdEnter();
  else if (cmd == "leave")  cmdLeave();
  else if (cmd == "info")   cmdInfo();
  else if (cmd == "pid?")   cmdPIDQuery();
  else if (cmd == "stall0") cmdStallOff();
  else if (cmd == "stall1") cmdStallOn();
  else if (cmd == "maxc")   {
    int32_t mA = arg.toInt();
    int32_t counts = mA * 100;
    roller.setPosMaxCurrent(counts);
    delay(20);
    roller.setSpeedMaxCurrent(counts);
    delay(20);
    Serial.printf("max current = %d mA\n", mA);
  }
  else if (cmd == "maxspeed") {
    // Argument is deg/s (float). Converted to 0.01-RPM counts and written
    // via UnitRollerI2C::setSpeed(int32_t) — the speed-loop setpoint that
    // the MODE_POS_SPEED cascaded controller caps at.
    //
    // Library API used:
    //   void UnitRollerI2C::setSpeed(int32_t speed);
    // Writes to register I2C_SPEED_REG (0x40). Native unit: 0.01 RPM/count
    // (confirmed by examples/06_calibrate_and_hold: setSpeed(240000) = 2400 RPM).
    // 1 RPM = 6°/s -> 1°/s ≈ 16.667 counts.
    String a = arg; a.trim();
    if (a.length() == 0) {
      Serial.println("usage: maxspeed <deg_per_sec>   (e.g. `maxspeed 30` = 30°/s = 500 counts)");
      Serial.printf ("current cap: %d counts (%.1f°/s, %.2f RPM)\n",
        posSpeedCapCount,
        speedCountToDegPerSec(posSpeedCapCount),
        posSpeedCapCount / 100.0f);
    } else {
      float dps = a.toFloat();
      int32_t counts = degPerSecToSpeedCount(dps);
      roller.setSpeed(counts);
      delay(20);
      posSpeedCapCount = counts;
      Serial.printf("speed cap = %d counts  (%.2f°/s = %.3f RPM)\n",
        counts, dps, counts / 100.0f);
      Serial.println("note: caps the speed setpoint the position PID can command in MODE_POS_SPEED.");
    }
  }
  else if (cmd == "pospid") {
    int s1 = arg.indexOf(' '), s2 = arg.indexOf(' ', s1 + 1);
    if (s1 < 0 || s2 < 0) Serial.println("usage: pospid <p> <i> <d>");
    else {
      uint32_t p = arg.substring(0, s1).toInt();
      uint32_t i = arg.substring(s1 + 1, s2).toInt();
      uint32_t d = arg.substring(s2 + 1).toInt();
      roller.setPosPID(p, i, d);
      delay(20);
      Serial.printf("pos PID set: P=%u  I=%u  D=%u\n", p, i, d);
    }
  }
  else if (cmd == "spdpid") {
    int s1 = arg.indexOf(' '), s2 = arg.indexOf(' ', s1 + 1);
    if (s1 < 0 || s2 < 0) Serial.println("usage: spdpid <p> <i> <d>");
    else {
      uint32_t p = arg.substring(0, s1).toInt();
      uint32_t i = arg.substring(s1 + 1, s2).toInt();
      uint32_t d = arg.substring(s2 + 1).toInt();
      roller.setSpeedPID(p, i, d);
      delay(20);
      Serial.printf("speed PID set: P=%u  I=%u  D=%u\n", p, i, d);
    }
  }
  else if (cmd == "pos") {
    float deg = arg.toFloat();
    int32_t target = degToSteps(deg);
    roller.setPos(target);
    Serial.printf("POS target -> %.2f° (raw %d)\n", deg, target);
  }
  else if (cmd == "rel") {
    float deg = arg.toFloat();
    int32_t here = roller.getPosReadback();
    int32_t target = here + degToSteps(deg);
    roller.setPos(target);
    Serial.printf("POS target -> here(%d) + %.2f° = %d\n", here, deg, target);
  }
  else if (cmd == "pidtest") {
    String a = arg; a.trim(); a.toLowerCase();
    int which = (a == "speed") ? 1 : 0;
    cmdPidTest(which);
  }
  else if (cmd == "go")    cmdGoSweep();
  else if (cmd == "gos")   {
    // gos [<deg_per_sec>] [<duration_s>]
    // Defaults: DEFAULT_GOS_DEG_PER_SEC °/s, DEFAULT_GOS_DURATION_MS ms.
    String a = arg; a.trim();
    float    dps       = DEFAULT_GOS_DEG_PER_SEC;
    uint32_t durationMs = DEFAULT_GOS_DURATION_MS;
    if (a.length() > 0) {
      int sp2 = a.indexOf(' ');
      String dpsStr = (sp2 < 0) ? a : a.substring(0, sp2);
      String durStr = (sp2 < 0) ? "" : a.substring(sp2 + 1);
      dpsStr.trim(); durStr.trim();
      if (dpsStr.length() > 0) dps = dpsStr.toFloat();
      if (durStr.length() > 0) {
        float durS = durStr.toFloat();
        if (durS > 0.0f) durationMs = (uint32_t)(durS * 1000.0f);
      }
    }
    cmdGoSpeed(dps, durationMs);
  }
  else if (cmd == "stop")  {
    // Zero speed first (safe whether we're in MODE_SPEED or not), then hold
    // position in MODE_POS_SPEED. Covers the case where a prior `gos`
    // loop was bypassed somehow (e.g. dispatcher re-entry).
    roller.setSpeed(0);
    delay(20);
    int32_t here = roller.getPosReadback();
    roller.setPos(here);
    Serial.printf("holding at %d\n", here);
  }
  else if (cmd == "debug") {
    String a = arg; a.trim(); a.toLowerCase();
    if (a == "on")       { dbgStream = true;  Serial.println("debug stream ON"); }
    else if (a == "off") { dbgStream = false; Serial.println("debug stream OFF"); }
    else Serial.println("usage: debug on | debug off");
  }
  else if (cmd == "invert") {
    sweepDir = -sweepDir;
    Serial.printf("sweepDir = %+d (applies to next/running `go`)\n", (int)sweepDir);
  }
  else Serial.println("?");

  printPrompt();
}

// ---------- Setup / loop ----------
void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(500);
  Serial.println();
  Serial.println("==========================================================");
  Serial.println(" Witness · 07_pos_speed_mode (MODE 5 test)");
  Serial.println(" Type 'help' for commands.");
  Serial.println("==========================================================");
  Serial.println("SINGLE-KEY (no Enter):");
  Serial.println("  R  release arm, lock sat reference");
  Serial.println("  0  slow move to sat (3 s)");
  Serial.println("  1  slow 360° rotation (30 s)");

  if (roller.begin(&Wire, ROLLER_ADDR, I2C_SDA, I2C_SCL, 400000)) {
    rollerReady = true;
    Serial.println("roller found at 0x64.");
  } else {
    Serial.println("ERROR: roller not found.");
  }
  printPrompt();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (rxBuf.length() == 0) {
      if (c == 'R' || c == 'r') { keyR(); printPrompt(); continue; }
      if (c == '0')             { key0(); printPrompt(); continue; }
      if (c == '1')             { key1(); printPrompt(); continue; }
    }
    if (c == '\r') continue;
    if (c == '\n') {
      String line = rxBuf;
      rxBuf = "";
      handleLine(line);
    } else if (rxBuf.length() < 120) {
      rxBuf += c;
    }
  }
  delay(5);
}
