# WiFi Motor Control

Control your RollerCAN motor wirelessly - set velocity and rotation angle via web interface or API.

## Features

### Control Modes
- **Velocity Mode**: Continuous rotation at set RPM (supports negative for reverse)
- **Absolute Position**: Rotate to specific angle (0-360°)
- **Relative Position**: Rotate by specific degrees from current position

### Web Interface
- Real-time motor status display
- Interactive sliders for velocity and angle control
- Quick rotation buttons (+90°, -90°, +180°, Home)
- Emergency stop button
- Position reset/calibration

### API Endpoints
- `GET /status` - Get current motor status (JSON)
- `POST /velocity?rpm=X` - Set velocity in RPM
- `POST /angle?deg=X` - Rotate to absolute angle
- `POST /rotate?deg=X` - Rotate by relative angle
- `POST /stop` - Emergency stop
- `POST /reset` - Reset position to 0°
- `POST /home` - Return to home position (0°)

## Quick Start

### 1. Upload Code

```bash
cd examples/05_wifi_control
pio run -t upload
```

### 2. Configure WiFi (First Time)

1. Connect to WiFi: `Witness-Control` (password: `witness123`)
2. Open browser: `http://192.168.4.1`
3. Enter your WiFi credentials
4. Device restarts and connects

### 3. Access Control Panel

After WiFi connection:
- **Using mDNS**: `http://witness-ctrl.local`
- **Using IP**: Check serial monitor for IP address

## Web Interface Usage

### Velocity Control
1. Move the **Speed** slider to desired RPM (-200 to +200)
2. Click **Set Velocity**
3. Motor will rotate continuously at that speed
4. Use **STOP** button to halt

### Position Control
1. Move the **Target Angle** slider (0-360°)
2. Click **Go to Angle**
3. Motor rotates to that position and stops

### Quick Rotation
- **+90°** / **-90°** buttons rotate 90° clockwise/counter-clockwise
- **+180°** button rotates 180°
- **Home** button returns to 0°

### Calibration
- **Reset Position** sets current position as 0°

## API Examples

### Using curl

```bash
# Set velocity to 100 RPM
curl -X POST http://witness-ctrl.local/velocity?rpm=100

# Rotate to 180 degrees
curl -X POST http://witness-ctrl.local/angle?deg=180

# Rotate 90 degrees clockwise from current position
curl -X POST http://witness-ctrl.local/rotate?deg=90

# Stop motor
curl -X POST http://witness-ctrl.local/stop

# Get status
curl http://witness-ctrl.local/status
```

### Using Python

```python
import requests

base_url = "http://witness-ctrl.local"

# Set velocity
requests.post(f"{base_url}/velocity?rpm=50")

# Rotate to angle
requests.post(f"{base_url}/angle?deg=270")

# Get status
status = requests.get(f"{base_url}/status").json()
print(f"Position: {status['position']}°")
print(f"Speed: {status['rpm']} RPM")
```

### Using JavaScript

```javascript
// Set velocity
fetch('http://witness-ctrl.local/velocity?rpm=100', { method: 'POST' });

// Rotate to angle
fetch('http://witness-ctrl.local/angle?deg=180', { method: 'POST' });

// Get status
fetch('http://witness-ctrl.local/status')
  .then(r => r.json())
  .then(data => {
    console.log(`Position: ${data.position}°`);
    console.log(`Speed: ${data.rpm} RPM`);
  });
```

## Status Response Format

```json
{
  "mode": "VELOCITY",      // IDLE, VELOCITY, POSITION, or MOVING
  "rpm": 50.2,             // Current speed in RPM
  "position": 123.4,       // Current angle in degrees (0-360)
  "target": 180.0,         // Target angle (for position mode)
  "connected": true        // Motor connection status
}
```

## Configuration

Edit `main.cpp` to customize:

```cpp
#define DEVICE_HOSTNAME "witness-ctrl"  // mDNS name
#define MAX_CURRENT_MA   1000            // Current limit (mA)
#define VELOCITY_LIMIT   500             // Max RPM
```

## Integration Examples

### Home Automation

Control from Home Assistant, Node-RED, or similar:

```yaml
# Home Assistant REST Command
rest_command:
  sculpture_home:
    url: http://witness-ctrl.local/home
    method: POST

  sculpture_rotate:
    url: http://witness-ctrl.local/angle?deg={{ angle }}
    method: POST
```

### Scheduled Movements

Use cron or task scheduler:

```bash
#!/bin/bash
# Rotate to different positions throughout the day
curl -X POST http://witness-ctrl.local/angle?deg=0    # Morning
sleep 21600
curl -X POST http://witness-ctrl.local/angle?deg=90   # Noon
sleep 21600
curl -X POST http://witness-ctrl.local/angle?deg=180  # Evening
```

### Interactive Art Installation

```python
# React to sensor input, time, or data
import time
import requests

base = "http://witness-ctrl.local"

while True:
    # Read sensor or data source
    angle = get_angle_from_data()

    # Move sculpture
    requests.post(f"{base}/angle?deg={angle}")

    time.sleep(60)
```

## Troubleshooting

### Motor doesn't move
- Check I2C wiring (SDA=D9/GPIO8, SCL=D10/GPIO9)
- Verify motor is in I2C mode (hold button during power-up)
- Check serial monitor for connection errors

### Can't access web interface
- Verify WiFi connection (check serial monitor)
- Try IP address instead of mDNS name
- Ensure device and computer on same network

### Position drift
- Use **Reset Position** to recalibrate
- Motor encoder may need mechanical calibration
- Check for mechanical resistance or binding

### API commands fail
- Check HTTP method (POST vs GET)
- Verify parameter names (rpm, deg)
- Look for error messages in serial monitor

## Notes

- Motor automatically stops when reaching target angle in position mode
- Velocity mode runs continuously until stopped or new command
- Position is normalized to 0-360° range
- Negative velocities rotate counter-clockwise
- Web interface updates every 500ms
- Serial monitor shows detailed status every second
