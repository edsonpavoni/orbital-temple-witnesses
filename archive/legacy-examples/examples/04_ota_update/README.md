# OTA Update Example

Wireless code updates for deployed Orbital Temple sculptures.

## What is OTA?

**OTA (Over-The-Air)** updates allow you to upload new code to your ESP32 wirelessly over WiFi, without needing a USB connection. This is essential for sculptures that are:
- Mounted in hard-to-reach locations
- Sealed in enclosures
- Deployed in the field
- Rotating or moving

## Quick Start

### 1. First Upload (via USB)

```bash
# Open this folder in PlatformIO
cd examples/04_ota_update

# Upload via USB (first time only)
pio run -t upload
```

### 2. Configure WiFi

1. Connect to WiFi network: `Witness-OTA`
2. Password: `witness123`
3. Open browser to: `http://192.168.4.1`
4. Enter your WiFi credentials
5. Device will restart and connect to your WiFi

### 3. Enable OTA Uploads

Edit `platformio.ini` and uncomment these lines:

```ini
upload_protocol = espota
upload_port = witness-ota.local
```

Or use IP address instead:
```ini
upload_protocol = espota
upload_port = 192.168.1.XXX  # Replace with your device's IP
```

### 4. Upload Wirelessly!

```bash
# Now uploads happen over WiFi automatically
pio run -t upload
```

## How It Works

1. **ArduinoOTA**: Built-in ESP32 library for wireless updates
2. **mDNS**: Makes device discoverable as `witness-ota.local`
3. **Password Protection**: OTA updates require password (`orbital2025`)
4. **Web Interface**: Configure WiFi and monitor status

## Security

**Default OTA Password:** `orbital2025`

For production sculptures, change the password in `main.cpp`:

```cpp
#define OTA_PASSWORD "your-secure-password-here"
```

## Troubleshooting

### Can't find device

1. Check serial monitor for IP address
2. Make sure computer and ESP32 are on same WiFi network
3. Try using IP address instead of `.local` name
4. Check firewall settings

### Upload fails

1. Verify OTA password is correct
2. Device might be busy - wait and retry
3. Check WiFi signal strength
4. Restart device and try again

### WiFi won't connect

1. Check credentials are correct
2. 2.4GHz WiFi only (5GHz not supported)
3. Some enterprise WiFi networks may not work
4. Try different WiFi network

## Using with Arduino IDE

1. Upload code via USB first (this example)
2. Configure WiFi via web interface
3. In Arduino IDE: **Tools → Port**
4. Select: `witness-ota at 192.168.X.X`
5. Upload normally!

## Integration with Your Code

To add OTA to your existing sculpture code:

1. Copy the WiFi setup code from this example
2. Copy the OTA setup code
3. Add `ArduinoOTA.handle()` to your `loop()`
4. Update `platformio.ini` for OTA uploads

Example:
```cpp
#include <ArduinoOTA.h>

void setup() {
  // Your existing setup...
  setupWiFi();
  setupOTA();
}

void loop() {
  ArduinoOTA.handle();  // Add this line
  // Your existing loop code...
}
```

## Notes

- Motor automatically stops during OTA update for safety
- First upload MUST be via USB
- All subsequent uploads can be wireless
- Device needs WiFi connection for OTA
- Keep OTA password secure for production use
