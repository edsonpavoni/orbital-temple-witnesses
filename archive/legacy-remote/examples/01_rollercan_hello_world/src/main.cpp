/*
 * ============================================================================
 * M5Stack RollerCAN - Hello World (Constant Speed)
 * ============================================================================
 *
 * Part of: Orbital Temple Witnesses Sculptures
 *
 * This example spins the M5Stack Unit RollerCAN motor at a constant speed
 * using I2C communication from a Seeed XIAO ESP32-S3.
 *
 * ============================================================================
 * FIRST TIME SETUP (IMPORTANT!)
 * ============================================================================
 * Before this code will work, you must configure the motor via its OLED menu:
 *
 * 1. BEFORE powering the motor, LONG PRESS the button on the RollerCAN
 * 2. While holding the button, connect power
 * 3. The setup menu will appear on the OLED screen
 * 4. Select "I2C" mode (even if it appears already selected)
 * 5. Exit the menu
 * 6. Power cycle the motor
 * 7. Now the I2C control will work
 *
 * ============================================================================
 * WIRING (Grove PORT.A to XIAO ESP32-S3)
 * ============================================================================
 *
 *   RollerCAN PORT.A    XIAO ESP32-S3
 *   ─────────────────────────────────
 *   GND (black)    →    GND
 *   5V  (red)      →    5V
 *   SDA (yellow)   →    D9  (GPIO 8)
 *   SCL (white)    →    D10 (GPIO 9)
 *
 * ============================================================================
 * POWER OPTIONS
 * ============================================================================
 *
 * Option 1: USB only (low torque)
 *   - Power XIAO via USB
 *   - Motor runs at 5V with limited torque (0.021 N.m)
 *   - Good for testing without load
 *
 * Option 2: External 6-16V (full torque)
 *   - Connect 6-16V DC to RollerCAN's XT30 connector
 *   - Motor provides 5V to XIAO via Grove cable
 *   - Full torque available (0.065 N.m at 16V)
 *
 * ============================================================================
 * HARDWARE
 * ============================================================================
 *
 * - M5Stack Unit RollerCAN (SKU: U182)
 *   - MCU: STM32G431CBU6 (Cortex-M4, 170MHz)
 *   - Motor: D3504 200KV brushless, 41mm diameter
 *   - Driver: DRV8311
 *   - Encoder: TLI5012BE1000 magnetic
 *   - Display: 0.66" OLED (64x48)
 *   - Default I2C address: 0x64
 *
 * - Seeed XIAO ESP32-S3
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

// I2C Settings
#define ROLLER_I2C_ADDR  0x64
#define I2C_SDA_PIN      8    // XIAO D9
#define I2C_SCL_PIN      9    // XIAO D10
#define I2C_FREQ         100000

// Motor Settings - values are scaled by 100 for precision
// Example: 10000 = 100.00 RPM, 50000 = 500.00 RPM
#define MOTOR_SPEED      10000    // 100.00 RPM
#define MOTOR_MAX_CURRENT 100000  // 1000.00 mA

// =============================================================================
// REGISTER ADDRESSES (M5Stack Unit Roller I2C Protocol)
// =============================================================================

#define REG_OUTPUT       0x00  // Motor output: 0=off, 1=on
#define REG_MODE         0x01  // Operating mode
#define REG_SYS_STATUS   0x0C  // System status
#define REG_ERROR_CODE   0x0D  // Error code
#define REG_KEY_SWITCH   0x0E  // Control source: 0=I2C, 1=button
#define REG_STALL_PROT   0x0F  // Stall protection: 0=off, 1=on
#define REG_SPEED        0x40  // Target speed (int32, scaled x100)
#define REG_SPEED_MAXCUR 0x50  // Speed mode max current (int32, scaled x100)
#define REG_SPEED_READ   0x60  // Actual speed readback (int32, scaled x100)
#define REG_RGB          0x30  // RGB LED color

// Operating Modes
#define MODE_SPEED       1
#define MODE_POSITION    2
#define MODE_CURRENT     3
#define MODE_ENCODER     4

// =============================================================================
// FUNCTION PROTOTYPES
// =============================================================================

void motorInit();
void motorSetSpeed(int32_t speed);
void motorEnable(bool enable);
int32_t motorGetSpeed();
void writeReg8(uint8_t reg, uint8_t value);
uint8_t readReg8(uint8_t reg);
void writeReg32(uint8_t reg, int32_t value);
int32_t readReg32(uint8_t reg);

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);  // Wait for USB CDC

  Serial.println();
  Serial.println("========================================");
  Serial.println("  RollerCAN Hello World");
  Serial.println("  Orbital Temple Witnesses Sculptures");
  Serial.println("========================================");
  Serial.println();

  // Initialize I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_FREQ);
  delay(100);

  // Check connection
  Wire.beginTransmission(ROLLER_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: RollerCAN not found!");
    Serial.println("Check wiring and first-time setup.");
    while (1) delay(1000);
  }
  Serial.println("RollerCAN connected.");

  // Initialize and start motor
  motorInit();

  Serial.println();
  Serial.print("Motor running at ");
  Serial.print(MOTOR_SPEED / 100.0f);
  Serial.println(" RPM");
  Serial.println();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  int32_t speed = motorGetSpeed();
  Serial.print("Speed: ");
  Serial.print(speed / 100.0f, 1);
  Serial.println(" RPM");

  delay(1000);
}

// =============================================================================
// MOTOR FUNCTIONS
// =============================================================================

void motorInit() {
  // 1. Disable output during configuration
  motorEnable(false);
  delay(50);

  // 2. Set I2C control mode
  writeReg8(REG_KEY_SWITCH, 0);
  delay(50);

  // 3. Disable stall protection
  writeReg8(REG_STALL_PROT, 0);
  delay(50);

  // 4. Set speed mode
  writeReg8(REG_MODE, MODE_SPEED);
  delay(50);

  // 5. Set target speed
  motorSetSpeed(MOTOR_SPEED);
  delay(50);

  // 6. Set max current
  writeReg32(REG_SPEED_MAXCUR, MOTOR_MAX_CURRENT);
  delay(50);

  // 7. Enable output
  motorEnable(true);
}

void motorSetSpeed(int32_t speed) {
  writeReg32(REG_SPEED, speed);
}

void motorEnable(bool enable) {
  writeReg8(REG_OUTPUT, enable ? 1 : 0);
}

int32_t motorGetSpeed() {
  return readReg32(REG_SPEED_READ);
}

// =============================================================================
// I2C HELPER FUNCTIONS
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
