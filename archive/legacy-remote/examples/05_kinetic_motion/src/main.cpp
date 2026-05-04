/*
 * ============================================================================
 *  KINETIC MOTION LAB
 *  Standalone firmware for tuning the Witness sculpture's motion language.
 *  Serial-driven. No WiFi, no HTTP, no schedule. Just motor + shell.
 *
 *  See KINETIC-MOTION.md for the task list and results log.
 *
 *  Usage:
 *    pio run --target upload
 *    pio device monitor --port /dev/cu.usbmodem101 --baud 115200
 *    > help
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

// ---------------- Hardware config ----------------
#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8
#define I2C_SCL_PIN      9
#define I2C_FREQ         400000

#define REG_OUTPUT       0x00
#define REG_MODE         0x01
#define REG_STALL_PROT   0x0F
#define REG_RGB          0x30
#define REG_SPEED        0x40
#define REG_SPEED_MAXCUR 0x50
#define REG_POS_READ     0x90
// Telemetry / power registers — read-only.
// The RollerCAN exposes voltage / current / temperature feedback. Exact
// addresses vary slightly by firmware revision; the `power` command probes
// the common candidates so we can identify which one moves when current
// draw changes.
#define REG_VBUS_MV      0x60   // candidate: input voltage in mV
#define REG_TEMP_C       0x64   // candidate: motor temperature
#define REG_ACTUAL_CUR   0x70   // candidate: actual motor current

#define MODE_POSITION    0
#define MODE_SPEED       1
#define REG_POS_TARGET   0x80   // M5 RollerCAN position-mode target (32-bit, encoder steps)

#define STEPS_PER_REV    36000
#define CURRENT_LIMIT    200000  // 2A

// ---------------- State ----------------
int32_t zeroSteps    = 0;        // after calibration, this is the encoder pos that corresponds to 0°
bool    zeroCalibrated = false;
float   lastCommandedRpm = 0;

// ---------------- I2C / motor helpers ----------------
void writeReg8(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg); Wire.write(val);
  Wire.endTransmission();
}
void writeReg32(uint8_t reg, int32_t val) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)(val));
  Wire.write((uint8_t)(val >> 8));
  Wire.write((uint8_t)(val >> 16));
  Wire.write((uint8_t)(val >> 24));
  Wire.endTransmission();
}
int32_t readReg32(uint8_t reg) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)ROLLER_I2C_ADDR, (uint8_t)4);
  int32_t v = 0;
  if (Wire.available() >= 4) {
    v  = (int32_t)Wire.read();
    v |= (int32_t)Wire.read() << 8;
    v |= (int32_t)Wire.read() << 16;
    v |= (int32_t)Wire.read() << 24;
  }
  return v;
}
void setLED(uint8_t r, uint8_t g, uint8_t b) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(REG_RGB); Wire.write(r); Wire.write(g); Wire.write(b);
  Wire.endTransmission();
}
int32_t motorPos() { return readReg32(REG_POS_READ); }
void motorSetRpm(float rpm) {
  lastCommandedRpm = rpm;
  writeReg32(REG_SPEED, (int32_t)(rpm * 100.0f));
}
void motorStop()    { motorSetRpm(0); }
void motorRelease() { writeReg8(REG_OUTPUT, 0); }
void motorEnable()  { writeReg8(REG_OUTPUT, 1); }
void motorInit() {
  writeReg8(REG_OUTPUT, 0);        delay(40);
  writeReg8(REG_STALL_PROT, 0);    delay(40);
  writeReg8(REG_MODE, MODE_SPEED); delay(40);
  writeReg32(REG_SPEED_MAXCUR, CURRENT_LIMIT); delay(40);
  writeReg32(REG_SPEED, 0);        delay(40);
  writeReg8(REG_OUTPUT, 1);
}

float posToDeg(int32_t steps) {
  int32_t mod = steps % STEPS_PER_REV;
  if (mod < 0) mod += STEPS_PER_REV;
  return (float)mod * 360.0f / (float)STEPS_PER_REV;
}
float currentAngleDeg() { return posToDeg(motorPos() - zeroSteps); }

// ---------------- Serial shell ----------------
String rxBuf;
bool   abortFlag = false;  // set when any char arrives during a long routine

// Returns true if the user typed anything since last call (consumes input).
bool checkAbort() {
  while (Serial.available()) { Serial.read(); abortFlag = true; }
  return abortFlag;
}

// Call at the start of every long routine. Drains any pending input and
// resets the abort flag. Prevents pasted / queued commands from aborting
// the thing they're being typed BEFORE.
void armAbort() {
  while (Serial.available()) Serial.read();
  abortFlag = false;
}

void printPrompt() { Serial.print("> "); }

void cmdHelp() {
  Serial.println();
  Serial.println("KINETIC MOTION LAB — commands:");
  Serial.println("  help");
  Serial.println("  info                      state, angle, last RPM");
  Serial.println("  stop                      set speed to 0 (motor still active)");
  Serial.println("  release                   disable motor output (free swing)");
  Serial.println("  enable                    re-enable motor output");
  Serial.println("  rpm <n>                   set speed directly (can be negative)");
  Serial.println("  spin <revs> <seconds>     smooth sine-velocity spin, returns to start");
  Serial.println("  deadband [start step end] for each RPM: run 1-rev worth of time,");
  Serial.println("                             release 2s, measure actual travel");
  Serial.println("  calibrate [rpm]           gravity-rest auto-zero (default 80 RPM)");
  Serial.println("  setzero <deg>             declare current arm position as <deg> in artwork frame");
  Serial.println("  go <deg>                  move to absolute angle (requires calibrate)");
  Serial.println("  breath <amp_deg> <per_s>  continuous sweep; 'stop' to end");
  Serial.println("  ritual <revs> <seconds>   one ritual gesture");
  Serial.println("  settle                    release and report final rest angle");
  Serial.println("  power                     one-shot read of all telemetry registers");
  Serial.println("  monitor                   live readout, 2 Hz; type any key to stop");
  Serial.println();
  Serial.println("  Press any key during a long routine to abort.");
}

void cmdInfo() {
  int32_t p = motorPos();
  Serial.printf("pos_raw=%d  angle_raw=%.2f°  angle_zeroed=%.2f°  zero=%s  last_rpm=%.2f  up=%lus\n",
    p, posToDeg(p), currentAngleDeg(),
    zeroCalibrated ? "YES" : "no",
    lastCommandedRpm, millis() / 1000);
}

void cmdRpm(float rpm) {
  motorEnable();
  motorSetRpm(rpm);
  Serial.printf("rpm = %.2f  (register = %ld)\n", rpm, (long)(rpm * 100.0f));
}

void cmdSpin(float revs, float seconds) {
  // sine velocity: v(t) = peak * (1-cos(2πt/T))/2.  Average = peak/2.
  //   total revs = (peak/2) * (seconds/60)  →  peak = revs*120/seconds RPM
  float peak = revs * 120.0f / seconds;
  Serial.printf("spin %.2f revs in %.2f s  →  peak %.2f RPM\n", revs, seconds, peak);
  motorEnable();
  armAbort();
  unsigned long t0 = millis();
  unsigned long dur = (unsigned long)(seconds * 1000.0f);
  while (millis() - t0 < dur) {
    if (checkAbort()) { Serial.println("aborted."); break; }
    float t = (float)(millis() - t0) / (float)dur;
    float rpm = peak * (1.0f - cosf(TWO_PI * t)) * 0.5f;
    motorSetRpm(rpm);
    delay(10);
  }
  motorStop();
  Serial.println("spin done.");
}

void cmdDeadband(float startRpm, float stepRpm, float endRpm) {
  // For each RPM: command that speed for exactly the time of one revolution
  // (60/RPM seconds), then stop, release, wait 2 s, measure actual travel.
  // This tells us how many of the commanded degrees the motor + arm actually
  // produced — the honest answer to "does it move at X RPM?".
  Serial.printf("deadband test.  start=%.1f  step=%.1f  end=%.1f  (abort: any key)\n",
    startRpm, stepRpm, endRpm);
  Serial.println("  rpm   expect_deg  actual_deg  ratio   verdict");
  Serial.println("  ----  ----------  ----------  ------  -------");
  armAbort();
  for (float r = startRpm; r <= endRpm + 0.01f; r += stepRpm) {
    if (checkAbort()) { motorStop(); motorRelease(); Serial.println("aborted."); return; }
    unsigned long dur = (unsigned long)(60000.0f / r);  // one full rev @ r RPM
    motorEnable();
    delay(30);
    int32_t p0 = motorPos();
    motorSetRpm(r);
    unsigned long t0 = millis();
    while (millis() - t0 < dur) {
      if (checkAbort()) { motorStop(); motorRelease(); Serial.println("aborted."); return; }
      delay(5);
    }
    motorStop();
    delay(50);
    motorRelease();
    delay(2000);                        // 2 s to stabilize
    int32_t p1 = motorPos();
    int32_t delta = p1 - p0;
    float actualDeg = (float)delta * 360.0f / (float)STEPS_PER_REV;
    float expectedDeg = 360.0f;
    float ratio = actualDeg / expectedDeg;
    const char* verdict =
      (fabsf(ratio) < 0.05f) ? "DEAD"   :
      (fabsf(ratio) < 0.50f) ? "weak"   :
      (fabsf(ratio) < 0.85f) ? "partial":
      (fabsf(ratio) < 1.15f) ? "OK"     : "overshoot";
    Serial.printf("  %4.1f  %10.1f  %10.1f  %6.2f  %s\n",
      r, expectedDeg, actualDeg, ratio, verdict);
  }
  motorStop();
  motorRelease();
  Serial.println("deadband done. motor released.");
}

void cmdCalibrate(float spinRpm) {
  // User's method: hard spin for 4 revs → release → settle on gravity rest → mark zero.
  const int   REVS     = 4;
  const float SPIN_SEC = (REVS * 60.0f) / spinRpm;   // at constant RPM
  const int   SETTLE_SEC = 10;  // empirically damps fully within ~7s

  Serial.printf("calibrate: %d revs at %.1f RPM (%.2f s), release, %d s settle, mark zero.\n",
    REVS, spinRpm, SPIN_SEC, SETTLE_SEC);
  Serial.println("Keep hands off the sculpture.");
  armAbort();
  motorEnable();
  motorSetRpm(spinRpm);
  unsigned long t0 = millis();
  while (millis() - t0 < (unsigned long)(SPIN_SEC * 1000)) {
    if (checkAbort()) { motorStop(); Serial.println("aborted."); return; }
    delay(20);
  }
  motorStop();
  delay(400);
  motorRelease();
  Serial.println("released. settling...");
  for (int s = SETTLE_SEC; s > 0; s--) {
    Serial.printf("  %2ds  raw=%.2f°\n", s, posToDeg(motorPos()));
    delay(1000);
    if (checkAbort()) { Serial.println("aborted during settle."); return; }
  }
  // Read zero from the encoder (works with motor disabled) and LEAVE the
  // motor released. Re-enabling would apply FOC hold torque that can
  // drift the arm away from its physical gravity rest — we saw this as
  // 30–60° drift within a couple seconds of re-enable. Instead, motion
  // commands (go/spin/breath/ritual) re-enable the motor when they need it.
  zeroSteps = motorPos();
  zeroCalibrated = true;
  Serial.printf("ZERO SET. zero_raw=%d steps (= %.2f° raw). Current angle reads 0.00°\n",
    zeroSteps, posToDeg(zeroSteps));
  Serial.println("Motor remains RELEASED. The arm is at its true gravity rest.");
  Serial.println("To declare what this position represents in artwork frame,");
  Serial.println("use  setzero <deg>  (e.g.  setzero 170).");
  setLED(0, 40, 40);
}

void cmdSetzero(float declaredDeg) {
  // Re-anchor the zero so that the CURRENT arm position now reads as the
  // given angle. Used right after `calibrate`: the gravity rest is not
  // artwork-frame 0°, it's some physical angle determined by how the
  // sculpture is mounted. Call `setzero 170` and from now on that rest
  // position reports as 170°, and `go 0` will rotate to what you consider
  // artwork-zero.
  if (!zeroCalibrated) {
    Serial.println("run 'calibrate' first.");
    return;
  }
  // Current motorPos() corresponds to what we *want* to read as declaredDeg.
  // currentAngleDeg() = posToDeg(motorPos() - zeroSteps)
  // We want this to equal declaredDeg, so:
  //   motorPos() - zeroSteps ≡ (declaredDeg / 360) * STEPS_PER_REV  (mod STEPS_PER_REV)
  //   zeroSteps_new = motorPos() - (declaredDeg / 360) * STEPS_PER_REV
  int32_t offsetSteps = (int32_t)(declaredDeg * (STEPS_PER_REV / 360.0f));
  zeroSteps = motorPos() - offsetSteps;
  Serial.printf("current arm position now reads as %.2f°. new zero_raw=%d steps.\n",
    declaredDeg, zeroSteps);
  Serial.printf("(sanity check: currentAngleDeg() = %.2f°)\n", currentAngleDeg());
}

void cmdGo(float targetDeg) {
  if (!zeroCalibrated) {
    Serial.println("no zero yet. run 'calibrate' first.");
    return;
  }
  // Always rotate in the POSITIVE direction. The RollerCAN is asymmetric.
  float curr = currentAngleDeg();
  float d = targetDeg - curr;
  while (d <    0) d += 360;
  while (d >= 360) d -= 360;
  // Trapezoidal velocity profile: ramp up to CRUISE_RPM quickly, hold,
  // ramp down. The motor needs sustained high RPM to fight gravity loads
  // (we proved sine-profile @ 30 RPM peak couldn't lift the heavy arm
  // through the top of the arc). Calibration's constant 80 RPM works
  // because it stays in the high-torque regime the whole time.
  const float CRUISE_RPM    = 80.0f;   // matches the speed proven by calibration
  const float RAMP_MS       = 200.0f;  // 0.20 s up + 0.20 s down
  // Distance covered in the two ramps (trapezoidal area = 0.5 * v * t for each):
  //   ramp_revs_each = 0.5 * CRUISE_RPM * (RAMP_MS/1000) / 60
  float rampRevs   = 0.5f * CRUISE_RPM * (RAMP_MS / 1000.0f) / 60.0f;
  float totalRevs  = d / 360.0f;
  float cruiseRevs = totalRevs - 2.0f * rampRevs;
  float cruiseMs;
  if (cruiseRevs <= 0) {
    // Distance is short — collapse to a triangle (no cruise phase).
    cruiseMs = 0;
  } else {
    cruiseMs = cruiseRevs / (CRUISE_RPM / 60.0f) * 1000.0f;
  }
  unsigned long durMs = (unsigned long)(2 * RAMP_MS + cruiseMs);
  Serial.printf("go %.2f°  (positive arc %.2f°)  trapezoid: ramp %.0fms · cruise %.0fms · total %lums @ %.0f RPM\n",
    targetDeg, d, RAMP_MS, cruiseMs, durMs, CRUISE_RPM);
  motorEnable();
  armAbort();
  unsigned long t0 = millis();
  while (millis() - t0 < durMs) {
    if (checkAbort()) { Serial.println("aborted."); break; }
    unsigned long t = millis() - t0;
    float rpm;
    if (t < RAMP_MS) {
      rpm = CRUISE_RPM * (t / RAMP_MS);              // ramp up
    } else if (t < RAMP_MS + cruiseMs) {
      rpm = CRUISE_RPM;                              // cruise
    } else {
      float td = (t - RAMP_MS - cruiseMs) / RAMP_MS;
      rpm = CRUISE_RPM * (1.0f - td);                // ramp down
    }
    motorSetRpm(rpm);
    delay(10);
  }
  // Read position immediately, then again after 1 s to see if the arm
  // drifted under gravity load when speed dropped to 0.
  float endRpmTarget = currentAngleDeg();
  motorStop();
  float justAfterStop = currentAngleDeg();
  delay(1000);
  float settled = currentAngleDeg();
  Serial.printf("commanded target reached (encoder): %.2f°\n", justAfterStop);
  Serial.printf("after 1s hold: %.2f°  (drift %.2f°)\n", settled, settled - justAfterStop);
}

void cmdBreath(float ampDeg, float periodSec) {
  Serial.printf("breath ±%.1f° period %.2fs. Type any key to stop.\n", ampDeg, periodSec);
  motorEnable();
  armAbort();
  unsigned long t0 = millis();
  float omega = TWO_PI / (periodSec * 1000.0f);       // rad/ms
  float ampSteps = ampDeg * (STEPS_PER_REV / 360.0f);
  while (!checkAbort()) {
    float t = (float)(millis() - t0);
    // v(t) = A * ω * cos(ω t) in steps/ms
    float vStepsMs = ampSteps * omega * cosf(omega * t);
    float vStepsS  = vStepsMs * 1000.0f;
    float rpm      = vStepsS * 60.0f / STEPS_PER_REV;
    motorSetRpm(rpm);
    delay(10);
  }
  motorStop();
  Serial.println("breath done.");
}

void cmdRitual(float revs, float seconds) {
  // Same math as spin but name it ritual for the log
  Serial.println("RITUAL.");
  cmdSpin(revs, seconds);
}

// ---------------- POWER / TELEMETRY ----------------
// The RollerCAN exposes a few read-only status registers. We probe a range
// of plausible addresses and print raw 32-bit and 16-bit values so we can
// identify which register reflects the change when you swap power source.
void cmdPower() {
  Serial.println("Wide register probe — looking for VIN / actual current / temp.");
  Serial.println("  reg     i32             u16     hex");
  // Common M5 RollerCAN feedback regions: 0x90 encoder, then various
  // 0xA0/B0/C0/D0 for actual speed, current, VIN, temp.
  uint8_t regs[] = {
    0x90, 0x94, 0x98, 0x9C,
    0xA0, 0xA4, 0xA8, 0xAC,
    0xB0, 0xB4, 0xB8, 0xBC,
    0xC0, 0xC4, 0xC8, 0xCC,
    0xD0, 0xD4, 0xD8, 0xDC,
    0xE0, 0xE4
  };
  for (uint8_t r : regs) {
    int32_t v32 = readReg32(r);
    uint16_t v16 = (uint16_t)(v32 & 0xFFFF);
    if (v32 != 0 && v32 != -1) {  // skip empties for readability
      Serial.printf("  0x%02X    %12ld    %5u   0x%08lX\n", r, (long)v32, v16, (long)v32);
    }
  }
}

void cmdRaw(uint8_t addr) {
  int32_t v32 = readReg32(addr);
  Serial.printf("  reg 0x%02X = %ld  (0x%08lX)\n", addr, (long)v32, (long)v32);
}

// Continuous polling of those same registers, every 500 ms. Type any key
// to stop. While running, plug/unplug the second USB-C cable and watch
// which value changes.
volatile bool monitorAlive = false;
void cmdMonitor() {
  Serial.println("LIVE TELEMETRY — type any key to stop. Plug/unplug 2nd USB-C now.");
  Serial.println("    t       0xA0       0xB0       0xC0       0xD0       0xE0   POS");
  armAbort();
  unsigned long t0 = millis();
  while (!checkAbort()) {
    int32_t a = readReg32(0xA0);
    int32_t b = readReg32(0xB0);
    int32_t c = readReg32(0xC0);
    int32_t d = readReg32(0xD0);
    int32_t e = readReg32(0xE0);
    int32_t pos = readReg32(REG_POS_READ);
    unsigned long t = (millis() - t0) / 1000;
    Serial.printf("  %3lus  %10ld  %10ld  %10ld  %10ld  %10ld   %ld\n",
      t, (long)a, (long)b, (long)c, (long)d, (long)e, (long)pos);
    delay(500);
  }
  Serial.println("monitor stopped.");
}

void cmdSettle() {
  Serial.println("settle: stop, release, wait 15 s, report.");
  armAbort();
  motorStop();
  delay(200);
  motorRelease();
  for (int s = 15; s > 0; s--) {
    Serial.printf("  %2ds  raw=%.2f°%s\n", s, posToDeg(motorPos()),
      zeroCalibrated ? "" : "  (no zero)");
    delay(1000);
    if (checkAbort()) { Serial.println("aborted."); return; }
  }
  motorEnable();
  if (zeroCalibrated) {
    Serial.printf("settled at %.2f° (zeroed). delta from zero = %.2f°\n",
      currentAngleDeg(), currentAngleDeg());
  } else {
    Serial.printf("settled at raw %.2f°.\n", posToDeg(motorPos()));
  }
}

// ---------------- Command dispatch ----------------
void handleLine(String line) {
  line.trim();
  if (!line.length()) { printPrompt(); return; }
  line.toLowerCase();

  int sp1 = line.indexOf(' ');
  String cmd = (sp1 < 0) ? line : line.substring(0, sp1);
  String rest = (sp1 < 0) ? "" : line.substring(sp1 + 1);
  rest.trim();

  if      (cmd == "help")      cmdHelp();
  else if (cmd == "info")      cmdInfo();
  else if (cmd == "stop")    { motorStop();    Serial.println("stopped."); }
  else if (cmd == "release") { motorRelease(); Serial.println("released."); }
  else if (cmd == "enable")  { motorEnable();  Serial.println("enabled."); }
  else if (cmd == "rpm")     { cmdRpm(rest.toFloat()); }
  else if (cmd == "spin") {
    int sp2 = rest.indexOf(' ');
    if (sp2 < 0) { Serial.println("usage: spin <revs> <seconds>"); }
    else cmdSpin(rest.substring(0, sp2).toFloat(), rest.substring(sp2+1).toFloat());
  }
  else if (cmd == "deadband") {
    // optional args: deadband [start [step [end]]]
    float s = 5.0f, st = 1.0f, e = 40.0f;
    if (rest.length()) {
      int sp2 = rest.indexOf(' ');
      s = rest.substring(0, sp2 < 0 ? rest.length() : sp2).toFloat();
      if (sp2 >= 0) {
        String rest2 = rest.substring(sp2 + 1); rest2.trim();
        int sp3 = rest2.indexOf(' ');
        st = rest2.substring(0, sp3 < 0 ? rest2.length() : sp3).toFloat();
        if (sp3 >= 0) e = rest2.substring(sp3 + 1).toFloat();
      }
    }
    cmdDeadband(s, st, e);
  }
  else if (cmd == "calibrate") {
    float rpm = rest.length() ? rest.toFloat() : 80.0f;
    if (rpm < 12.0f) { Serial.println("spin RPM must be ≥ 12 (below motor floor)."); }
    else cmdCalibrate(rpm);
  }
  else if (cmd == "go")        cmdGo(rest.toFloat());
  else if (cmd == "setzero")   cmdSetzero(rest.toFloat());
  else if (cmd == "breath") {
    int sp2 = rest.indexOf(' ');
    if (sp2 < 0) { Serial.println("usage: breath <amp_deg> <period_s>"); }
    else cmdBreath(rest.substring(0, sp2).toFloat(), rest.substring(sp2+1).toFloat());
  }
  else if (cmd == "ritual") {
    int sp2 = rest.indexOf(' ');
    if (sp2 < 0) { Serial.println("usage: ritual <revs> <seconds>"); }
    else cmdRitual(rest.substring(0, sp2).toFloat(), rest.substring(sp2+1).toFloat());
  }
  else if (cmd == "settle")    cmdSettle();
  else if (cmd == "power")     cmdPower();
  else if (cmd == "monitor")   cmdMonitor();
  else if (cmd == "raw") {
    // accept hex (0xNN) or decimal
    long v = (rest.startsWith("0x") || rest.startsWith("0X"))
      ? strtol(rest.c_str() + 2, nullptr, 16)
      : strtol(rest.c_str(), nullptr, 10);
    cmdRaw((uint8_t)(v & 0xFF));
  }
  else Serial.printf("? unknown: %s\n", cmd.c_str());

  printPrompt();
}

// ---------------- POSITION-HOLD TEST ----------------
// Switch motor to POSITION mode and drive to a target step count. The motor's
// internal PID drives there and HOLDS continuously. This is the only way to
// keep the arm at a non-gravity-stable angle.

void motorPositionMode() {
  writeReg8(REG_OUTPUT, 0);
  delay(40);
  writeReg8(REG_MODE, MODE_POSITION);
  delay(40);
  writeReg8(REG_OUTPUT, 1);
  delay(40);
}

void motorGotoSteps(int32_t steps) {
  writeReg32(REG_POS_TARGET, steps);
}

// ---------------- POWER TEST AUTO-SEQUENCE ----------------
// Runs once at boot. Lets us swap power sources without having to type
// commands over serial. LED colors signal phase so you can observe even
// without a monitor connected.
//
//   YELLOW   — booting / waiting
//   WHITE    — spinning at 80 RPM (calibration spin)
//   GREEN    — released, settling on gravity rest
//   CYAN     — zero captured, declaring 170°
//   BLUE     — executing  go 0  (moving the long way to artwork-zero)
//   MAGENTA  — finished, motor released, sequence done
//   RED      — motor not found / hard error
void runPowerTestSequence() {
  Serial.println();
  Serial.println(">>> SIMPLE POSITION-HOLD TEST <<<");
  Serial.println("Calibrates, then steps +90° every 5 seconds in POSITION mode.");
  Serial.println("If the motor can hold positions against gravity, this works.");
  Serial.println();

  setLED(60, 50, 0);  // yellow — boot pause
  delay(2000);

  // Step 1: calibrate (in SPEED mode) to find gravity rest.
  Serial.println("[calibrate] spinning at 80 RPM");
  setLED(50, 50, 50);  // white
  motorEnable();
  motorSetRpm(80.0f);
  delay(3000);
  motorStop();
  delay(300);
  motorRelease();
  Serial.println("[calibrate] released, settling 10 s");
  setLED(0, 50, 0);  // green
  delay(10000);
  int32_t baseSteps = motorPos();   // raw encoder pos at gravity rest
  Serial.printf("[calibrate] gravity rest at raw step %d\n", baseSteps);

  // Step 2: switch to POSITION mode, anchor at gravity rest, hold there.
  Serial.println("[position] switching to POSITION mode");
  setLED(0, 40, 50);  // cyan
  motorPositionMode();
  motorGotoSteps(baseSteps);
  delay(2000);

  // Step 3: loop forever, +90° every 5 seconds.
  Serial.println("[loop] +90° every 5 s. Watch the arm.");
  setLED(0, 0, 60);  // blue
  int32_t target = baseSteps;
  int  step = 0;
  const int32_t STEPS_PER_90DEG = STEPS_PER_REV / 4;  // 9000
  while (true) {
    delay(5000);
    target += STEPS_PER_90DEG;
    step++;
    motorGotoSteps(target);
    int32_t actual = motorPos();
    int32_t delta  = actual - baseSteps;
    Serial.printf("  step %2d → target %+d (= %+.1f°)   actual %+d (= %+.1f°)   error %+.1f°\n",
      step, target - baseSteps, (target - baseSteps) * 360.0f / STEPS_PER_REV,
      delta, delta * 360.0f / STEPS_PER_REV,
      ((target - baseSteps) - delta) * 360.0f / STEPS_PER_REV);
  }
}

// ---------------- Arduino entry points ----------------
void setup() {
  Serial.begin(115200);
  // CRITICAL: USB-CDC Serial writes block when no host is connected. If
  // the auto-test fires while disconnected from the Mac, the first print
  // hangs forever and the motor never gets commanded. Setting tx timeout
  // to 0 makes all Serial writes return immediately whether or not a
  // host is reading them.
  Serial.setTxTimeoutMs(0);
  delay(400);
  Serial.println();
  Serial.println("==========================================================");
  Serial.println("  KINETIC MOTION LAB");
  Serial.println("  Type 'help' + Enter.");
  Serial.println("==========================================================");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  bool found = false;
  for (int i = 0; i < 10; i++) {
    Wire.beginTransmission(ROLLER_I2C_ADDR);
    if (Wire.endTransmission() == 0) { found = true; break; }
    delay(300);
  }
  if (!found) {
    Serial.println("ERROR: motor not found on I2C.");
    setLED(80, 0, 0);
    while (true) delay(1000);
  }
  Serial.println("motor found.");
  motorInit();
  setLED(0, 30, 0);

  // Auto power-test on every boot. Lets us hot-swap power sources and
  // immediately see the motor's behaviour. Sequence runs once; the serial
  // shell stays available afterward.
  runPowerTestSequence();

  printPrompt();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
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
