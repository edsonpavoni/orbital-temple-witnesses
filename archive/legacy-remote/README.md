# Orbital Temple Witnesses Sculptures

Kinetic sculptures connected with the Orbital Temple Satellite.

Control systems using ESP32 and brushless motors.

## Project Structure

```
orbital-temple-witnesses/
├── examples/
│   └── 01_rollercan_hello_world/   # Basic motor control example
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

## Getting Started

1. Install [PlatformIO](https://platformio.org/)
2. Clone this repository
3. Open an example folder in PlatformIO
4. Connect hardware and upload

## License

MIT License - See LICENSE file for details.

## Author

Edson Pavoni - Orbital Temple Project
