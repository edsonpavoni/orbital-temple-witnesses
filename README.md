# Orbital Temple Witnesses Sculptures

Kinetic sculptures connected with the Orbital Temple Satellite.

Control systems using ESP32 and brushless motors.

## Project Structure

```
orbital-temple-witnesses/
├── examples/
│   ├── 01_rollercan_hello_world/   # Basic motor control example
│   ├── 02_witness_sculpture/       # Full sculpture with WiFi & cloud sync
│   ├── 03_motion_test/             # Motion testing with easing & PID tuning
│   ├── 04_ota_update/              # Wireless code updates (OTA)
│   ├── 05_wifi_control/            # WiFi motor control (velocity & angle)
│   ├── 06_feet_simple_rotation/    # Feet sculpture - simple rotation
│   ├── 07_feet_wifi_control/       # Feet sculpture - WiFi control
│   ├── 07_satellite_sync/          # Real-time satellite position tracking
│   ├── 08_heart_pulsation/         # Heart sculpture - pulsation test
│   ├── 09_simple_pulse_test/       # Simple pulse motion test
│   ├── 10_heart_simple/            # Heart sculpture - basic control
│   ├── 11_heart_smooth/            # Heart sculpture - WiFi web interface
│   ├── 12_heart_autoloop/          # Heart sculpture - auto loop (720°)
│   └── 13_heart_fast/              # Heart sculpture - fast loop (1080°)
├── lib/                             # Shared libraries (future)
├── docs/                            # Documentation (future)
└── README.md
```

## Hardware

### Microcontroller
- **Seeed XIAO ESP32-S3** - Compact ESP32-S3 board with USB-C

### Motors
- **M5Stack Unit RollerCAN** (SKU: U182)
  - Brushless DC motor with integrated FOC driver
  - I2C/CAN communication
  - Built-in magnetic encoder
  - 0.66" OLED display for configuration

## Examples

### 01_rollercan_hello_world
Basic example to spin the RollerCAN motor at a constant speed via I2C.

**Features:**
- Constant speed control
- I2C communication
- Works with USB power (5V) or external power (6-16V)

**First-time motor setup:**
1. Long press the button on RollerCAN before powering
2. Select I2C mode in the menu
3. Power cycle the motor

### 02_witness_sculpture
Full kinetic sculpture with searching and tracking behaviors.

**Features:**
- State machine (searching/tracking modes)
- WiFi with credential storage
- Web interface for control and calibration
- mDNS discovery (witness-XXX.local)
- Cloud sync with Firebase
- Remote control via cloud commands

### 03_motion_test
Serial-controlled motion testing for developing smooth motor transitions.

**Features:**
- Interactive serial commands
- Multiple easing modes (linear, ease-in-out, ease-in, ease-out)
- Adjustable transition times
- PID tuning interface
- Speed and position control modes
- Real-time motion monitoring

### 04_ota_update
Wireless code updates for deployed sculptures.

**Features:**
- ArduinoOTA for wireless uploads
- WiFi configuration via web interface
- mDNS discovery (witness-ota.local)
- Password-protected updates
- Works with PlatformIO and Arduino IDE
- No USB cable needed after initial setup

### 05_wifi_control
WiFi-based motor control with web interface and API.

**Features:**
- Velocity control (continuous rotation in RPM)
- Absolute position control (rotate to specific angle)
- Relative position control (rotate by degrees)
- Real-time status monitoring
- Web interface with interactive sliders
- RESTful API for programmatic control
- mDNS discovery (witness-ctrl.local)
- Position calibration/reset

### 06_feet_simple_rotation
Simple rotation control for feet sculpture.

**Features:**
- Basic rotation control
- Serial command interface
- Position monitoring

### 07_feet_wifi_control
WiFi-enabled feet sculpture with web control.

**Features:**
- WiFi access point mode
- Web interface for control
- Real-time status updates

### 07_satellite_sync
Real-time satellite position tracking and visualization.

**Features:**
- Fetches ISS position from API
- Calculates azimuth and elevation
- Maps satellite movement to motor rotation
- Web-based 3D visualization
- Automatic position updates
- Local server for development

### 08_heart_pulsation
Heart sculpture pulsation testing.

**Features:**
- Pulsating movement patterns
- Serial control interface
- Adjustable pulse parameters

### 09_simple_pulse_test
Simple pulse motion testing.

**Features:**
- Basic pulse movement
- Testing different motion patterns

### 10_heart_simple
Heart sculpture with basic serial control.

**Features:**
- Initialize (2 rotations)
- Hold position with maximum power
- Gentle rotation loop
- Active feedback control
- Emergency stop
- Serial command interface

### 11_heart_smooth
Heart sculpture with WiFi web interface and burst rotation.

**Features:**
- **WiFi Access Point mode** (Heart-Sculpture network)
- **Beautiful web interface** at http://192.168.4.1
- Control from any phone/tablet
- Initialize, Hold, Loop, Power Pull commands
- Adjustable loop distance (45° to 1440°)
- Adjustable pause time (0ms to 10s)
- Real-time status updates
- Burst rotation with maximum force (800 speed)
- Emergency stop
- 720° default loop (2 full rotations)

### 12_heart_autoloop
Heart sculpture that auto-starts looping on power-up.

**Features:**
- **Auto-start**: Begins looping 3 seconds after power-on
- 720° rotation (2 full rotations each direction)
- Speed: 800
- No WiFi - simple and reliable
- USB-powered control
- Serial commands for adjustments
- Perfect for exhibitions

### 13_heart_fast
Fast version with 50% more rotation and 50% faster speed.

**Features:**
- **Auto-start**: Begins looping 3 seconds after power-on
- **1080° rotation** (3 full rotations each direction)
- **Speed: 1200** (50% faster than standard)
- Most dramatic movement
- High-speed burst rotation
- Serial command adjustments
- Emergency stop

## Getting Started

1. Install [PlatformIO](https://platformio.org/)
2. Clone this repository
3. Open an example folder in PlatformIO
4. Connect hardware and upload

## License

MIT License - See LICENSE file for details.

## Author

Edson Pavoni - Orbital Temple Project
