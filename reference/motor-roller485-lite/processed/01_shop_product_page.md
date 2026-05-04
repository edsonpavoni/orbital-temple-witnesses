# Roller485 Lite Unit (STM32) — Shop Product Page

Source: https://shop.m5stack.com/products/roller485-lite-unit-without-slip-ring-stm32?variant=45585621778689

## Product Overview

The **Roller485 Lite** is a brushless DC motor control module featuring integrated FOC (field-oriented control) drive technology. It combines motor control, sensing, and display capabilities in a compact 40x40x40mm package designed for robotic joints and motion automation applications.

## Key Features

- Brushless DC motor with integrated FOC closed-loop control
- RS-485 and I2C communication interfaces
- Built-in 0.66-inch OLED display for real-time status monitoring
- RGB LED indicators for visual feedback
- LEGO-compatible mounting with M3 screw holes
- Fully open-source hardware and software
- SWD/SWO debugging interfaces

## Technical Specifications

| Parameter | Value |
|-----------|-------|
| **Microcontroller** | STM32G431CBU6 (Cortex-M4), 128KB Flash, 32KB SRAM, 170MHz |
| **Motor Type** | D3504 200KV brushless (41mm diameter) |
| **Driver Chip** | DRV8311HRRWR |
| **Angle Sensor** | TLI5012BE1000 |
| **Communication** | RS-485 (HT3.96-4P), I2C (addr: 0x64) |
| **Display** | 0.66" OLED, 64x48 resolution, SPI |
| **RGB LEDs** | 2x WS2812-2020 |
| **Motor Power Input** | 6-16V DC (RS-485) or 5V (Grove) |
| **Max Phase Current** | 0.5A continuous, 1A short-term |
| **Operating Temperature** | 0–40°C |
| **Noise Level** | 48dB |

### Performance Under Load (16V DC)

| Load | Speed | Current |
|------|-------|---------|
| No load | — | 78mA |
| 50g | 2100 rpm | 225mA |
| 200g | 1400 rpm | 601mA |
| 500g (max) | 560 rpm | 918mA |

### Output Torque

- **5V Grove power**: 0.021 N·m at 350mA
- **16V RS-485 power**: 0.065 N·m at 927mA

### Standby Current

- 5V Grove: 70mA
- 16V RS-485: 32mA

## Physical Specifications

| Dimension | Measurement |
|-----------|-------------|
| Product Size | 40.0 x 40.0 x 40.0mm |
| Product Weight | 71.0g |
| Package Size | 105.0 x 76.0 x 54.0mm |
| Gross Weight | 151.0g |

## Package Contents

- 1x Roller485 Lite Unit
- 1x HT3.96-4P connector
- 1x HY2.0-4P Grove cable (5cm)
- 6x Friction pins
- 1x Flange
- 1x Bracket
- 1x Motor mounting plate
- 2x Hex keys (2.5mm, 2mm)
- 6x M3 nuts
- 2x M3×14mm hex socket head screws
- 4x M3×14mm hex socket head screws
- 2x M3×12mm hex socket head screws
- 4x M3×5mm hex socket head screws
- 1x Single-ended terminal wire 5P debug cable

## Control Capabilities

The unit supports "current, speed, and position triple-loop control" for precise motion execution across three operational modes.

## Important Notes

**Voltage Limits**: Input voltage must not exceed 16V. Exceeding 18V triggers an "E:1" over-voltage fault code, disabling motor operation with red LED indication.

**Position Accuracy**: In absolute position mode, 36,000 encoder counts = 360°. Mechanical installation tolerances may introduce approximately 2° angular error.

**Key Differences from Standard Unit-Roller485**: The Lite variant omits the slip ring expansion Grove interface and top LEGO expansion interface.

## Applications

- Robotic joint control
- Smart manufacturing equipment
- Visual demonstration and prototyping projects

## Documentation & Support

Complete technical documentation available at: https://docs.m5stack.com/en/products/sku/U182-Lite
