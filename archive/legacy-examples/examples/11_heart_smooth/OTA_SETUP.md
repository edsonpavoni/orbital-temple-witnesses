# Heart Sculpture - OTA Update Setup

## First Time Setup (USB Required Once)

### 1. Configure WiFi Credentials

Edit `src/main.cpp` and change these lines (around line 26-28):

```cpp
const char* ssid = "YOUR_WIFI_SSID";        // Replace with your WiFi name
const char* password = "YOUR_WIFI_PASSWORD"; // Replace with your WiFi password
const char* hostname = "heart-sculpture";    // OTA hostname
```

### 2. Upload via USB (One Time Only)

```bash
cd "/Users/edsonpavoni/Library/CloudStorage/Dropbox/2025 Witness/code/examples/11_heart_smooth"
~/.platformio/penv/bin/platformio run --target upload
```

**Note:** If USB upload fails repeatedly, you may need to:
- Disconnect motor power during upload
- Try a different USB cable
- Press the BOOT button on the XIAO while uploading

### 3. Verify WiFi Connection

After the first upload, open Serial Monitor:

```bash
~/.platformio/penv/bin/platformio device monitor
```

You should see:
```
Connecting to WiFi.... ✓
  IP address: 192.168.X.X
  Hostname: heart-sculpture
  OTA ready! Upload via: heart-sculpture.local
```

## Future Updates (Over WiFi)

Once WiFi is configured and working, you can upload updates wirelessly:

### Method 1: PlatformIO Command Line

```bash
cd "/Users/edsonpavoni/Library/CloudStorage/Dropbox/2025 Witness/code/examples/11_heart_smooth"
~/.platformio/penv/bin/platformio run --target upload --upload-port heart-sculpture.local
```

### Method 2: Using IP Address

If mDNS (.local) doesn't work, use the IP address:

```bash
~/.platformio/penv/bin/platformio run --target upload --upload-port 192.168.X.X
```

(Replace 192.168.X.X with the actual IP shown in Serial Monitor)

## Troubleshooting

### WiFi Not Connecting
- Check SSID and password are correct
- Make sure ESP32 is within WiFi range
- Verify WiFi is 2.4GHz (ESP32 doesn't support 5GHz)

### OTA Upload Fails
- Make sure device is powered on and connected to WiFi
- Check you're on the same network as the ESP32
- Try using IP address instead of hostname
- Restart the ESP32 and try again

### Finding the Device
If you don't know the IP address, check your router's DHCP client list or use:

```bash
# On macOS/Linux
ping heart-sculpture.local

# Or use nmap to scan network
nmap -sn 192.168.1.0/24 | grep -B 2 "Espressif"
```

## Current Features

All previous commands still work:
- `i` - Initialize (2 rotations)
- `h` - Hold position
- `l` - Loop rotation
- `p` - Power pull (burst + 3 rotations)
- `e` - Emergency stop
- `+/-` - Adjust speed
- `?` - Show status

The OTA functionality runs in the background and doesn't interfere with motor control.
