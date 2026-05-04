# Orbital Temple - Satellite Synchronized Sculpture

ESP32 firmware for synchronizing sculpture movement with satellite visualization using Firebase Realtime Database.

## 🎯 System Overview

The sculpture tracks a satellite's position relative to Miami in real-time:
- **Satellite above Miami (0°)** → Sculpture points **UP** (12:00)
- **Satellite to right (90°)** → Sculpture points **RIGHT** (3:00)
- **Satellite below Miami (180°)** → Sculpture points **DOWN** (6:00)
- **Satellite to left (270°)** → Sculpture points **LEFT** (9:00)

**Key Feature**: Unidirectional rotation - motor only moves forward (clockwise), always taking the shortest forward path to the target.

## 🛠️ Hardware

- **Board**: Seeed XIAO ESP32-S3
- **Motor**: M5Stack Unit RollerCAN (I2C address 0x64)
- **Connections**:
  - SDA: GPIO 8 (D9)
  - SCL: GPIO 9 (D10)

## 📋 Setup Instructions

### 1. Configure WiFi & Firebase

The code is already configured with:
- WiFi: `iPhone Pavoni` / `12345678`
- Firebase database secret: `TFWzZASAG79bobA3pGUpqgrsD97ItaE00hbckqnh`

If you need to change WiFi, edit `src/main.cpp` lines 42-43:
```cpp
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

### 2. Build and Upload

Using PlatformIO:

```bash
cd "/Users/edsonpavoni/Library/CloudStorage/Dropbox/2025 Witness/code/examples/07_satellite_sync"
pio run --target upload
```

Or use PlatformIO IDE:
1. Open folder in VS Code
2. Click "Upload" in PlatformIO toolbar

### 3. Monitor Serial Output

```bash
pio device monitor
```

You should see:
```
============================================
  ORBITAL TEMPLE - SCULPTURE CONTROLLER
  Satellite Synchronized Tracking System
============================================

Initializing I2C... ✓
Initializing motor... ✓
⚠ No calibration found - use web panel to calibrate
Connecting to WiFi 'iPhone Pavoni'... ✓
  IP: 192.168.1.XXX
✓ Firebase initialized
✓ Web server started
  Control panel: http://192.168.1.XXX

✓ System ready!
============================================

⚠ CALIBRATION REQUIRED:
  1. Manually position sculpture pointing UP (12:00)
  2. Access web panel and click 'SET CALIBRATION'
```

### 4. Calibrate the Sculpture

**IMPORTANT**: This step is required before the sculpture will track the satellite!

1. **Manually rotate** the sculpture so it points **straight up** (12:00 position)

2. **Access the web control panel** in your browser:
   ```
   http://192.168.1.XXX
   ```
   (Use the IP address shown in serial monitor)

3. **Click "SET CALIBRATION"** button

4. The sculpture will now automatically track the satellite! 🎉

The calibration is saved to flash memory, so you only need to do this once.

## 🌐 Web Control Panel

Access at `http://[ESP32_IP_ADDRESS]` to view:

- **Calibration Status**: Shows if system is calibrated
- **Satellite Angle (Target)**: Current angle from Firebase
- **Sculpture Angle (Current)**: Actual sculpture position
- **Distance to Target**: How far sculpture needs to move
- **Motor Position (Raw)**: Motor encoder reading

### Control Buttons

- **SET CALIBRATION**: Calibrate current position as pointing up
- **STOP MOTOR**: Emergency stop
- **CLEAR CALIBRATION**: Reset calibration (requires re-calibration)

The page auto-refreshes every 2 seconds to show live updates.

## 📡 How It Works

### Data Flow

```
Satellite Position → Browser Calculation → Firebase
                                              ↓
                                         ESP32 Read (every 500ms)
                                              ↓
                                      Calculate Forward Distance
                                              ↓
                                    Unidirectional Motor Movement
                                              ↓
                                      Sculpture Tracks Satellite
```

### Firebase Structure

```json
{
  "sculpture": {
    "angle": {
      "angle": 45.7,
      "timestamp": 1701234567890
    }
  }
}
```

### Unidirectional Movement Logic

The motor **only rotates forward** (positive speed):

- If satellite is at 10° and sculpture is at 350°:
  - Forward distance: 20° (350° → 360° → 10°)
  - Motor rotates forward 20°

- If satellite is at 350° and sculpture is at 10°:
  - Forward distance: 340° (10° → 350°)
  - Motor rotates forward 340°

The system always chooses forward rotation, even if backward would be shorter. This ensures consistent, predictable movement.

### Speed Control

Speed varies based on distance to target:
- **> 30°**: Full speed (BASE_SPEED × 2)
- **10-30°**: Medium speed (BASE_SPEED)
- **< 10°**: Slow approach (proportional to distance)
- **< 2°**: Stop (within tolerance)

## 🔧 Configuration Variables

In `src/main.cpp` you can adjust:

```cpp
const float GEAR_RATIO = 1.0;            // Motor:sculpture ratio
const int32_t BASE_SPEED = 100;          // Base rotation speed
const int32_t MIN_SPEED = 10;            // Minimum speed
const float POSITION_TOLERANCE = 2.0;    // Stop within ±2°
const float PULSES_PER_REV = 1000.0;     // Motor encoder resolution
```

**IMPORTANT**: `PULSES_PER_REV` needs to match your motor's encoder. Check the RollerCAN datasheet.

## 🐛 Troubleshooting

### "Motor not found!"
- Check I2C wiring (SDA=GPIO8, SCL=GPIO9)
- Verify RollerCAN is powered
- Check I2C address is 0x64

### "WiFi failed!"
- Verify SSID and password are correct
- Check WiFi signal strength
- Motor will still work for manual testing via serial

### "Firebase not updating"
- Check Firebase database secret is correct
- Verify satellite visualization is running at https://orbital-temple.web.app/satellite
- Check Firebase console for database activity

### Web panel not loading
- Verify ESP32 IP address from serial monitor
- Check that WiFi connected successfully
- Try ping the IP address

### Sculpture not moving after calibration
- Check "Distance to Target" in web panel - should be updating
- Verify motor is enabled (should auto-enable on startup)
- Check serial monitor for error messages
- Try clicking "STOP MOTOR" then wait for movement to resume

### Motor moving wrong direction
The motor should **always** rotate forward (positive speed). If it's rotating backward:
- Check motor wiring polarity
- The system uses positive speed values only - verify in serial monitor

### Sculpture position drifting
- Recalibrate using web panel
- Check `PULSES_PER_REV` matches your motor
- Adjust `GEAR_RATIO` if using gears
- Motor encoder might be slipping - check mechanical connection

## 📊 Serial Monitor Commands

While the web panel is recommended, you can also monitor via serial at 115200 baud:

- System will print status on startup
- New target angles from Firebase: `📡 New target: 45.1°`
- Calibration events: `✓ CALIBRATION SET`
- Firebase errors (if any): `⚠ Firebase read error: [reason]`

## 🚀 Deployment

Once tested and calibrated:

1. Sculpture will automatically track satellite on power-up (calibration stored in flash)
2. System auto-reconnects to WiFi and Firebase if connection is lost
3. Web panel always available for monitoring and emergency stop
4. No intervention needed - just power on and it tracks!

## 🎨 Live Visualization

Watch the satellite in real-time:
**https://orbital-temple.web.app/satellite**

The sculpture mirrors the satellite's movement relative to Miami!

---

**Firmware Version**: v1.0 - Unidirectional Tracking
**Compatible with**: Firebase Realtime Database, ESP32-S3, RollerCAN motor
