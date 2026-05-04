/*
 * ============================================================================
 * ORBITAL TEMPLE WITNESS - SMOOTH MOTION TUNER v2.0
 * ============================================================================
 *
 * Complete test code for finding optimal settings for slow, smooth rotation
 * with a heavy, balanced pointer using direct drive (no belt reduction).
 *
 * HARDWARE:
 * - Seeed XIAO ESP32-S3
 * - M5Stack RollerCAN BLDC (I2C)
 * - 15V power via USB-C PD trigger board
 * - Direct drive (no gear reduction)
 *
 * FEATURES:
 * - Speed mode testing (ultra-slow RPM)
 * - Position stepping mode (smooth micro-steps)
 * - Current mode for torque control
 * - PID reading and tuning
 * - Calibration scan for sensorless homing
 * - Burst mode options for satellite tracking
 * - Position persistence testing
 * - Save settings to flash
 *
 * TUNING GOALS:
 * - Very slow rotation (1 revolution per 90 minutes for satellite tracking)
 * - Smooth motion with ~136g pointer
 * - No cogging or jerking
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// =============================================================================
// HARDWARE CONFIGURATION
// =============================================================================

#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8    // XIAO ESP32-S3
#define I2C_SCL_PIN      9
#define I2C_FREQ         100000

// =============================================================================
// WIFI CONFIGURATION - ACCESS POINT MODE
// =============================================================================

WebServer server(80);
Preferences prefs;

// =============================================================================
// ROLLERCAN REGISTERS - COMPLETE MAP
// =============================================================================

// Core Control (Priority 1)
#define REG_OUTPUT       0x00   // Motor enable (0=off, 1=on)
#define REG_MODE         0x01   // Mode (1=speed, 2=position, 3=current, 4=encoder)
#define REG_I2C_ADDR     0x02   // I2C address (default 0x64)
#define REG_FIRMWARE     0x04   // Firmware version
#define REG_STALL_PROT   0x0F   // Stall protection (0=disabled)
#define REG_OVER_RANGE   0x0A   // Position overflow protection
#define REG_REMOVE_PROT  0x0B   // Clear jam/stall protection

// Current Mode (Priority 4)
#define REG_CURRENT      0x20   // Current setpoint (mA)
#define REG_CURRENT_READ 0x24   // Current readback

// System Monitoring
#define REG_RGB          0x30   // RGB LED color (3 bytes: R, G, B)
#define REG_VIN          0x34   // Input voltage (mV)
#define REG_TEMP         0x38   // Internal temperature

// Speed Mode (Priority 2)
#define REG_SPEED        0x40   // Speed setpoint (RPM x 100)
#define REG_SPEED_MAXCUR 0x50   // Max current in speed mode (x100000)
#define REG_SPEED_READ   0x60   // Actual speed reading
#define REG_SPEED_PID    0x70   // Speed PID: P, I, D (12 bytes, 4 each)

// Position Mode (Priority 3)
#define REG_POS_PID      0x80   // Position PID: P, I, D (12 bytes, 4 each)
#define REG_POS_READ     0x90   // Current position (36000 = 360°)
#define REG_POS_TARGET   0xA0   // Target position
#define REG_POS_MAXCUR   0xB0   // Max current in position mode
#define REG_POS_MAXSPD   0xC0   // Max speed in position mode

// Encoder Mode (Priority 7)
#define REG_DIAL_COUNTER 0xD0   // Raw encoder counter

// Flash Storage
#define REG_SAVE_FLASH   0xF0   // Write 2 to save settings

// Mode constants
#define MODE_SPEED       1
#define MODE_POSITION    2
#define MODE_CURRENT     3
#define MODE_ENCODER     4

// =============================================================================
// TRACKING MODES
// =============================================================================

enum TrackingMode {
  TRACKING_CONTINUOUS,    // Smooth continuous motion
  TRACKING_MICRO_BURST,   // Small bursts every second
  TRACKING_FULL_BURST     // 5 rotations every minute
};

TrackingMode trackingMode = TRACKING_CONTINUOUS;
unsigned long lastBurstTime = 0;
bool burstInProgress = false;
int burstRotationsRemaining = 0;

// =============================================================================
// SMOOTH ROTATION TEST
// =============================================================================

bool smoothTestRunning = false;
int32_t smoothTestStart = 0;
int32_t smoothTestTarget = 0;
unsigned long smoothTestStartTime = 0;
unsigned long smoothTestDuration = 10000;  // 10 seconds for 360°
unsigned long lastSmoothUpdate = 0;        // Throttle updates
int smoothTestType = 0;  // 0=360°, 1=180°, 2=90°

// Easing function: ease-in-out cubic
float easeInOutCubic(float t) {
  return t < 0.5f ? 4.0f * t * t * t : 1.0f - pow(-2.0f * t + 2.0f, 3) / 2.0f;
}

// =============================================================================
// CALIBRATION STATE
// =============================================================================

bool calibrationRunning = false;
int calibrationStep = 0;
int32_t calibrationReadings[36];  // 36 readings at 10° intervals
int32_t calibrationPeakPosition = 0;
int32_t calibrationPeakCurrent = 0;
unsigned long lastCalibrationStep = 0;
String calibrationLog = "";

// =============================================================================
// POSITION PERSISTENCE TEST
// =============================================================================

int32_t lastKnownPosition = 0;
bool positionTestRequested = false;

// =============================================================================
// TUNABLE PARAMETERS
// =============================================================================

// Speed values are RPM x 100, so 100 = 1.0 RPM, 10 = 0.1 RPM
int32_t currentSpeed = 0;
int32_t speedIncrement = 5;
int32_t currentLimit = 100000;      // Max current (x100000, so 100000 = 1A)
int32_t currentIncrement = 10000;

// Position mode stepping
bool positionStepMode = false;
int32_t stepSize = 100;
unsigned long stepInterval = 100;
unsigned long lastStepTime = 0;

// Encoder resolution (M5Stack RollerCAN uses 36000 = 360°)
const int32_t STEPS_PER_REV = 36000;

// Current mode
uint8_t motorMode = MODE_SPEED;
bool motorEnabled = false;

// For position mode slow rotation
int32_t positionTarget = 0;
int32_t positionSpeed = 50;

// Current mode setpoint
int32_t currentModeSetpoint = 100;  // mA

// PID values (cached from motor)
int32_t speedPID_P = 0, speedPID_I = 0, speedPID_D = 0;
int32_t posPID_P = 0, posPID_I = 0, posPID_D = 0;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

void processCommand(char cmd);
void handleCalibrationLoop();
void handleBurstLoop();

// =============================================================================
// LED STATUS COLORS
// =============================================================================

void setLED(uint8_t r, uint8_t g, uint8_t b) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(REG_RGB);
  Wire.write(r);
  Wire.write(g);
  Wire.write(b);
  Wire.endTransmission();
}

void setLEDStatus() {
  if (calibrationRunning) {
    setLED(50, 50, 0);     // YELLOW - calibrating
  } else if (smoothTestRunning) {
    setLED(0, 50, 50);     // CYAN - smooth rotation test
  } else if (!motorEnabled) {
    setLED(0, 50, 0);      // GREEN - ready, stopped
  } else if (positionStepMode) {
    setLED(50, 0, 50);     // PURPLE - position stepping
  } else if (motorMode == MODE_CURRENT) {
    setLED(50, 25, 0);     // ORANGE - current mode
  } else if (motorMode == MODE_POSITION) {
    setLED(0, 50, 50);     // CYAN - position mode
  } else {
    setLED(0, 0, 50);      // BLUE - speed mode
  }
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

uint8_t readReg8(uint8_t reg) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)ROLLER_I2C_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
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

// Read 12 bytes for PID (3 x int32)
void readPID(uint8_t reg, int32_t* P, int32_t* I, int32_t* D) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)ROLLER_I2C_ADDR, (uint8_t)12);

  if (Wire.available() >= 12) {
    *P = Wire.read() | ((int32_t)Wire.read() << 8) |
         ((int32_t)Wire.read() << 16) | ((int32_t)Wire.read() << 24);
    *I = Wire.read() | ((int32_t)Wire.read() << 8) |
         ((int32_t)Wire.read() << 16) | ((int32_t)Wire.read() << 24);
    *D = Wire.read() | ((int32_t)Wire.read() << 8) |
         ((int32_t)Wire.read() << 16) | ((int32_t)Wire.read() << 24);
  }
}

// Write 12 bytes for PID (3 x int32)
void writePID(uint8_t reg, int32_t P, int32_t I, int32_t D) {
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  Wire.write(reg);
  // P
  Wire.write((uint8_t)(P & 0xFF));
  Wire.write((uint8_t)((P >> 8) & 0xFF));
  Wire.write((uint8_t)((P >> 16) & 0xFF));
  Wire.write((uint8_t)((P >> 24) & 0xFF));
  // I
  Wire.write((uint8_t)(I & 0xFF));
  Wire.write((uint8_t)((I >> 8) & 0xFF));
  Wire.write((uint8_t)((I >> 16) & 0xFF));
  Wire.write((uint8_t)((I >> 24) & 0xFF));
  // D
  Wire.write((uint8_t)(D & 0xFF));
  Wire.write((uint8_t)((D >> 8) & 0xFF));
  Wire.write((uint8_t)((D >> 16) & 0xFF));
  Wire.write((uint8_t)((D >> 24) & 0xFF));
  Wire.endTransmission();
}

// =============================================================================
// PID FUNCTIONS
// =============================================================================

void readSpeedPID() {
  readPID(REG_SPEED_PID, &speedPID_P, &speedPID_I, &speedPID_D);
  Serial.printf("Speed PID: P=%d I=%d D=%d\n", speedPID_P, speedPID_I, speedPID_D);
}

void writeSpeedPID(int32_t P, int32_t I, int32_t D) {
  writePID(REG_SPEED_PID, P, I, D);
  speedPID_P = P; speedPID_I = I; speedPID_D = D;
  Serial.printf("Speed PID set: P=%d I=%d D=%d\n", P, I, D);
}

void readPosPID() {
  readPID(REG_POS_PID, &posPID_P, &posPID_I, &posPID_D);
  Serial.printf("Position PID: P=%d I=%d D=%d\n", posPID_P, posPID_I, posPID_D);
}

void writePosPID(int32_t P, int32_t I, int32_t D) {
  writePID(REG_POS_PID, P, I, D);
  posPID_P = P; posPID_I = I; posPID_D = D;
  Serial.printf("Position PID set: P=%d I=%d D=%d\n", P, I, D);
}

// =============================================================================
// SYSTEM FUNCTIONS
// =============================================================================

uint8_t readFirmwareVersion() {
  return readReg8(REG_FIRMWARE);
}

void saveToFlash() {
  writeReg8(REG_SAVE_FLASH, 2);  // Magic value to trigger save
  Serial.println("Settings saved to motor flash!");
}

int32_t readCurrentFeedback() {
  int32_t raw = readReg32(REG_CURRENT_READ);
  // Sanity check - current can't be more than 10A (10000mA)
  if (raw > 10000 || raw < -10000) {
    return 0;  // Return 0 for invalid readings
  }
  return raw;
}

int32_t readDialCounter() {
  return readReg32(REG_DIAL_COUNTER);
}

void clearProtection() {
  writeReg8(REG_REMOVE_PROT, 1);
  Serial.println("Protection cleared");
}

// =============================================================================
// MOTOR CONTROL
// =============================================================================

void motorInit() {
  Serial.println("Initializing motor...");

  writeReg8(REG_OUTPUT, 0);
  delay(50);

  writeReg8(REG_STALL_PROT, 0);  // Disable stall protection for testing
  delay(50);

  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);

  writeReg32(REG_SPEED_MAXCUR, currentLimit);
  delay(50);

  writeReg32(REG_SPEED, 0);
  delay(50);

  // Read current PID values
  readSpeedPID();
  readPosPID();

  // If Position PID is zero, copy from Speed PID (which is known to work)
  if (posPID_P == 0 && posPID_I == 0 && posPID_D == 0) {
    Serial.println("Position PID is 0,0,0 - copying Speed PID values");
    writePosPID(speedPID_P, speedPID_I, speedPID_D);
  }

  Serial.println("Motor initialized (disabled)");
}

void motorEnable() {
  writeReg8(REG_OUTPUT, 1);
  motorEnabled = true;
  setLEDStatus();
  Serial.println("Motor ENABLED");
}

void motorDisable() {
  writeReg32(REG_SPEED, 0);
  writeReg8(REG_OUTPUT, 0);
  motorEnabled = false;
  currentSpeed = 0;
  burstInProgress = false;
  setLEDStatus();
  Serial.println("Motor DISABLED");
}

void motorSetSpeed(int32_t speed) {
  currentSpeed = speed;
  writeReg32(REG_SPEED, speed);
}

void motorSetMode(uint8_t mode) {
  motorMode = mode;
  writeReg8(REG_MODE, mode);
  delay(50);

  if (mode == MODE_SPEED) {
    writeReg32(REG_SPEED_MAXCUR, currentLimit);
  } else if (mode == MODE_POSITION) {
    writeReg32(REG_POS_MAXCUR, currentLimit);
    writeReg32(REG_POS_MAXSPD, positionSpeed);
  } else if (mode == MODE_CURRENT) {
    writeReg32(REG_CURRENT, currentModeSetpoint);
  }
  delay(50);

  const char* modeNames[] = {"", "SPEED", "POSITION", "CURRENT", "ENCODER"};
  Serial.printf("Mode: %s\n", modeNames[mode]);
}

void motorSetCurrentLimit(int32_t limit) {
  currentLimit = limit;
  if (motorMode == MODE_SPEED) {
    writeReg32(REG_SPEED_MAXCUR, limit);
  } else if (motorMode == MODE_POSITION) {
    writeReg32(REG_POS_MAXCUR, limit);
  }
  Serial.printf("Current limit: %.2f A\n", limit / 100000.0f);
}

void setCurrentModeSetpoint(int32_t mA) {
  currentModeSetpoint = mA;
  if (motorMode == MODE_CURRENT) {
    writeReg32(REG_CURRENT, mA);
  }
  Serial.printf("Current setpoint: %d mA\n", mA);
}

int32_t motorGetPosition() {
  return readReg32(REG_POS_READ);
}

int32_t motorGetSpeed() {
  return readReg32(REG_SPEED_READ);
}

int32_t motorGetVoltage() {
  return readReg32(REG_VIN);
}

int32_t motorGetTemperature() {
  return readReg32(REG_TEMP);
}

// Start position stepping mode for smooth slow rotation
void startPositionStepping(int direction) {
  motorSetMode(MODE_POSITION);
  positionStepMode = true;
  positionTarget = motorGetPosition();
  stepSize = direction > 0 ? abs(stepSize) : -abs(stepSize);
  motorEnable();
  setLEDStatus();
  Serial.printf("Position stepping: %d steps every %lums\n", stepSize, stepInterval);
}

void stopPositionStepping() {
  positionStepMode = false;
  motorDisable();
  setLEDStatus();
}

// =============================================================================
// SMOOTH ROTATION TEST
// =============================================================================

void startSmoothRotation(int type, unsigned long duration) {
  smoothTestType = type;
  smoothTestDuration = duration;

  int32_t degrees = (type == 0) ? 360 : (type == 1) ? 180 : 90;
  int32_t steps = (degrees * STEPS_PER_REV) / 360;

  smoothTestStart = motorGetPosition();
  smoothTestTarget = smoothTestStart + steps;
  smoothTestStartTime = millis();
  lastSmoothUpdate = 0;  // Reset throttle
  smoothTestRunning = true;

  // Use position mode - keep default max speed (don't override)
  motorSetMode(MODE_POSITION);
  // Don't override POS_MAXSPD - use default from motorSetMode
  motorEnable();

  // Set initial target to current position
  writeReg32(REG_POS_TARGET, smoothTestStart);
  delay(50);

  Serial.printf("Smooth rotation: %d° over %lu seconds\n", degrees, duration / 1000);
  Serial.printf("From %d to %d (%.1f° to %.1f°)\n",
    smoothTestStart, smoothTestTarget,
    (smoothTestStart * 360.0f) / STEPS_PER_REV,
    (smoothTestTarget * 360.0f) / STEPS_PER_REV);

  setLED(0, 50, 50);  // CYAN for smooth test
}

void handleSmoothRotationLoop() {
  if (!smoothTestRunning) return;

  unsigned long now = millis();
  unsigned long elapsed = now - smoothTestStartTime;

  if (elapsed >= smoothTestDuration) {
    // Done - snap to final position
    writeReg32(REG_POS_TARGET, smoothTestTarget);
    smoothTestRunning = false;

    float finalDeg = (smoothTestTarget * 360.0f) / STEPS_PER_REV;
    Serial.printf("Smooth rotation complete! Final target: %.1f°\n", finalDeg);
    setLEDStatus();
    return;
  }

  // Throttle updates to every 50ms (20 updates per second)
  if (now - lastSmoothUpdate < 50) return;
  lastSmoothUpdate = now;

  // Calculate progress 0.0 to 1.0
  float progress = (float)elapsed / (float)smoothTestDuration;

  // Apply easing
  float easedProgress = easeInOutCubic(progress);

  // Calculate current target position
  int32_t currentTarget = smoothTestStart + (int32_t)((smoothTestTarget - smoothTestStart) * easedProgress);

  // Update global positionTarget for consistency
  positionTarget = currentTarget;

  // Send to motor
  writeReg32(REG_POS_TARGET, currentTarget);

  // Debug output every 500ms
  static unsigned long lastDebug = 0;
  if (now - lastDebug >= 500) {
    lastDebug = now;
    int32_t actualPos = motorGetPosition();
    float targetDeg = (currentTarget * 360.0f) / STEPS_PER_REV;
    float actualDeg = (actualPos * 360.0f) / STEPS_PER_REV;
    Serial.printf("[Smooth] %.0f%% | Target: %.1f° | Actual: %.1f° | Diff: %.1f°\n",
      progress * 100, targetDeg, actualDeg, targetDeg - actualDeg);
  }
}

void stopSmoothRotation() {
  smoothTestRunning = false;
  motorDisable();
  setLEDStatus();
}

// =============================================================================
// CALIBRATION SCAN (Sensorless Homing)
// =============================================================================

void startCalibrationScan() {
  Serial.println("\n========================================");
  Serial.println("  CALIBRATION SCAN - Torque Signature");
  Serial.println("========================================");
  Serial.println("Rotating 360° in current mode...");
  Serial.println("Logging current draw every 10°");
  Serial.println("========================================\n");

  calibrationRunning = true;
  calibrationStep = 0;
  calibrationPeakCurrent = 0;
  calibrationPeakPosition = 0;
  calibrationLog = "";

  // Switch to current mode with low current
  motorSetMode(MODE_CURRENT);
  setCurrentModeSetpoint(150);  // 150mA - gentle torque
  delay(100);

  // Get starting position
  positionTarget = motorGetPosition();

  motorEnable();
  setLEDStatus();
  lastCalibrationStep = millis();
}

void handleCalibrationLoop() {
  if (!calibrationRunning) return;

  // Move 10° every 500ms
  if (millis() - lastCalibrationStep >= 500) {
    lastCalibrationStep = millis();

    if (calibrationStep < 36) {
      // Read current before moving
      int32_t current = readCurrentFeedback();
      int32_t pos = motorGetPosition();
      float degrees = (pos * 360.0f) / STEPS_PER_REV;

      calibrationReadings[calibrationStep] = current;

      // Track peak
      if (current > calibrationPeakCurrent) {
        calibrationPeakCurrent = current;
        calibrationPeakPosition = pos;
      }

      String entry = String(calibrationStep * 10) + "°: " + String(current) + "mA";
      calibrationLog += entry + "\n";
      Serial.printf("  [%2d] %3d°: %4d mA  (pos: %d)\n",
                    calibrationStep, calibrationStep * 10, current, pos);

      // Move 10° (1000 steps in 36000/rev system)
      positionTarget += 1000;
      writeReg32(REG_POS_TARGET, positionTarget);

      calibrationStep++;
    } else {
      // Calibration complete
      calibrationRunning = false;
      motorDisable();

      float peakDegrees = (calibrationPeakPosition * 360.0f) / STEPS_PER_REV;

      Serial.println("\n========================================");
      Serial.println("  CALIBRATION COMPLETE");
      Serial.println("========================================");
      Serial.printf("Peak current: %d mA at %.1f°\n", calibrationPeakCurrent, peakDegrees);
      Serial.println("This is the 'heavy side up' position.");
      Serial.println("========================================\n");

      // Save to ESP32 preferences
      prefs.begin("witness", false);
      prefs.putInt("homePeak", calibrationPeakCurrent);
      prefs.putInt("homePos", calibrationPeakPosition);
      prefs.end();

      Serial.println("Home position saved to ESP32 flash.");

      setLEDStatus();
    }
  }
}

// =============================================================================
// BURST MODE HANDLING
// =============================================================================

void setTrackingMode(TrackingMode mode) {
  trackingMode = mode;
  burstInProgress = false;
  lastBurstTime = millis();

  const char* modeNames[] = {"CONTINUOUS", "MICRO_BURST", "FULL_BURST"};
  Serial.printf("Tracking mode: %s\n", modeNames[mode]);
}

void handleBurstLoop() {
  if (!motorEnabled || trackingMode == TRACKING_CONTINUOUS) return;

  unsigned long now = millis();

  if (trackingMode == TRACKING_MICRO_BURST) {
    // Every 1 second: move 6° quickly then stop
    if (now - lastBurstTime >= 1000) {
      lastBurstTime = now;

      if (!burstInProgress) {
        // Start burst: move 6° (600 steps at 36000/rev)
        burstInProgress = true;
        positionTarget = motorGetPosition() + 600;
        writeReg32(REG_POS_TARGET, positionTarget);
        writeReg32(REG_POS_MAXSPD, 1000);  // Fast
      } else {
        burstInProgress = false;
      }
    }
  } else if (trackingMode == TRACKING_FULL_BURST) {
    // Every 60 seconds: rotate 5 revolutions quickly
    if (now - lastBurstTime >= 60000 && !burstInProgress) {
      lastBurstTime = now;
      burstInProgress = true;
      burstRotationsRemaining = 5;

      // Switch to speed mode for smooth rotation
      motorSetMode(MODE_SPEED);
      motorSetSpeed(500);  // 5 RPM
      Serial.println("BURST: 5 rotations at 5 RPM");
    }

    if (burstInProgress && burstRotationsRemaining > 0) {
      // Check if we've completed rotations (simple time-based)
      if (now - lastBurstTime >= 60000) {  // 5 rev at 5 RPM = 60 sec
        motorSetSpeed(0);
        burstInProgress = false;
        burstRotationsRemaining = 0;
        Serial.println("BURST complete, holding position");
      }
    }
  }
}

// =============================================================================
// DISPLAY STATUS
// =============================================================================

void showStatus() {
  int32_t pos = motorGetPosition();
  int32_t actualSpeed = motorGetSpeed();
  int32_t voltage = motorGetVoltage();
  int32_t temp = motorGetTemperature();
  int32_t current = readCurrentFeedback();
  uint8_t firmware = readFirmwareVersion();
  float degrees = (pos * 360.0f) / STEPS_PER_REV;

  Serial.println("\n============================================");
  Serial.println("  SMOOTH MOTION TUNER v2.0 - STATUS");
  Serial.println("============================================");
  Serial.printf("Firmware:     v%d\n", firmware);
  Serial.printf("Motor:        %s\n", motorEnabled ? "ENABLED" : "DISABLED");
  Serial.printf("Mode:         %s%s\n",
    motorMode == MODE_SPEED ? "SPEED" :
    motorMode == MODE_POSITION ? "POSITION" :
    motorMode == MODE_CURRENT ? "CURRENT" : "ENCODER",
    positionStepMode ? " (stepping)" : "");
  Serial.printf("Tracking:     %s\n",
    trackingMode == TRACKING_CONTINUOUS ? "CONTINUOUS" :
    trackingMode == TRACKING_MICRO_BURST ? "MICRO_BURST" : "FULL_BURST");
  Serial.println("--------------------------------------------");
  Serial.printf("Voltage:      %.2f V\n", voltage / 100.0f);
  Serial.printf("Temperature:  %d C\n", temp);
  Serial.printf("Current:      %d mA\n", current);
  Serial.println("--------------------------------------------");
  Serial.printf("Speed set:    %d (%.2f RPM)\n", currentSpeed, currentSpeed / 100.0f);
  Serial.printf("Speed actual: %d (%.2f RPM)\n", actualSpeed, actualSpeed / 100.0f);
  Serial.printf("Current lim:  %d (%.2f A)\n", currentLimit, currentLimit / 100000.0f);
  Serial.println("--------------------------------------------");
  Serial.printf("Position:     %d steps (%.1f deg)\n", pos, degrees);
  Serial.printf("Step size:    %d steps\n", stepSize);
  Serial.printf("Step rate:    %lu ms\n", stepInterval);
  Serial.println("--------------------------------------------");
  Serial.printf("Speed PID:    P=%d I=%d D=%d\n", speedPID_P, speedPID_I, speedPID_D);
  Serial.printf("Pos PID:      P=%d I=%d D=%d\n", posPID_P, posPID_I, posPID_D);
  Serial.println("============================================");
  Serial.println("COMMANDS:");
  Serial.println("  s - Start (speed mode)");
  Serial.println("  p - Start POSITION STEPPING");
  Serial.println("  t - Start CURRENT/TORQUE mode");
  Serial.println("  r - Reverse direction");
  Serial.println("  x - Stop motor");
  Serial.println("  + - Increase speed  | - - Decrease speed");
  Serial.println("  c - Inc current lim | v - Dec current lim");
  Serial.println("  [ - Faster steps    | ] - Slower steps");
  Serial.println("  < - Smaller steps   | > - Larger steps");
  Serial.println("  1-5 - Preset speeds");
  Serial.println("  6 - Satellite speed (0.0067 RPM)");
  Serial.println("--------------------------------------------");
  Serial.println("  SMOOTH ROTATION (ease-in-out):");
  Serial.println("  g - 360° in 10 seconds");
  Serial.println("  h - 180° in 5 seconds");
  Serial.println("  j - 90° in 3 seconds");
  Serial.println("  l - 360° in 30 seconds (slow)");
  Serial.println("  o - 360° in 60 seconds (very slow)");
  Serial.println("--------------------------------------------");
  Serial.println("  k - Run CALIBRATION scan");
  Serial.println("  f - Save to FLASH");
  Serial.println("  i - Read PID values");
  Serial.println("  ? - Show this status");
  Serial.println("============================================\n");
}

// =============================================================================
// PRESET SPEEDS
// =============================================================================

void applyPreset(int preset) {
  switch (preset) {
    case 1:
      currentSpeed = 5;      // 0.05 RPM
      Serial.println("Preset 1: 0.05 RPM (20 min/rev)");
      break;
    case 2:
      currentSpeed = 10;     // 0.1 RPM
      Serial.println("Preset 2: 0.1 RPM (10 min/rev)");
      break;
    case 3:
      currentSpeed = 25;     // 0.25 RPM
      Serial.println("Preset 3: 0.25 RPM (4 min/rev)");
      break;
    case 4:
      currentSpeed = 50;     // 0.5 RPM
      Serial.println("Preset 4: 0.5 RPM (2 min/rev)");
      break;
    case 5:
      currentSpeed = 100;    // 1.0 RPM
      Serial.println("Preset 5: 1.0 RPM (1 min/rev)");
      break;
    case 6:
      // Satellite tracking: 1 rev / 90 min = 0.0111 RPM
      // RPM x 100 = 1.11, round to 1
      currentSpeed = 1;
      Serial.println("Preset 6: 0.01 RPM (SATELLITE - 100 min/rev)");
      break;
  }
  motorSetSpeed(currentSpeed);
}

// =============================================================================
// WEB INTERFACE
// =============================================================================

String getWebPage() {
  String html = R"(<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>Witness Tuner v2</title>
<style>
body{font-family:Arial;background:#1a1a2e;color:#eee;padding:15px;max-width:500px;margin:0 auto}
.card{background:#16213e;padding:12px;border-radius:10px;margin:8px 0}
.value{font-size:20px;font-weight:bold;color:#e94560}
.small{font-size:12px;color:#888}
button{background:#e94560;color:#fff;border:none;padding:12px 15px;
font-size:14px;border-radius:5px;margin:3px;cursor:pointer;min-width:55px}
button:active{background:#c73e54}
.grn{background:#4CAF50}
.blu{background:#2196F3}
.prp{background:#9C27B0}
.org{background:#FF9800}
.gry{background:#607D8B}
.row{display:flex;justify-content:center;flex-wrap:wrap}
h1{color:#e94560;text-align:center;font-size:22px;margin:10px 0}
h3{margin:5px 0;color:#e94560}
.status-row{display:flex;justify-content:space-between;margin:3px 0}
</style>
<script>
function cmd(c){fetch('/cmd?c='+c).then(()=>updateStatus());}
function setVal(param,val){fetch('/set?'+param+'='+val).then(()=>updateStatus());}
function updateStatus(){
  fetch('/status').then(r=>r.json()).then(d=>{
    document.getElementById('motor').textContent=d.enabled?'ON':'OFF';
    document.getElementById('mode').textContent=d.mode+(d.stepping?' (step)':'');
    document.getElementById('track').textContent=d.track;
    document.getElementById('voltage').textContent=d.voltage.toFixed(1)+' V';
    document.getElementById('temp').textContent=d.temp+' C';
    document.getElementById('current').textContent=d.current+' mA';
    document.getElementById('speedSet').textContent=d.speedSet.toFixed(2)+' RPM';
    document.getElementById('speedAct').textContent=d.speedAct.toFixed(2)+' RPM';
    document.getElementById('pos').textContent=d.pos.toFixed(1)+' deg';
    document.getElementById('curLim').textContent=d.curLim.toFixed(2)+' A';
    document.getElementById('stepInfo').textContent=d.stepInt+'ms, '+d.stepSize+' steps';
    document.getElementById('pidSpd').textContent='P='+d.spdP+' I='+d.spdI+' D='+d.spdD;
    document.getElementById('pidPos').textContent='P='+d.posP+' I='+d.posI+' D='+d.posD;
    if(d.calRunning){document.getElementById('calStatus').style.display='block';
      document.getElementById('calStep').textContent=d.calStep;}
    else{document.getElementById('calStatus').style.display='none';}
  });
}
setInterval(updateStatus,2000);
updateStatus();
</script></head><body>
)";

  html += "<h1>Witness Tuner v2.0</h1>";

  // Status card
  html += "<div class='card'>";
  html += "<div class='status-row'>Motor: <span class='value' id='motor'>--</span></div>";
  html += "<div class='status-row'>Mode: <span class='value' id='mode'>--</span></div>";
  html += "<div class='status-row'>Tracking: <span class='value' id='track'>--</span></div>";
  html += "<div class='status-row'>Voltage: <span class='value' id='voltage'>--</span></div>";
  html += "<div class='status-row'>Temp: <span class='value' id='temp'>--</span></div>";
  html += "<div class='status-row'>Current: <span class='value' id='current'>--</span></div>";
  html += "</div>";

  // Speed/Position card
  html += "<div class='card'>";
  html += "<div class='status-row'>Speed Set: <span class='value' id='speedSet'>--</span></div>";
  html += "<div class='status-row'>Actual: <span class='value' id='speedAct'>--</span></div>";
  html += "<div class='status-row'>Position: <span class='value' id='pos'>--</span></div>";
  html += "<div class='status-row'>Curr Limit: <span class='value' id='curLim'>--</span></div>";
  html += "</div>";

  // Main controls
  html += "<div class='card'><h3>Motor Control</h3><div class='row'>";
  html += "<button onclick=\"cmd('s')\" class='blu'>SPEED</button>";
  html += "<button onclick=\"cmd('p')\" class='grn'>STEP</button>";
  html += "<button onclick=\"cmd('t')\" class='org'>TORQUE</button>";
  html += "<button onclick=\"cmd('x')\">STOP</button>";
  html += "<button onclick=\"cmd('r')\" class='gry'>REV</button>";
  html += "</div></div>";

  // Speed presets
  html += "<div class='card'><h3>Speed (RPM)</h3><div class='row'>";
  html += "<button onclick=\"cmd('-')\">-</button>";
  html += "<button onclick=\"cmd('+')\">+</button>";
  html += "</div><div class='row'>";
  html += "<button onclick=\"cmd('1')\" class='gry'>0.05</button>";
  html += "<button onclick=\"cmd('2')\" class='gry'>0.1</button>";
  html += "<button onclick=\"cmd('3')\" class='gry'>0.25</button>";
  html += "<button onclick=\"cmd('4')\" class='gry'>0.5</button>";
  html += "<button onclick=\"cmd('5')\" class='gry'>1.0</button>";
  html += "<button onclick=\"cmd('6')\" class='prp'>SAT</button>";
  html += "</div></div>";

  // Current limit
  html += "<div class='card'><h3>Current Limit</h3><div class='row'>";
  html += "<button onclick=\"cmd('v')\">-0.1A</button>";
  html += "<button onclick=\"cmd('c')\">+0.1A</button>";
  html += "</div></div>";

  // Step interval
  html += "<div class='card'><h3>Step Mode (<span id='stepInfo'>--</span>)</h3><div class='row'>";
  html += "<button onclick=\"cmd('[')\" class='prp'>FASTER</button>";
  html += "<button onclick=\"cmd(']')\" class='prp'>SLOWER</button>";
  html += "<button onclick=\"cmd('<')\" class='prp'>SMALLER</button>";
  html += "<button onclick=\"cmd('>')\" class='prp'>LARGER</button>";
  html += "</div></div>";

  // Smooth Rotation Tests
  html += "<div class='card'><h3>Smooth Rotation Test</h3>";
  html += "<div class='small' style='margin-bottom:8px'>Ease-in-out cubic interpolation</div>";
  html += "<div class='row'>";
  html += "<button onclick=\"cmd('j')\" class='blu'>90° 3s</button>";
  html += "<button onclick=\"cmd('h')\" class='blu'>180° 5s</button>";
  html += "<button onclick=\"cmd('g')\" class='grn'>360° 10s</button>";
  html += "</div><div class='row'>";
  html += "<button onclick=\"cmd('l')\" class='org'>360° 30s</button>";
  html += "<button onclick=\"cmd('o')\" class='prp'>360° 60s</button>";
  html += "</div></div>";

  // Tracking modes
  html += "<div class='card'><h3>Tracking Mode</h3><div class='row'>";
  html += "<button onclick=\"setVal('track','0')\" class='blu'>CONT</button>";
  html += "<button onclick=\"setVal('track','1')\" class='org'>uBURST</button>";
  html += "<button onclick=\"setVal('track','2')\" class='prp'>BURST</button>";
  html += "</div></div>";

  // Advanced
  html += "<div class='card'><h3>Advanced</h3><div class='row'>";
  html += "<button onclick=\"cmd('k')\" class='org'>CALIBRATE</button>";
  html += "<button onclick=\"cmd('f')\" class='grn'>SAVE FLASH</button>";
  html += "<button onclick=\"cmd('i')\" class='gry'>READ PID</button>";
  html += "</div>";

  // PID display
  html += "<div class='small' style='margin-top:10px'>";
  html += "Speed PID: <span id='pidSpd'>--</span><br>";
  html += "Pos PID: <span id='pidPos'>--</span>";
  html += "</div></div>";

  // Calibration status
  html += "<div class='card' id='calStatus' style='background:#3d2914;display:none'>";
  html += "<h3>CALIBRATING...</h3>";
  html += "<div>Step <span id='calStep'>0</span>/36</div>";
  html += "</div>";

  html += "</body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", getWebPage());
}

void handleCommand() {
  if (server.hasArg("c")) {
    String cmd = server.arg("c");
    if (cmd.length() > 0) {
      processCommand(cmd.charAt(0));
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleSet() {
  if (server.hasArg("track")) {
    int mode = server.arg("track").toInt();
    setTrackingMode((TrackingMode)mode);
  }
  if (server.hasArg("spd_p")) {
    speedPID_P = server.arg("spd_p").toInt();
    writeSpeedPID(speedPID_P, speedPID_I, speedPID_D);
  }
  if (server.hasArg("spd_i")) {
    speedPID_I = server.arg("spd_i").toInt();
    writeSpeedPID(speedPID_P, speedPID_I, speedPID_D);
  }
  if (server.hasArg("spd_d")) {
    speedPID_D = server.arg("spd_d").toInt();
    writeSpeedPID(speedPID_P, speedPID_I, speedPID_D);
  }
  server.send(200, "text/plain", "OK");
}

void handleCalibrationData() {
  String json = "{\"running\":" + String(calibrationRunning ? "true" : "false");
  json += ",\"step\":" + String(calibrationStep);
  json += ",\"peak\":" + String(calibrationPeakCurrent);
  json += ",\"peakPos\":" + String(calibrationPeakPosition);
  json += ",\"readings\":[";
  for (int i = 0; i < 36 && i < calibrationStep; i++) {
    if (i > 0) json += ",";
    json += String(calibrationReadings[i]);
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleStatus() {
  int32_t pos = motorGetPosition();
  int32_t actualSpeed = motorGetSpeed();
  int32_t voltage = motorGetVoltage();
  int32_t current = readCurrentFeedback();
  int32_t temp = motorGetTemperature();
  float degrees = (pos * 360.0f) / STEPS_PER_REV;

  String modeStr = (motorMode == MODE_SPEED ? "Speed" : motorMode == MODE_POSITION ? "Position" : "Current");
  String trackStr = (trackingMode == TRACKING_CONTINUOUS ? "Cont" : trackingMode == TRACKING_MICRO_BURST ? "uBurst" : "Burst");

  String json = "{";
  json += "\"enabled\":" + String(motorEnabled ? "true" : "false");
  json += ",\"mode\":\"" + modeStr + "\"";
  json += ",\"stepping\":" + String(positionStepMode ? "true" : "false");
  json += ",\"track\":\"" + trackStr + "\"";
  json += ",\"voltage\":" + String(voltage / 100.0f, 2);
  json += ",\"temp\":" + String(temp);
  json += ",\"current\":" + String(current);
  json += ",\"speedSet\":" + String(currentSpeed / 100.0f, 3);
  json += ",\"speedAct\":" + String(actualSpeed / 100.0f, 3);
  json += ",\"pos\":" + String(degrees, 1);
  json += ",\"curLim\":" + String(currentLimit / 100000.0f, 2);
  json += ",\"stepInt\":" + String(stepInterval);
  json += ",\"stepSize\":" + String(stepSize);
  json += ",\"spdP\":" + String(speedPID_P);
  json += ",\"spdI\":" + String(speedPID_I);
  json += ",\"spdD\":" + String(speedPID_D);
  json += ",\"posP\":" + String(posPID_P);
  json += ",\"posI\":" + String(posPID_I);
  json += ",\"posD\":" + String(posPID_D);
  json += ",\"calRunning\":" + String(calibrationRunning ? "true" : "false");
  json += ",\"calStep\":" + String(calibrationStep);
  json += ",\"smoothRunning\":" + String(smoothTestRunning ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

// =============================================================================
// COMMAND PROCESSING
// =============================================================================

void processCommand(char cmd) {
  switch (cmd) {
    case 's':
    case 'S':
      positionStepMode = false;
      motorSetMode(MODE_SPEED);
      if (!motorEnabled) motorEnable();
      if (currentSpeed == 0) currentSpeed = 25;
      motorSetSpeed(abs(currentSpeed));
      Serial.printf("Starting SPEED mode: %d (%.2f RPM)\n", currentSpeed, currentSpeed / 100.0f);
      break;

    case 'p':
    case 'P':
      startPositionStepping(1);
      break;

    case 't':
    case 'T':
      positionStepMode = false;
      motorSetMode(MODE_CURRENT);
      if (!motorEnabled) motorEnable();
      Serial.printf("Starting CURRENT mode: %d mA\n", currentModeSetpoint);
      break;

    case 'r':
    case 'R':
      if (positionStepMode) {
        stepSize = -stepSize;
        Serial.printf("Reversed stepping: %d steps\n", stepSize);
      } else if (motorMode == MODE_CURRENT) {
        currentModeSetpoint = -currentModeSetpoint;
        setCurrentModeSetpoint(currentModeSetpoint);
      } else {
        currentSpeed = -currentSpeed;
        motorSetSpeed(currentSpeed);
        Serial.printf("Reversed: %d (%.2f RPM)\n", currentSpeed, currentSpeed / 100.0f);
      }
      break;

    case 'x':
    case 'X':
      positionStepMode = false;
      calibrationRunning = false;
      smoothTestRunning = false;
      motorDisable();
      break;

    case '+':
    case '=':
      currentSpeed += speedIncrement;
      motorSetSpeed(currentSpeed);
      Serial.printf("Speed: %d (%.2f RPM)\n", currentSpeed, currentSpeed / 100.0f);
      break;

    case '-':
    case '_':
      currentSpeed -= speedIncrement;
      if (currentSpeed < 0) currentSpeed = 0;
      motorSetSpeed(currentSpeed);
      Serial.printf("Speed: %d (%.2f RPM)\n", currentSpeed, currentSpeed / 100.0f);
      break;

    case 'c':
    case 'C':
      currentLimit += currentIncrement;
      if (currentLimit > 500000) currentLimit = 500000;
      motorSetCurrentLimit(currentLimit);
      break;

    case 'v':
    case 'V':
      currentLimit -= currentIncrement;
      if (currentLimit < 10000) currentLimit = 10000;
      motorSetCurrentLimit(currentLimit);
      break;

    case 'm':
    case 'M':
      if (motorMode == MODE_SPEED) motorSetMode(MODE_POSITION);
      else if (motorMode == MODE_POSITION) motorSetMode(MODE_CURRENT);
      else motorSetMode(MODE_SPEED);
      break;

    case '1': case '2': case '3': case '4': case '5': case '6':
      applyPreset(cmd - '0');
      break;

    case '?':
      showStatus();
      break;

    case '[':
      if (stepInterval > 10) stepInterval -= 10;
      Serial.printf("Step interval: %lu ms\n", stepInterval);
      break;

    case ']':
      if (stepInterval < 2000) stepInterval += 10;
      Serial.printf("Step interval: %lu ms\n", stepInterval);
      break;

    case '<':
      if (abs(stepSize) > 10) stepSize = stepSize > 0 ? stepSize - 10 : stepSize + 10;
      Serial.printf("Step size: %d steps\n", stepSize);
      break;

    case '>':
      stepSize = stepSize > 0 ? stepSize + 10 : stepSize - 10;
      Serial.printf("Step size: %d steps\n", stepSize);
      break;

    case 'k':
    case 'K':
      startCalibrationScan();
      break;

    case 'f':
    case 'F':
      saveToFlash();
      break;

    case 'g':
    case 'G':
      // Smooth 360° rotation - 10 seconds
      startSmoothRotation(0, 10000);
      break;

    case 'h':
    case 'H':
      // Smooth 180° rotation - 5 seconds
      startSmoothRotation(1, 5000);
      break;

    case 'j':
    case 'J':
      // Smooth 90° rotation - 3 seconds
      startSmoothRotation(2, 3000);
      break;

    case 'l':
    case 'L':
      // Slow smooth 360° rotation - 30 seconds
      startSmoothRotation(0, 30000);
      break;

    case 'o':
    case 'O':
      // Very slow smooth 360° rotation - 60 seconds
      startSmoothRotation(0, 60000);
      break;

    case 'i':
    case 'I':
      readSpeedPID();
      readPosPID();
      break;

    case 'z':
    case 'Z':
      // Simple position test - move 90° and stop
      {
        Serial.println("Simple position test starting...");

        // Clear any protection first
        writeReg8(REG_REMOVE_PROT, 1);
        delay(50);

        // Read current position
        int32_t currentPos = motorGetPosition();
        int32_t target = currentPos + 9000;  // 90° = 9000 steps

        Serial.printf("Current pos: %d (%.1f°)\n", currentPos, (currentPos * 360.0f) / STEPS_PER_REV);
        Serial.printf("Target pos:  %d (%.1f°)\n", target, (target * 360.0f) / STEPS_PER_REV);

        // Set position mode
        writeReg8(REG_MODE, MODE_POSITION);
        delay(50);

        // Set max current and speed for position mode
        writeReg32(REG_POS_MAXCUR, currentLimit);
        delay(50);
        writeReg32(REG_POS_MAXSPD, 100);  // Slow speed
        delay(50);

        // Set target BEFORE enabling
        writeReg32(REG_POS_TARGET, currentPos);  // Start at current
        delay(50);

        // Enable motor
        writeReg8(REG_OUTPUT, 1);
        motorEnabled = true;
        motorMode = MODE_POSITION;
        delay(100);

        // Now set the actual target
        Serial.println("Setting target...");
        writeReg32(REG_POS_TARGET, target);

        Serial.println("Motor should be moving now. Press x to stop.");
      }
      break;
  }
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n");
  Serial.println("============================================");
  Serial.println("  ORBITAL TEMPLE WITNESS");
  Serial.println("  SMOOTH MOTION TUNER v2.0");
  Serial.println("============================================");
  Serial.println("  15V Power | Direct Drive | ~136g Pointer");
  Serial.println("============================================\n");

  // Initialize preferences
  prefs.begin("witness", true);
  lastKnownPosition = prefs.getInt("lastPos", 0);
  prefs.end();

  // Initialize I2C
  Serial.print("I2C init... ");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);
  delay(100);

  // Check motor connection
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.println("OK");
    setLED(50, 0, 0);  // RED - initializing
  } else {
    Serial.println("FAILED - Motor not found!");
    while(1) {
      setLED(50, 0, 0);
      delay(500);
      setLED(0, 0, 0);
      delay(500);
    }
  }

  // Initialize motor
  motorInit();

  // Check position persistence
  int32_t currentPos = motorGetPosition();
  Serial.printf("Stored position: %d\n", lastKnownPosition);
  Serial.printf("Current position: %d\n", currentPos);
  if (lastKnownPosition != 0 && abs(currentPos - lastKnownPosition) < 100) {
    Serial.println("Position PERSISTED across power cycle!");
  } else if (lastKnownPosition != 0) {
    Serial.println("Position CHANGED - may need recalibration");
  }

  setLED(50, 50, 0);  // YELLOW - WiFi starting

  // Start AP mode
  Serial.println("\nStarting WiFi Access Point...");
  Serial.flush();
  WiFi.mode(WIFI_AP);
  Serial.println("WiFi mode set to AP");
  Serial.flush();

  bool apStarted = WiFi.softAP("WitnessTuner", "witness123");
  Serial.printf("WiFi.softAP returned: %s\n", apStarted ? "true" : "false");
  Serial.flush();
  delay(500);  // Give AP more time to start

  Serial.println("============================================");
  Serial.println("  WiFi: WitnessTuner");
  Serial.println("  Password: witness123");
  Serial.printf("  URL: http://%s\n", WiFi.softAPIP().toString().c_str());
  Serial.println("============================================");

  // Setup web server
  server.on("/", handleRoot);
  server.on("/cmd", handleCommand);
  server.on("/set", handleSet);
  server.on("/status", handleStatus);
  server.on("/calibration", handleCalibrationData);
  server.begin();

  setLED(0, 50, 0);  // GREEN - ready!
  Serial.println("\nReady! Use Serial or Web interface.");
  Serial.println("LED: GREEN=ready, BLUE=speed, PURPLE=step, ORANGE=current");
  showStatus();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

unsigned long lastStatusPrint = 0;
unsigned long lastPositionSave = 0;

void loop() {
  server.handleClient();

  if (Serial.available()) {
    char cmd = Serial.read();
    processCommand(cmd);
  }

  // Handle calibration scan
  handleCalibrationLoop();

  // Handle smooth rotation test
  handleSmoothRotationLoop();

  // Handle burst mode
  handleBurstLoop();

  // Position stepping mode
  if (positionStepMode && motorEnabled && !calibrationRunning) {
    if (millis() - lastStepTime >= stepInterval) {
      lastStepTime = millis();
      positionTarget += stepSize;
      writeReg32(REG_POS_TARGET, positionTarget);
    }
  }

  // Save position to ESP32 flash every 30 seconds when motor is running
  if (motorEnabled && (millis() - lastPositionSave > 30000)) {
    lastPositionSave = millis();
    int32_t pos = motorGetPosition();
    prefs.begin("witness", false);
    prefs.putInt("lastPos", pos);
    prefs.end();
  }

  // Print status every 5 seconds when running
  if (motorEnabled && (millis() - lastStatusPrint > 5000)) {
    lastStatusPrint = millis();
    int32_t pos = motorGetPosition();
    int32_t actualSpeed = motorGetSpeed();
    int32_t voltage = motorGetVoltage();
    int32_t current = readCurrentFeedback();
    float degrees = (pos * 360.0f) / STEPS_PER_REV;

    if (calibrationRunning) {
      Serial.printf("[Calibrating] Step %d/36 | Pos: %.1f deg\n", calibrationStep, degrees);
    } else if (positionStepMode) {
      Serial.printf("[Stepping] Pos: %.1f deg | V: %.1fV | Step: %d @ %lums\n",
                    degrees, voltage / 100.0f, stepSize, stepInterval);
    } else if (motorMode == MODE_CURRENT) {
      Serial.printf("[Current] %d mA | Pos: %.1f deg | V: %.1fV\n",
                    current, degrees, voltage / 100.0f);
    } else {
      Serial.printf("[Speed] %.2f RPM | Pos: %.1f deg | V: %.1fV | I: %dmA\n",
                    actualSpeed / 100.0f, degrees, voltage / 100.0f, current);
    }
  }

  delay(10);
}
