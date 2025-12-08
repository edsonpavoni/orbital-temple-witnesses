/*
 * ============================================================================
 * ORBITAL TEMPLE - MOTION TEST
 * ============================================================================
 *
 * Simple serial-controlled motion test for developing smooth transitions.
 *
 * SERIAL COMMANDS:
 * ----------------
 * s<value>    Set target speed (RPM, can be negative)
 *             Example: s50 (50 RPM), s-30 (-30 RPM), s0 (stop)
 *
 * c<value>    Set max current/torque (mA)
 *             Example: c500 (500mA), c1000 (1000mA)
 *
 * t<value>    Set transition time (ms)
 *             Example: t2000 (2 second transitions)
 *
 * e<0-3>      Set easing mode
 *             0 = Linear
 *             1 = Ease-In-Out (smooth)
 *             2 = Ease-In (slow start)
 *             3 = Ease-Out (slow end)
 *
 * p           Print current settings
 *
 * x           Emergency stop (immediate)
 *
 * EXAMPLES:
 * ---------
 * t3000       Set 3 second transitions
 * e1          Use smooth ease-in-out
 * s200        Ramp to 200 RPM smoothly
 * s2          Slow down to 2 RPM (tracking speed)
 * s0          Stop
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8
#define I2C_SCL_PIN      9
#define I2C_FREQ         100000

// Register addresses
#define REG_OUTPUT       0x00
#define REG_MODE         0x01
#define REG_STALL_PROT   0x0F
#define REG_POS_MAXCUR   0x20   // Position mode max current
#define REG_SPEED        0x40
#define REG_SPEED_MAXCUR 0x50
#define REG_SPEED_READ   0x60
#define REG_SPEED_PID    0x70   // Speed PID: 12 bytes (P, I, D as int32)
#define REG_POS          0x80   // Position target
#define REG_POS_READ     0x90   // Position readback
#define REG_POS_PID      0xA0   // Position PID
#define REG_CURRENT      0xB0   // Direct current control
#define REG_CURRENT_READ 0xC0

#define MODE_SPEED       1
#define MODE_POSITION    2
#define MODE_CURRENT     3      // Direct current/torque mode

// =============================================================================
// EASING FUNCTIONS
// =============================================================================

enum EasingMode {
  EASE_LINEAR = 0,
  EASE_IN_OUT = 1,
  EASE_IN = 2,
  EASE_OUT = 3
};

// Easing function: takes progress 0.0-1.0, returns eased value 0.0-1.0
float applyEasing(float t, EasingMode mode) {
  switch (mode) {
    case EASE_LINEAR:
      return t;

    case EASE_IN_OUT:
      // Smooth S-curve (sine-based)
      return 0.5f * (1.0f - cos(t * PI));

    case EASE_IN:
      // Slow start, fast end (quadratic)
      return t * t;

    case EASE_OUT:
      // Fast start, slow end (quadratic)
      return 1.0f - (1.0f - t) * (1.0f - t);

    default:
      return t;
  }
}

const char* easingName(EasingMode mode) {
  switch (mode) {
    case EASE_LINEAR: return "LINEAR";
    case EASE_IN_OUT: return "EASE_IN_OUT";
    case EASE_IN: return "EASE_IN";
    case EASE_OUT: return "EASE_OUT";
    default: return "UNKNOWN";
  }
}

// =============================================================================
// GLOBAL STATE
// =============================================================================

// Motion parameters
float targetSpeedRPM = 0;
float currentSpeedRPM = 0;
float startSpeedRPM = 0;
int32_t maxCurrentMA = 500;       // Default 500mA (gentle)
uint32_t transitionTimeMS = 3000; // Default 3 seconds
EasingMode easingMode = EASE_IN_OUT;

// PID parameters (scaled by 100000 internally)
int32_t speedPID_P = 150000;  // Default P
int32_t speedPID_I = 15000;   // Default I
int32_t speedPID_D = 200000;  // Default D

// Control mode
bool usePositionMode = false;  // false = speed mode, true = position mode
float floatPosition = 0;       // Accumulated position in degrees (float for precision)
float positionIncrement = 0;   // Degrees per update cycle

// Transition state
bool inTransition = false;
unsigned long transitionStartTime = 0;

// Serial input buffer
String inputBuffer = "";

// =============================================================================
// FUNCTION PROTOTYPES
// =============================================================================

void motorInit();
void motorSetSpeed(float rpm);
void motorSetMaxCurrent(int32_t mA);
void motorEnable(bool enable);
float motorGetSpeed();
void motorSetSpeedPID(int32_t p, int32_t i, int32_t d);
void motorReadSpeedPID();
void motorSetPosition(int32_t pos);
int32_t motorGetPosition();
void switchToSpeedMode();
void switchToPositionMode();

void writeReg8(uint8_t reg, uint8_t value);
void writeReg32(uint8_t reg, int32_t value);
int32_t readReg32(uint8_t reg);
void writeReg96(uint8_t reg, int32_t v1, int32_t v2, int32_t v3);
void readReg96(uint8_t reg, int32_t* v1, int32_t* v2, int32_t* v3);

void processSerialCommand(String cmd);
void startTransition(float newTarget);
void updateMotion();
void printSettings();
void emergencyStop();

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("============================================");
  Serial.println("  ORBITAL TEMPLE - MOTION TEST");
  Serial.println("============================================");
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  s<rpm>   Set target speed (e.g., s50, s-30, s0)");
  Serial.println("  c<mA>    Set max current (e.g., c500, c1000)");
  Serial.println("  t<ms>    Set transition time (e.g., t3000)");
  Serial.println("  e<0-3>   Set easing (0=linear, 1=smooth, 2=ease-in, 3=ease-out)");
  Serial.println();
  Serial.println("  m        Toggle mode: SPEED <-> POSITION");
  Serial.println("           (Position mode = smoother at low speeds)");
  Serial.println();
  Serial.println("  kp<val>  Set PID P gain (e.g., kp100000)");
  Serial.println("  ki<val>  Set PID I gain (e.g., ki20000)");
  Serial.println("  kd<val>  Set PID D gain (e.g., kd150000)");
  Serial.println("  r        Read current PID values from motor");
  Serial.println("  p        Print settings");
  Serial.println("  x        Emergency stop");
  Serial.println();

  // Initialize I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  // Check motor connection
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: Motor not found at 0x64!");
    Serial.println("Check wiring: SDA=D9(GPIO8), SCL=D10(GPIO9)");
    while (1) delay(1000);
  }

  Serial.println("Motor connected!");
  motorInit();

  // Read current PID values from motor
  Serial.println("Reading motor PID values...");
  motorReadSpeedPID();

  printSettings();
  Serial.println();
  Serial.println("Ready. Enter commands:");
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  // Handle serial input
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

  // Update motion
  updateMotion();

  // Status output every 200ms
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    lastPrint = millis();

    Serial.print(usePositionMode ? "[POS] " : "[SPD] ");
    Serial.print("Target: ");
    Serial.print(targetSpeedRPM, 1);
    Serial.print(" | Cmd: ");
    Serial.print(currentSpeedRPM, 1);
    Serial.print(" RPM");

    if (usePositionMode) {
      Serial.print(" | Pos: ");
      Serial.print(floatPosition, 1);
      Serial.print("°");
    } else {
      float actualRPM = motorGetSpeed() / 100.0f;
      Serial.print(" | Act: ");
      Serial.print(actualRPM, 1);
    }

    if (inTransition) {
      float progress = (float)(millis() - transitionStartTime) / transitionTimeMS;
      Serial.print(" | Trans: ");
      Serial.print((int)(progress * 100));
      Serial.print("%");
    }
    Serial.println();
  }

  delay(10);  // 100Hz update rate
}

// =============================================================================
// SERIAL COMMAND PROCESSING
// =============================================================================

void processSerialCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  // Check for two-character commands first (kp, ki, kd)
  if (cmd.length() >= 2) {
    String prefix = cmd.substring(0, 2);
    prefix.toLowerCase();

    if (prefix == "kp") {
      int32_t val = cmd.substring(2).toInt();
      speedPID_P = val;
      motorSetSpeedPID(speedPID_P, speedPID_I, speedPID_D);
      Serial.print(">> PID P set to: ");
      Serial.println(speedPID_P);
      return;
    }
    if (prefix == "ki") {
      int32_t val = cmd.substring(2).toInt();
      speedPID_I = val;
      motorSetSpeedPID(speedPID_P, speedPID_I, speedPID_D);
      Serial.print(">> PID I set to: ");
      Serial.println(speedPID_I);
      return;
    }
    if (prefix == "kd") {
      int32_t val = cmd.substring(2).toInt();
      speedPID_D = val;
      motorSetSpeedPID(speedPID_P, speedPID_I, speedPID_D);
      Serial.print(">> PID D set to: ");
      Serial.println(speedPID_D);
      return;
    }
  }

  char cmdType = cmd.charAt(0);
  String value = cmd.substring(1);

  switch (cmdType) {
    case 's':
    case 'S': {
      // Set target speed
      float newSpeed = value.toFloat();
      Serial.print(">> Setting target speed: ");
      Serial.print(newSpeed);
      Serial.println(" RPM");
      startTransition(newSpeed);
      break;
    }

    case 'c':
    case 'C': {
      // Set max current
      int32_t newCurrent = value.toInt();
      if (newCurrent < 100) newCurrent = 100;
      if (newCurrent > 2000) newCurrent = 2000;
      maxCurrentMA = newCurrent;
      motorSetMaxCurrent(maxCurrentMA);
      Serial.print(">> Max current set to: ");
      Serial.print(maxCurrentMA);
      Serial.println(" mA");
      break;
    }

    case 't':
    case 'T': {
      // Set transition time
      uint32_t newTime = value.toInt();
      if (newTime < 100) newTime = 100;
      if (newTime > 30000) newTime = 30000;
      transitionTimeMS = newTime;
      Serial.print(">> Transition time set to: ");
      Serial.print(transitionTimeMS);
      Serial.println(" ms");
      break;
    }

    case 'e':
    case 'E': {
      // Set easing mode
      int mode = value.toInt();
      if (mode >= 0 && mode <= 3) {
        easingMode = (EasingMode)mode;
        Serial.print(">> Easing mode set to: ");
        Serial.println(easingName(easingMode));
      } else {
        Serial.println(">> Invalid easing mode (0-3)");
      }
      break;
    }

    case 'm':
    case 'M':
      // Toggle control mode
      if (usePositionMode) {
        switchToSpeedMode();
        Serial.println(">> Switched to SPEED MODE");
      } else {
        switchToPositionMode();
        Serial.println(">> Switched to POSITION MODE (smoother)");
      }
      break;

    case 'r':
    case 'R':
      // Read PID from motor
      motorReadSpeedPID();
      Serial.println(">> PID values read from motor");
      printSettings();
      break;

    case 'p':
    case 'P':
      printSettings();
      break;

    case 'x':
    case 'X':
      emergencyStop();
      break;

    default:
      Serial.print(">> Unknown command: ");
      Serial.println(cmd);
      break;
  }
}

// =============================================================================
// MOTION CONTROL
// =============================================================================

void startTransition(float newTarget) {
  if (newTarget == targetSpeedRPM && !inTransition) {
    return;  // Already at target
  }

  startSpeedRPM = currentSpeedRPM;
  targetSpeedRPM = newTarget;
  transitionStartTime = millis();
  inTransition = true;
}

void updateMotion() {
  if (inTransition) {
    unsigned long elapsed = millis() - transitionStartTime;

    if (elapsed >= transitionTimeMS) {
      // Transition complete
      currentSpeedRPM = targetSpeedRPM;
      inTransition = false;
    } else {
      // Calculate progress with easing
      float progress = (float)elapsed / transitionTimeMS;
      float easedProgress = applyEasing(progress, easingMode);

      // Interpolate speed
      currentSpeedRPM = startSpeedRPM + (targetSpeedRPM - startSpeedRPM) * easedProgress;
    }
  }

  if (usePositionMode) {
    // Position mode: we increment position ourselves for smooth motion
    // Convert RPM to position increment per loop iteration (10ms loop = 100Hz)
    // RPM * 360 degrees / 60 seconds / 100 Hz = degrees per iteration
    positionIncrement = currentSpeedRPM * 360.0f / 60.0f / 100.0f;

    // Accumulate position (using float for sub-degree precision)
    floatPosition += positionIncrement;

    // Convert to motor position units (assuming 1 unit = 0.01 degrees based on scaling)
    // The motor uses int32 position, scaled by 100
    int32_t targetPos = (int32_t)(floatPosition * 100.0f);

    motorSetPosition(targetPos);
  } else {
    // Speed mode: send speed command directly
    motorSetSpeed(currentSpeedRPM);
  }
}

void emergencyStop() {
  Serial.println(">> EMERGENCY STOP");
  inTransition = false;
  targetSpeedRPM = 0;
  currentSpeedRPM = 0;
  motorSetSpeed(0);
}

void printSettings() {
  Serial.println();
  Serial.println("=== Current Settings ===");
  Serial.print("Control Mode:    ");
  Serial.println(usePositionMode ? "POSITION (smooth)" : "SPEED");
  Serial.print("Target Speed:    ");
  Serial.print(targetSpeedRPM);
  Serial.println(" RPM");
  Serial.print("Max Current:     ");
  Serial.print(maxCurrentMA);
  Serial.println(" mA");
  Serial.print("Transition Time: ");
  Serial.print(transitionTimeMS);
  Serial.println(" ms");
  Serial.print("Easing Mode:     ");
  Serial.println(easingName(easingMode));
  Serial.println("--- Speed PID ---");
  Serial.print("P (kp): ");
  Serial.println(speedPID_P);
  Serial.print("I (ki): ");
  Serial.println(speedPID_I);
  Serial.print("D (kd): ");
  Serial.println(speedPID_D);
  Serial.println("========================");
}

// =============================================================================
// MOTOR FUNCTIONS
// =============================================================================

void motorInit() {
  // Disable output during setup
  writeReg8(REG_OUTPUT, 0);
  delay(50);

  // Disable stall protection
  writeReg8(REG_STALL_PROT, 0);
  delay(50);

  // Set speed mode
  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);

  // Set initial max current
  motorSetMaxCurrent(maxCurrentMA);
  delay(50);

  // Start at zero speed
  motorSetSpeed(0);
  delay(50);

  // Enable output
  writeReg8(REG_OUTPUT, 1);
  delay(50);
}

void motorSetSpeed(float rpm) {
  // Convert RPM to scaled value (multiply by 100)
  int32_t scaledSpeed = (int32_t)(rpm * 100.0f);
  writeReg32(REG_SPEED, scaledSpeed);
}

void motorSetMaxCurrent(int32_t mA) {
  // Convert mA to scaled value (multiply by 100)
  int32_t scaledCurrent = mA * 100;
  writeReg32(REG_SPEED_MAXCUR, scaledCurrent);
}

void motorEnable(bool enable) {
  writeReg8(REG_OUTPUT, enable ? 1 : 0);
}

float motorGetSpeed() {
  return (float)readReg32(REG_SPEED_READ);
}

void motorSetSpeedPID(int32_t p, int32_t i, int32_t d) {
  writeReg96(REG_SPEED_PID, p, i, d);
  Serial.print(">> PID written: P=");
  Serial.print(p);
  Serial.print(" I=");
  Serial.print(i);
  Serial.print(" D=");
  Serial.println(d);
}

void motorReadSpeedPID() {
  readReg96(REG_SPEED_PID, &speedPID_P, &speedPID_I, &speedPID_D);
  Serial.print(">> PID from motor: P=");
  Serial.print(speedPID_P);
  Serial.print(" I=");
  Serial.print(speedPID_I);
  Serial.print(" D=");
  Serial.println(speedPID_D);
}

void motorSetPosition(int32_t pos) {
  writeReg32(REG_POS, pos);
}

int32_t motorGetPosition() {
  return readReg32(REG_POS_READ);
}

void switchToSpeedMode() {
  motorEnable(false);
  delay(50);
  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);
  writeReg32(REG_SPEED_MAXCUR, maxCurrentMA * 100);
  delay(50);
  motorSetSpeed(0);
  delay(50);
  motorEnable(true);
  usePositionMode = false;
  currentSpeedRPM = 0;
  targetSpeedRPM = 0;
}

void switchToPositionMode() {
  motorEnable(false);
  delay(50);
  writeReg8(REG_MODE, MODE_POSITION);
  delay(50);
  writeReg32(REG_POS_MAXCUR, maxCurrentMA * 100);
  delay(50);
  // Read current position and use as starting point
  int32_t currentPos = motorGetPosition();
  floatPosition = currentPos / 100.0f;  // Convert to degrees
  motorSetPosition(currentPos);
  delay(50);
  motorEnable(true);
  usePositionMode = true;
  currentSpeedRPM = 0;
  targetSpeedRPM = 0;
  Serial.print(">> Starting position: ");
  Serial.print(floatPosition);
  Serial.println(" degrees");
}

// =============================================================================
// I2C FUNCTIONS
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
  // Write 3 x 32-bit values (12 bytes total)
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

void readReg96(uint8_t reg, int32_t* v1, int32_t* v2, int32_t* v3) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)ROLLER_I2C_ADDR, (uint8_t)12);

  if (Wire.available() >= 12) {
    *v1 = Wire.read();
    *v1 |= (int32_t)Wire.read() << 8;
    *v1 |= (int32_t)Wire.read() << 16;
    *v1 |= (int32_t)Wire.read() << 24;

    *v2 = Wire.read();
    *v2 |= (int32_t)Wire.read() << 8;
    *v2 |= (int32_t)Wire.read() << 16;
    *v2 |= (int32_t)Wire.read() << 24;

    *v3 = Wire.read();
    *v3 |= (int32_t)Wire.read() << 8;
    *v3 |= (int32_t)Wire.read() << 16;
    *v3 |= (int32_t)Wire.read() << 24;
  }
}
