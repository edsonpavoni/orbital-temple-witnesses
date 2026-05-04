// ============================================================
//  Witness · 06_calibrate_and_hold
//  Run M5UnitRoller encoder calibration once, then verify
//  POSITION mode holds the arm against gravity.
//
//  Hardware: XIAO ESP32-S3 + M5 RollerCAN BLDC (I2C 0x64), pointer mounted,
//  sculpture FLAT ON THE TABLE (reduces axial load during cal).
//
//  Usage:
//    1. Flash, open serial at 115200, wait for prompt.
//    2. Type `cal` + Enter. Watch the motor do a short self-spin. DON'T touch
//       the arm while "Calibrating..." is printing. It stores the result to
//       motor flash — one-time per motor, survives reboots.
//    3. After calibration, firmware switches to POSITION mode and commands
//       the motor to hold at the CURRENT encoder reading. Try to move the
//       arm by hand. You should feel it resist.
//    4. Type `hold <deg>` to command an absolute angle in the motor's own
//       frame. `hold 0` = wherever the encoder reads 0 right now (raw).
//       `hold 90` = 90° from that. Try several. The arm should drive there
//       and hold. If yes, position mode is ALIVE. If no, calibration didn't
//       take or the PID needs tuning.
//    5. `info` prints current target/actual/error. `release` drops torque.
//       `stop` holds current position. `reboot` restarts the chip.
//
//  NOTE: after calibration we never command speeds > ~20 RPM in this example.
//  The stock M5 example ran 2400 RPM after cal — we explicitly do NOT, to
//  avoid flinging the pointer across the desk.
// ============================================================

#include <Arduino.h>
#include "unit_rolleri2c.hpp"

// ---------- Hardware ----------
#define I2C_SDA 8
#define I2C_SCL 9
#define ROLLER_ADDR 0x64
#define STEPS_PER_REV 36000

UnitRollerI2C roller;

// ---------- State ----------
bool     rollerReady    = false;
bool     calibrated     = false;
bool     holdingPos     = false;
int32_t  holdTargetRaw  = 0;     // raw encoder steps being commanded

// ---------- Helpers ----------
int32_t degToSteps(float deg) {
  return (int32_t)(deg * STEPS_PER_REV / 360.0f);
}

float stepsToDeg(int32_t steps) {
  return steps * 360.0f / STEPS_PER_REV;
}

void enterPositionHold(int32_t targetRaw) {
  roller.setOutput(0);
  delay(30);
  roller.setMode(ROLLER_MODE_POSITION);
  delay(30);
  // Holding current ceiling. 100000 = 1 A (short-term spec; 0.5 A continuous).
  roller.setPosMaxCurrent(100000);
  delay(30);
  roller.setPos(targetRaw);
  delay(30);
  roller.setOutput(1);
  holdTargetRaw = targetRaw;
  holdingPos    = true;
}

// Single-key ritual controls (no Enter needed).
int32_t  satRef_key    = 0;
bool     satRefSet_key = false;

// Smooth trajectory: steps the position target from current actual to
// `finalTarget` over `durationMs`. Internal PID follows.
// If not yet in POSITION mode, arms it at current encoder reading first.
void smoothMoveTo(int32_t finalTarget, uint32_t durationMs) {
  if (!holdingPos) {
    // First entry: arm position mode at current actual so we don't snap.
    int32_t here = roller.getPosReadback();
    enterPositionHold(here);
  }
  // Always START from the motor's actual current reading, not a stale
  // cached value. Prevents multi-revolution runaway if cached state drifted.
  int32_t startTarget = roller.getPosReadback();
  int32_t delta       = finalTarget - startTarget;
  uint32_t t0         = millis();
  const uint32_t STEP_MS = 5;  // 200 Hz — fine enough to dissolve visible steps
  while (true) {
    uint32_t t = millis() - t0;
    if (t >= durationMs) break;
    // smoothstep: 3t^2 - 2t^3, eases in and out
    float u  = (float)t / (float)durationMs;
    float s  = u * u * (3.0f - 2.0f * u);
    int32_t target = startTarget + (int32_t)(delta * s);
    roller.setPos(target);
    delay(STEP_MS);
  }
  roller.setPos(finalTarget);
  holdTargetRaw = finalTarget;
}

void printPrompt() {
  Serial.print("> ");
}

void printHelp() {
  Serial.println();
  Serial.println("commands:");
  Serial.println("  cal             encoder calibration only (safe with pointer)");
  Serial.println("  caltest         cal + 2400 RPM verify spin (BENCH ONLY, no pointer!)");
  Serial.println("  hold <deg>      smooth move to absolute angle over ~1.5 s");
  Serial.println("  jump <deg>      instant move (max speed, test stiffness)");
  Serial.println("  nudge <deg>     smooth move by +<deg> from current target");
  Serial.println("  sweep <deg> <s> POSITION-mode move +<deg> over <s> s (may be jerky)");
  Serial.println("  vsweep <deg> <s> SPEED-mode move +<deg> over <s> s (constant RPM)");
  Serial.println("  ssweep <deg> <s> SPEED-mode w/ sine velocity profile (Feb 4 method)");
  Serial.println("  osc <amp> <T> <total_s>  POSITION-mode ± amp sine around current pos");
  Serial.println("  ritual          FULL RITUAL LOOP: ssweep + oscillation, any key aborts");
  Serial.println("  rpm <rpm>       speed-mode spin at given RPM (negative for reverse)");
  Serial.println("  sstop           stop speed mode, release motor");
  Serial.println("  pid?            print position PID (holds angle against load)");
  Serial.println("  pid <p> <i> <d> set position PID (raise P for stiffer hold)");
  Serial.println("  spid?           print speed PID (smoothness of continuous motion)");
  Serial.println("  spid <p> <i> <d> set speed PID (lower for smoother motion)");
  Serial.println("  err             read motor error code (0 = no error)");
  Serial.println("  sys             read system status + mode + output state");
  Serial.println("  clear           clear stall protection trip, re-enable output");
  Serial.println("  info            print target/actual/error, mode, voltage");
  Serial.println("  watch           stream vin + err once per second (any key stops)");
  Serial.println("  stop            hold at current actual position");
  Serial.println("  release         drop torque (arm goes limp)");
  Serial.println("  reboot          restart ESP32");
  Serial.println("  help            this list");
  Serial.println();
  Serial.println("SINGLE-KEY CONTROLS (no Enter needed):");
  Serial.println("  R   release arm, lock reference for sat=0°");
  Serial.println("  0   slow position-mode move to sat (0°, +190° from rest)");
  Serial.println("  3   one 360° rotation as slow as possible (10 s, sine vel)");
  Serial.println();
}

// ---------- Single-key ritual actions ----------
void keyR() {
  Serial.println("[R] release. settling 2.5 s...");
  roller.setOutput(0);
  holdingPos = false;
  delay(2500);
  int32_t rest = roller.getPosReadback();
  satRef_key = rest + degToSteps(190.0f);
  satRefSet_key = true;
  Serial.printf("[R] rest=%d (%.2f°)  sat=%d (+190°)\n",
    rest, stepsToDeg(rest), satRef_key);
}

void key0() {
  // Apply tuned PIDs (idempotent)
  roller.setPosPID(15000000, 30, 40000000); delay(20);
  roller.setPosMaxCurrent(100000);           delay(20);
  if (!satRefSet_key) {
    // Use CURRENT reading as rest reference
    int32_t rest = roller.getPosReadback();
    satRef_key = rest + degToSteps(190.0f);
    satRefSet_key = true;
    Serial.printf("[0] auto-lock rest=%d, sat=%d\n", rest, satRef_key);
  }
  Serial.printf("[0] slow move to sat=%d over 3 s\n", satRef_key);
  int32_t here = roller.getPosReadback();
  enterPositionHold(here);
  delay(100);
  smoothMoveTo(satRef_key, 3000);
  Serial.println("[0] arrived.");
}

void key3() {
  // POSITION-MODE sweep over 10 s. Position mode doesn't have a speed
  // deadband — it commands whatever current is needed to track trajectory.
  // Stiff PID (P=15M, D=40M) drives through gravity resistance.
  // Ends precisely at target, no speed-integration drift.
  Serial.println("[3] 360° POSITION-mode sweep over 10 s (drives through gravity)");
  roller.setPosPID(15000000, 30, 40000000); delay(20);
  roller.setPosMaxCurrent(100000);           delay(20);

  int32_t here = roller.getPosReadback();
  int32_t target;
  if (satRefSet_key) {
    target = satRef_key + STEPS_PER_REV;
  } else {
    target = here + STEPS_PER_REV;
  }
  enterPositionHold(here);
  delay(100);
  smoothMoveTo(target, 10000);
  if (satRefSet_key) satRef_key = target;
  Serial.println("[3] done, at sat.");
}

// ---------- Calibration routine ----------
// Matches M5's canonical example exactly:
//   https://github.com/m5stack/M5Unit-Roller/blob/main/examples/i2c/
//   encoder_calibration/encoder_calibration.ino
// No polling delay, no safety timeout, identical post-cal delay.
// After cal, we do NOT run the 2400 RPM SPEED-mode test from the stock
// example (that would be destructive with a pointer mounted).
void runCalibration() {
  if (!rollerReady) { Serial.println("no roller."); return; }

  Serial.println();
  Serial.println(">>> ENCODER CALIBRATION <<<");
  Serial.println("Shaft must be FREE (no load, no friction). Do NOT touch it.");
  Serial.println("Starting in 3 seconds...");
  for (int i = 3; i > 0; i--) { Serial.printf("  %d\n", i); delay(1000); }

  uint32_t t0 = millis();

  roller.setOutput(0);
  delay(100);

  roller.startAngleCal();
  delay(100);

  Serial.println("Calibrating... (M5 example tight-loops the busy flag)");
  uint32_t busyTicks = 0;
  while (roller.getCalBusyStatus()) {
    busyTicks++;               // count polls, no delay (matches M5)
  }

  roller.updateAngleCal();
  delay(500);

  Serial.printf("calibration done in %lu ms. busy-poll ticks=%lu.\n",
    millis() - t0, busyTicks);
  Serial.println("Stored to motor flash.");

  uint8_t err = roller.getErrorCode();
  if (err != 0) {
    Serial.printf("WARNING: post-cal error code = %u\n", err);
  }

  calibrated = true;

  // NOTE: the M5 stock example runs a 2400 RPM verification spin after cal.
  // We do NOT do that in `cal` because on a sculpture with pointer mounted,
  // 2400 RPM will damage it. Use `caltest` on BENCH ONLY for that sequence.

  // Switch to POSITION mode and hold at current actual position.
  int32_t here = roller.getPosReadback();
  Serial.printf("current encoder: %d steps (%.2f°)\n", here, stepsToDeg(here));
  Serial.println("entering POSITION HOLD at current position.");
  Serial.println("try to move the arm by hand — you should feel it resist.");
  enterPositionHold(here);
}

// ---------- Command parser ----------
String rxBuf;

void handleLine(String line) {
  line.trim();
  if (line.length() == 0) { printPrompt(); return; }

  int sp = line.indexOf(' ');
  String cmd = (sp < 0) ? line : line.substring(0, sp);
  String arg = (sp < 0) ? ""   : line.substring(sp + 1);
  cmd.toLowerCase();

  if (cmd == "help") {
    printHelp();
  }
  else if (cmd == "cal") {
    runCalibration();
  }
  else if (cmd == "caltest") {
    runCalibration();
    // BENCH-ONLY verification: 2400 RPM spin for 5 seconds.
    Serial.println();
    Serial.println(">>> BENCH-ONLY: 2400 RPM verification spin <<<");
    Serial.println("Make sure shaft is free. Starting in 3 s...");
    delay(3000);
    roller.setOutput(0);
    roller.setMode(ROLLER_MODE_SPEED);
    roller.setSpeed(240000);
    roller.setSpeedMaxCurrent(100000);
    roller.setOutput(1);
    for (int s = 5; s > 0; s--) {
      Serial.printf("  spinning, %d s remaining\n", s);
      delay(1000);
    }
    roller.setSpeed(0); delay(200);
    roller.setOutput(0);
    int32_t here = roller.getPosReadback();
    enterPositionHold(here);
    Serial.println("caltest done. motor holding at current position.");
  }
  else if (cmd == "hold") {
    // Interpret <deg> as ABSOLUTE artwork angle, but compute target in the
    // same revolution as the current actual position so we never command
    // huge multi-revolution moves.
    if (!calibrated) { Serial.println("note: not calibrated yet, may not hold."); }
    float deg = arg.toFloat();
    int32_t here      = roller.getPosReadback();
    int32_t hereRev   = here - (here % STEPS_PER_REV);  // nearest lower multiple
    int32_t target    = hereRev + degToSteps(deg);
    // Pick the wrap that's closest to `here` (+/- one rev)
    if (target - here >  STEPS_PER_REV / 2) target -= STEPS_PER_REV;
    if (target - here < -STEPS_PER_REV / 2) target += STEPS_PER_REV;
    Serial.printf("smooth hold -> %.2f° (near current rev, target=%d, here=%d) over 1.5 s\n",
      deg, target, here);
    smoothMoveTo(target, 1500);
    Serial.println("arrived.");
  }
  else if (cmd == "jump") {
    if (!calibrated) { Serial.println("note: not calibrated yet, may not hold."); }
    float deg = arg.toFloat();
    int32_t here      = roller.getPosReadback();
    int32_t hereRev   = here - (here % STEPS_PER_REV);
    int32_t target    = hereRev + degToSteps(deg);
    if (target - here >  STEPS_PER_REV / 2) target -= STEPS_PER_REV;
    if (target - here < -STEPS_PER_REV / 2) target += STEPS_PER_REV;
    enterPositionHold(target);
    Serial.printf("jump -> %.2f° (target=%d, here=%d) [max speed]\n", deg, target, here);
  }
  else if (cmd == "nudge") {
    // Relative to CURRENT actual, not stale holdTargetRaw.
    float deg = arg.toFloat();
    int32_t here = roller.getPosReadback();
    int32_t target = here + degToSteps(deg);
    Serial.printf("nudge %+.2f° from here(%d) -> target %d over 1.0 s\n",
      deg, here, target);
    smoothMoveTo(target, 1000);
  }
  else if (cmd == "rpm") {
    // Speed mode continuous spin. units: 0.01 RPM/count → 100 = 1 RPM.
    float rpm = arg.toFloat();
    int32_t speedCount = (int32_t)(rpm * 100.0f);
    roller.setOutput(0); delay(20);
    roller.setMode(ROLLER_MODE_SPEED); delay(20);
    roller.setSpeedMaxCurrent(100000); delay(20);   // 1 A
    roller.setSpeed(speedCount); delay(20);
    roller.setOutput(1);
    holdingPos = false;
    Serial.printf("speed mode: %.2f RPM (count=%d)\n", rpm, speedCount);
  }
  else if (cmd == "sstop") {
    roller.setSpeed(0); delay(20);
    roller.setOutput(0);
    holdingPos = false;
    Serial.println("speed stopped, motor released.");
  }
  else if (cmd == "vsweep") {
    // SPEED-mode sweep. Runs constant RPM for the duration, then stops.
    // Much smoother than POSITION-mode sweep for continuous motion.
    int sp = arg.indexOf(' ');
    if (sp < 0) { Serial.println("usage: vsweep <deg> <seconds>"); }
    else {
      float deg = arg.substring(0, sp).toFloat();
      float sec = arg.substring(sp + 1).toFloat();
      if (sec < 0.1f) sec = 0.1f;
      // RPM = (deg / 360) / (sec / 60) = deg / (6 * sec)
      float rpm = deg / (6.0f * sec);
      int32_t speedCount = (int32_t)(rpm * 100.0f);
      Serial.printf("vsweep %+.2f° over %.2f s -> %.3f RPM (count=%d)\n",
        deg, sec, rpm, speedCount);
      roller.setOutput(0); delay(20);
      roller.setMode(ROLLER_MODE_SPEED); delay(20);
      roller.setSpeedMaxCurrent(100000); delay(20);
      roller.setSpeed(speedCount); delay(20);
      roller.setOutput(1);
      holdingPos = false;
      uint32_t t0 = millis();
      uint32_t durMs = (uint32_t)(sec * 1000.0f);
      while (millis() - t0 < durMs) { delay(10); }
      roller.setSpeed(0); delay(20);
      roller.setOutput(0);
      int32_t here = roller.getPosReadback();
      Serial.printf("done. encoder now at %d (%.2f°).\n", here, stepsToDeg(here));
    }
  }
  else if (cmd == "osc") {
    // ± amplitude oscillation around current position, position mode, sine.
    // Aborts on any serial input.
    int s1 = arg.indexOf(' ');
    int s2 = arg.indexOf(' ', s1 + 1);
    if (s1 < 0 || s2 < 0) {
      Serial.println("usage: osc <amp_deg> <period_s> <total_s>");
    } else {
      float amp    = arg.substring(0, s1).toFloat();
      float period = arg.substring(s1 + 1, s2).toFloat();
      float total  = arg.substring(s2 + 1).toFloat();
      if (period < 0.5f) period = 0.5f;
      int32_t center = roller.getPosReadback();
      enterPositionHold(center);
      int32_t ampSteps = degToSteps(amp);
      Serial.printf("osc ±%.2f° period=%.2fs total=%.2fs center=%d\n",
        amp, period, total, center);
      uint32_t t0 = millis();
      uint32_t durMs = (uint32_t)(total * 1000.0f);
      float omega = 2.0f * PI / period;
      while (true) {
        uint32_t t = millis() - t0;
        if (t >= durMs) break;
        if (Serial.available()) {
          while (Serial.available()) Serial.read();
          Serial.println("osc aborted.");
          break;
        }
        float seconds = t / 1000.0f;
        int32_t target = center + (int32_t)(ampSteps * sinf(omega * seconds));
        roller.setPos(target);
        delay(5);   // 200 Hz
      }
      // Settle back to center
      roller.setPos(center);
      holdTargetRaw = center;
      Serial.println("osc done. holding at center.");
    }
  }
  else if (cmd == "ritual") {
    // RITUAL v2 (per Edson 2026-04-15 18:07):
    //   Setup: release arm (falls to gravity rest), apply tuned PIDs.
    //   Opening: SLOW position-mode lift from rest to sat (+190°) over 3 s.
    //   Loop:
    //     - Hold 10 s at sat.
    //     - Rotate 360° in 6 s (speed mode, sine velocity, 1 rev).
    //     - Snap to next sat (exact encoder).
    //   Abort: any serial input + Enter.
    Serial.println("RITUAL v2 starting.");
    Serial.println("Any key + Enter aborts mid-cycle.");

    // Release, let arm fall to gravity rest
    Serial.println("[setup] releasing arm, settling at rest...");
    roller.setOutput(0);
    holdingPos = false;
    delay(2500);

    // Apply tuned PIDs
    roller.setPosPID(15000000, 30, 40000000);   delay(30);
    roller.setSpeedPID(1500000, 1000, 1000000); delay(30);
    roller.setPosMaxCurrent(100000);            delay(30);
    roller.setSpeedMaxCurrent(100000);          delay(30);

    int32_t restPos = roller.getPosReadback();
    int32_t satRef  = restPos + degToSteps(190.0f);
    Serial.printf("rest=%d (%.2f°) sat=%d (+190°)\n",
      restPos, stepsToDeg(restPos), satRef);

    // --- Opening: SLOW lift to sat (3 s trajectory) ---
    // Arm position mode at CURRENT position so no snap at start.
    Serial.println("[opening] slow lift to sat (3 s)");
    enterPositionHold(restPos);
    delay(200);
    smoothMoveTo(satRef, 3000);   // 200 Hz, smoothstep, our existing helper

    while (true) {
      if (Serial.available()) {
        while (Serial.available()) Serial.read();
        Serial.println("ritual aborted.");
        break;
      }

      // --- HOLD 10 s at sat ---
      Serial.println("[hold] 10 s at sat");
      uint32_t tH = millis();
      bool aborted = false;
      while (millis() - tH < 10000) {
        if (Serial.available()) {
          while (Serial.available()) Serial.read();
          aborted = true; break;
        }
        delay(50);
      }
      if (aborted) { Serial.println("aborted in hold."); break; }

      // --- ROTATE 360° in 6 s (speed mode, sine velocity) ---
      // avg = 10 RPM, peak = 20 RPM (well above 11 RPM deadband).
      Serial.println("[rotate] 360° in 6 s (avg 10 RPM, peak 20 RPM)");
      roller.setOutput(0); delay(20);
      roller.setMode(ROLLER_MODE_SPEED); delay(20);
      roller.setSpeed(0); delay(20);
      roller.setOutput(1);
      holdingPos = false;

      const uint32_t ROT_MS = 6000;
      const float v_avg_rpm = 10.0f;   // 360°/6s = 10 RPM avg
      uint32_t tR = millis();
      while (true) {
        uint32_t t = millis() - tR;
        if (t >= ROT_MS) break;
        if (Serial.available()) {
          while (Serial.available()) Serial.read();
          aborted = true; break;
        }
        float u       = (float)t / (float)ROT_MS;
        float factor  = 1.0f - cosf(2.0f * PI * u);
        int32_t speedCount = (int32_t)(v_avg_rpm * 100.0f * factor);
        roller.setSpeed(speedCount);
        delay(10);
      }
      roller.setSpeed(0); delay(100);
      if (aborted) {
        roller.setOutput(0);
        Serial.println("aborted in rotate.");
        break;
      }

      // --- SNAP to next sat (1 full rev forward in encoder frame) ---
      int32_t nextSat = satRef + STEPS_PER_REV;
      roller.setOutput(0); delay(20);
      roller.setMode(ROLLER_MODE_POSITION); delay(20);
      roller.setPos(nextSat); delay(20);
      roller.setOutput(1);
      satRef = nextSat;
      holdTargetRaw = nextSat;
      holdingPos = true;
      delay(500);
      Serial.println("[cycle done] looping...");
    }
  }
  else if (cmd == "ssweep") {
    // SPEED-mode SINE-VELOCITY profile. Matches the Feb 4 approach that
    // produced smooth motion on the loaded sculpture.
    //   v(t) = v_avg * (1 - cos(2π t / T))
    // Starts at 0, peaks at 2*v_avg mid-sweep, returns to 0. Updated at 100 Hz.
    int sp = arg.indexOf(' ');
    if (sp < 0) { Serial.println("usage: ssweep <deg> <seconds>"); }
    else {
      float deg = arg.substring(0, sp).toFloat();
      float sec = arg.substring(sp + 1).toFloat();
      if (sec < 0.3f) sec = 0.3f;
      // deg/sec → avg RPM = (deg/360) / (sec/60) = deg / (6*sec)
      float v_avg_rpm = deg / (6.0f * sec);
      float v_peak_rpm = 2.0f * v_avg_rpm;
      Serial.printf("ssweep %+.2f° over %.2f s. avg=%.2f RPM peak=%.2f RPM\n",
        deg, sec, v_avg_rpm, v_peak_rpm);
      if (v_peak_rpm < 11.0f) {
        Serial.println("WARNING: peak RPM below deadband (~11 RPM). Motion may stall.");
      }
      roller.setOutput(0); delay(20);
      roller.setMode(ROLLER_MODE_SPEED); delay(20);
      roller.setSpeedMaxCurrent(100000); delay(20);
      roller.setSpeed(0); delay(20);
      roller.setOutput(1);
      holdingPos = false;
      uint32_t t0 = millis();
      uint32_t durMs = (uint32_t)(sec * 1000.0f);
      while (true) {
        uint32_t t = millis() - t0;
        if (t >= durMs) break;
        float u = (float)t / (float)durMs;
        float factor = 1.0f - cosf(2.0f * PI * u);   // 0 → 2 → 0
        int32_t speedCount = (int32_t)(v_avg_rpm * 100.0f * factor);
        roller.setSpeed(speedCount);
        delay(10);   // 100 Hz update
      }
      roller.setSpeed(0); delay(20);
      roller.setOutput(0);
      int32_t here = roller.getPosReadback();
      Serial.printf("done. encoder now at %d (%.2f°).\n", here, stepsToDeg(here));
    }
  }
  else if (cmd == "sweep") {
    int sp = arg.indexOf(' ');
    if (sp < 0) { Serial.println("usage: sweep <deg> <seconds>"); }
    else {
      float deg = arg.substring(0, sp).toFloat();
      float sec = arg.substring(sp + 1).toFloat();
      if (sec < 0.1f) sec = 0.1f;
      int32_t here = roller.getPosReadback();
      int32_t target = here + degToSteps(deg);
      uint32_t durMs = (uint32_t)(sec * 1000.0f);
      Serial.printf("sweep %+.2f° over %.2f s (here=%d -> target=%d)\n",
        deg, sec, here, target);
      smoothMoveTo(target, durMs);
      Serial.println("arrived.");
    }
  }
  else if (cmd == "pid?") {
    uint32_t p, i, d;
    roller.getPosPID(&p, &i, &d);
    Serial.printf("pos PID:  P=%u  I=%u  D=%u\n", p, i, d);
  }
  else if (cmd == "spid?") {
    uint32_t p, i, d;
    roller.getSpeedPID(&p, &i, &d);
    Serial.printf("speed PID:  P=%u  I=%u  D=%u\n", p, i, d);
  }
  else if (cmd == "spid") {
    int s1 = arg.indexOf(' ');
    int s2 = arg.indexOf(' ', s1 + 1);
    if (s1 < 0 || s2 < 0) {
      Serial.println("usage: spid <p> <i> <d>");
    } else {
      uint32_t p = (uint32_t)arg.substring(0, s1).toInt();
      uint32_t i = (uint32_t)arg.substring(s1 + 1, s2).toInt();
      uint32_t d = (uint32_t)arg.substring(s2 + 1).toInt();
      roller.setSpeedPID(p, i, d);
      delay(30);
      uint32_t pr, ir, dr;
      roller.getSpeedPID(&pr, &ir, &dr);
      Serial.printf("speed PID set. readback: P=%u  I=%u  D=%u\n", pr, ir, dr);
    }
  }
  else if (cmd == "pid") {
    // "pid 1000 10 500" etc.
    int s1 = arg.indexOf(' ');
    int s2 = arg.indexOf(' ', s1 + 1);
    if (s1 < 0 || s2 < 0) {
      Serial.println("usage: pid <p> <i> <d>");
    } else {
      uint32_t p = (uint32_t)arg.substring(0, s1).toInt();
      uint32_t i = (uint32_t)arg.substring(s1 + 1, s2).toInt();
      uint32_t d = (uint32_t)arg.substring(s2 + 1).toInt();
      roller.setPosPID(p, i, d);
      delay(30);
      uint32_t pr, ir, dr;
      roller.getPosPID(&pr, &ir, &dr);
      Serial.printf("pos PID set. readback: P=%u  I=%u  D=%u\n", pr, ir, dr);
    }
  }
  else if (cmd == "info") {
    int32_t target = roller.getPos();
    int32_t actual = roller.getPosReadback();
    int32_t vin    = roller.getVin();
    int32_t temp   = roller.getTemp();
    Serial.printf("target %d (%.2f°)   actual %d (%.2f°)   err %+.2f°\n",
      target, stepsToDeg(target), actual, stepsToDeg(actual),
      stepsToDeg(target - actual));
    Serial.printf("vin %d   temp %d   calibrated=%d   holding=%d\n",
      vin, temp, calibrated ? 1 : 0, holdingPos ? 1 : 0);
  }
  else if (cmd == "err") {
    uint8_t e = roller.getErrorCode();
    Serial.printf("error code: %u  ", e);
    switch (e) {
      case 0: Serial.println("(no error)"); break;
      case 1: Serial.println("(E:1 — over-voltage?)"); break;
      case 2: Serial.println("(E:2)"); break;
      case 3: Serial.println("(E:3)"); break;
      default: Serial.println("(unknown)"); break;
    }
  }
  else if (cmd == "sys") {
    uint8_t sys  = roller.getSysStatus();
    uint8_t out  = roller.getOutputStatus();
    uint8_t mode = roller.getMotorMode();
    uint8_t err  = roller.getErrorCode();
    const char *modeName = "?";
    switch (mode) {
      case 1: modeName = "SPEED";    break;
      case 2: modeName = "POSITION"; break;
      case 3: modeName = "CURRENT";  break;
      case 4: modeName = "ENCODER";  break;
    }
    Serial.printf("sys=0x%02X  output=%u  mode=%u (%s)  err=%u\n",
      sys, out, mode, modeName, err);
  }
  else if (cmd == "clear") {
    roller.resetStalledProtect();
    delay(50);
    uint8_t e = roller.getErrorCode();
    Serial.printf("stall protect reset. error now: %u\n", e);
  }
  else if (cmd == "watch") {
    Serial.println("streaming... press any key + Enter to stop.");
    Serial.println("vin units: 10 mV per count (USB ~500 = 5V, PD 15V ~1500)");
    while (true) {
      int32_t vin    = roller.getVin();
      int32_t target = roller.getPos();
      int32_t actual = roller.getPosReadback();
      int32_t temp   = roller.getTemp();
      float vv       = vin / 100.0f;   // volts
      Serial.printf("vin=%d (%.2fV)  err=%+.2f°  temp=%d\n",
        vin, vv, stepsToDeg(target - actual), temp);
      uint32_t t0 = millis();
      while (millis() - t0 < 1000) {
        if (Serial.available()) {
          while (Serial.available()) Serial.read();
          Serial.println("stopped.");
          goto watch_done;
        }
        delay(20);
      }
    }
    watch_done:;
  }
  else if (cmd == "stop") {
    int32_t here = roller.getPosReadback();
    enterPositionHold(here);
    Serial.printf("holding at %d (%.2f°)\n", here, stepsToDeg(here));
  }
  else if (cmd == "release") {
    roller.setOutput(0);
    holdingPos = false;
    Serial.println("released. arm is free.");
  }
  else if (cmd == "reboot") {
    Serial.println("rebooting...");
    delay(200);
    ESP.restart();
  }
  else {
    Serial.println("?");
  }

  printPrompt();
}

// ---------- Arduino ----------
void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(500);
  Serial.println();
  Serial.println("==========================================================");
  Serial.println(" Witness · calibrate_and_hold");
  Serial.println(" Type 'help' for commands. Type 'cal' to run calibration.");
  Serial.println("==========================================================");

  if (roller.begin(&Wire, ROLLER_ADDR, I2C_SDA, I2C_SCL, 400000)) {
    rollerReady = true;
    Serial.println("roller found at 0x64.");
  } else {
    Serial.println("ERROR: roller not found. check wiring.");
  }

  printPrompt();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    // Single-key controls fire immediately ONLY when the input buffer is
    // empty (so digits inside multi-char commands like "pid 6000000 ..."
    // aren't intercepted).
    if (rxBuf.length() == 0) {
      if (c == 'R' || c == 'r') { keyR(); printPrompt(); continue; }
      if (c == '0')             { key0(); printPrompt(); continue; }
      if (c == '3')             { key3(); printPrompt(); continue; }
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
