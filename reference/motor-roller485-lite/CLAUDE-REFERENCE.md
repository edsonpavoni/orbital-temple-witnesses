# Unit-Roller485 Lite — Claude Reference

**SKU:** U182-Lite · **Form factor:** 40×40×40mm, 71g

Integrated BLDC motor actuator with onboard FOC control, magnetic encoder feedback, OLED status display, RGB LEDs, function button, and RS-485 + I2C control. Fully open hardware/firmware. The "Lite" variant drops the slip-ring passthrough and top LEGO interface of the standard Unit-Roller485.

---

## 1. Key parts

| Subsystem | Part | Notes |
|-----------|------|-------|
| MCU | STM32G431CBU6 (Cortex-M4, 170 MHz, 128 KB Flash, 32 KB SRAM) | Runs FOC loop + RS-485/I2C stack |
| Motor driver | **TI DRV8311HRRWR** | 3-phase FOC/6-step BLDC gate driver, integrated FETs, SPI + PWM |
| Angle sensor | **Infineon TLI5012B E1000** | GMR-based 360° contactless, SSC (SPI-compatible) / IIF / HSM / PWM interfaces; 15-bit absolute |
| Motor | D3504 200 KV BLDC, Ø41 mm | |
| Display | 0.66″ OLED, 64×48, SPI | |
| LEDs | 2× WS2812-2020 RGB | |

---

## 2. Power & electrical limits (READ BEFORE WIRING)

| Parameter | Value |
|-----------|-------|
| Motor power via PWR-485 (HT3.96-4P) | **6 – 16 V DC** |
| Motor power via Grove | 5 V DC |
| **Absolute max input** | **≤ 16 V** — >18 V triggers `E:1 Over Voltage`, motor disabled, red LED |
| Max continuous phase current | 0.5 A |
| Short-term peak phase current | 1 A |
| Standby current (16 V RS-485) | 32 mA |
| Standby current (5 V Grove) | 70 mA |
| Operating temp | 0 – 40 °C |
| Noise | 48 dB |

**Output torque:** 0.021 N·m @ 5 V/350 mA · 0.065 N·m @ 16 V/927 mA.

**Load → speed @ 16 V:** 50 g → 2100 rpm · 200 g → 1400 rpm · 500 g (max) → 560 rpm · no load → 78 mA.

---

## 3. Pinout

### Grove port (HY2.0-4P, PORT.A)

| Colour | Black | Red | Yellow | White |
|--------|-------|-----|--------|-------|
| Signal | **GND** | **5 V** | **SDA** | **SCL** |

### MCU pin assignments

| Function | STM32 pin | Signal |
|----------|-----------|--------|
| I2C SCL | PA15 | SYS_I2C_SCL |
| I2C SDA | PB7 | SYS_I2C_SDA |
| RS-485 RX | PC11 | RS485_RX |
| RS-485 TX | PC10 | RS485_TX |
| RS-485 DIR | PB4 | RS485_DIR (driver enable) |
| RGB data | PB5 | LED_DAT (WS2812) |
| Button A | PA12 | SYS_SW |
| OLED MOSI | PB15 | OLED_MOSI |
| OLED SCK | PB13 | OLED_SCK |
| OLED DC | PB14 | OLED_DC |
| OLED RST | PB11 | OLED_RST |
| OLED CS | PB12 | OLED_CS |

PWR-485 connector: HT3.96-4P (V+, V-, 485-A, 485-B — confirm against `sources/sch_Unit-Roller485_V1.0.pdf`).

---

## 4. Operating modes

| Mode | Code | Description |
|------|------|-------------|
| `ROLLER_MODE_SPEED` | 1 | Closed-loop target rpm |
| `ROLLER_MODE_POSITION` | 2 | Closed-loop absolute position (encoder counts) |
| `ROLLER_MODE_CURRENT` | 3 | Closed-loop target current |
| `ROLLER_MODE_ENCODER` | 4 | Passive: unit reports encoder counts only (use it as an input dial) |

**Encoder scaling:** `36 000 counts = 360°` in absolute position mode. Mechanical alignment tolerance ≈ ±2°.

---

## 5. RS-485 protocol (summary)

Full detail: `processed/Unit_Roller485_RS485_Protocol_EN.md`.

- **Interface:** half-duplex async serial, default **115200 bps**, 8N1, LSB first.
- **Packet:** fixed **15 bytes** both directions.
- **Response** frames are prefixed by `0xAA 0x55` (NOT part of the CRC).

### Send packet layout

```
| Command | Device ID | Data1 | Data2 | Data3 | CRC8 |
|  1 byte |   1 byte  | 4 B   | 4 B   | 4 B   | 1 B  |
```

Unused data bytes → `0x00`. Default Device ID = `0x00` (range 0x00 – 0xFF).

### CRC8 (polynomial 0x8C, init 0x00, reflected)

```c
uint8_t crc8(uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00, i;
    while (len--) {
        crc ^= *data++;
        for (i = 0; i < 8; i++) {
            if (crc & 0x01) crc = (crc >> 1) ^ 0x8c;
            else            crc >>= 1;
        }
    }
    return crc;
}
```

### Command codes (from `unit_roller485.hpp`)

| Code | Name | Purpose |
|------|------|---------|
| `0x00` | SET_OUTPUT | Motor enable/disable |
| `0x01` | SET_MODE | Switch mode (speed/pos/current/encoder) |
| `0x06` | REMOVE_PROTECTION | Clear jam / over-range fault |
| `0x07` | SAVE_FLASH | Persist config to flash |
| `0x08` | ENCODER | Read/write encoder counter |
| `0x09` | BUTTON | Button → mode-switch enable |
| `0x0A` | SET_RGB | RGB LED colour + brightness + mode |
| `0x0B` | BAUD_RATE | 115200 / 19200 / 9600 |
| `0x0C` | SET_MOTOR_ID | Change device ID |
| `0x0D` | JAM_PROTECTION | Enable stall protection |
| `0x20` | SPEED_MODE | Set target speed + max current |
| `0x21` | SPEED_PID | Speed-loop P/I/D |
| `0x22` | POSITION_MODE | Set target position + max current |
| `0x23` | POSITION_PID | Position-loop P/I/D |
| `0x24` | CURRENT_MODE | Set target current |
| `0x40` | READ_BACK0 | Status group 0 (actual speed/pos/current) |
| `0x41` | READ_BACK1 | Status group 1 (Vin, temp, encoder) |
| `0x42` | READ_BACK2 | Status group 2 |
| `0x43` | READ_BACK3 | Status group 3 |
| `0x60` | READ_I2C_DATA1 | RS-485→I2C forwarding: read |
| `0x61` | WRITE_I2C_DATA1 | RS-485→I2C forwarding: write |
| `0x62` | READ_I2C_DATA2 | (longer form) |
| `0x63` | WRITE_I2C_DATA2 | (longer form) |

**Response command codes** are the request code `| 0x10` (e.g. `0x00` → `0x10`, `0x01` → `0x11`).

### Example: enable motor (Device 0x00)

```
TX: 00 00 01 00 00 00 00 00 00 00 00 00 00 00 68
RX: AA 55 10 00 01 00 00 00 00 00 00 00 00 00 00 9A
```

(CRC covers the 15 bytes starting at the command byte; the AA 55 prefix on the response is excluded.)

---

## 6. I2C protocol (summary)

Full detail: `processed/Unit_Roller485_I2C_Protocol_EN.md`.

- **Address:** `0x64` (7-bit). Configurable.
- **Clock:** 200 – 400 kHz recommended.
- **Access pattern:** standard 8-bit register address, variable-length R/W.

### Register map (abridged)

| Reg | R/W | Bytes | Meaning |
|-----|-----|-------|---------|
| `0x00` | R/W | 1 | Motor enable (0 off / 1 on) |
| `0x01` | R/W | 1 | Mode (1=speed, 2=position, 3=current, 4=encoder) |
| `0x0A` | W | 1 | Position over-range protection enable |
| `0x0B` | W | 1 | Remove protection (fault clear) |
| `0x0C` | R | 1 | Motor status |
| `0x0D` | R | 1 | Error code |
| `0x0E` | R/W | 1 | Button → mode-switch enable |
| `0x0F` | R/W | 1 | Jam protection enable |
| `0x10` | R/W | 1 | Device ID |
| `0x11` | R/W | 1 | RS-485 baud rate selector |
| `0x12` | R/W | 1 | RGB brightness |
| `0x20` | R/W | 4 | Position max current |
| `0x30` | R/W | 3 | RGB color |
| `0x33` | R/W | 1 | RGB mode (default / user) |
| `0x34` | R | 4 | Vin (mV) |
| `0x38` | R | 4 | Internal temperature |
| `0x3C` | R | 4 | Encoder counter |
| `0x40` | R/W | 4 | Speed target |
| `0x50` | R/W | 4 | Speed max current |
| `0x60` | R | 4 | Speed readback |
| `0x70` | R/W | 12 | Speed PID (3× uint32) |
| `0x80` | R/W | 4 | Position target |
| `0x90` | R | 4 | Position readback |
| `0xA0` | R/W | 12 | Position PID |
| `0xB0` | R/W | 4 | Current target |
| `0xC0` | R | 4 | Current readback |
| `0xF0` | W | 1 | Save to flash |
| `0xFE` | R | 1 | Firmware version |

Protection range (`0x0A`): encoder value outside `±2 100 000 000` counts → motor stops.

---

## 7. Arduino library (`M5Unit-Roller`)

Headers in `repos/M5Unit-Roller/src/`. Three transports provided:

- `UnitRoller485` (RS-485) — `unit_roller485.hpp`
- `UnitRollerI2C` (direct I2C) — `unit_rolleri2c.hpp`
- `UnitRollerCAN` (for the CAN variant U188) — `unit_rollercan.hpp`

### RS-485 minimal example

```cpp
#include "unit_roller485.hpp"
UnitRoller485 Roller485;
HardwareSerial mySerial(1);
#define motor485Id 0x00

void setup() {
    Roller485.begin(&mySerial, 115200, SERIAL_8N1,
                    /*rxPin*/16, /*txPin*/17, /*dePin*/-1,
                    /*invert*/false, /*timeout*/10000UL, /*bufferSize*/112U);
    Roller485.setMode(motor485Id, ROLLER_MODE_SPEED);
    Roller485.setSpeedMode(motor485Id, /*speed*/2400, /*max_current*/1200);
    Roller485.setOutput(motor485Id, true);
}
```

Speed units appear to be raw (≈ 0.01 rpm per count from firmware; confirm in situ). Current units raw mA.

### Key methods (RS-485)

Setters: `setOutput`, `setMode`, `setRemoveProtection`, `setSaveFlash`, `setEncoder`, `setButton`, `setRGB`, `setBaudRate`, `setMotorId`, `setJamProtection`, `setSpeedMode`, `setSpeedPID`, `setPositionMode`, `setPositionPID`, `setCurrentMode`.

Getters (send + read back): `getActualSpeed`, `getActualPosition`, `getActualCurrent`, `getEncoder`, `getActualVin`, `getActualTemp`, `getMotorId`, `getSpeedPID`, `getPositionPID`, `getRGB`, `getMode`, `getStatus`, `getError`, `getRGBMode`, `getRGBBrightness`, `getBaudRate`, `getButton`.

Slave-addressed variants (for forwarding through another device): `readX(id, address, …)` / `writeX(id, address, …)`.

Return values: `int8_t` methods return `1` success, `0` write fail, `-1` CRC fail, `-2` timeout, `-3` unexpected response, `-4` send failure (see `roller_errcode_t`).

### I2C API (direct, addr 0x64)

Same conceptual ops, no packet framing:
`begin`, `setMode`, `setOutput`, `setSpeed`, `setSpeedMaxCurrent`, `setSpeedPID`, `setPos`, `setPosMaxCurrent`, `setPosPID`, `setCurrent`, `setDialCounter`, `setRGB`, `setRGBBrightness`, `setMotorID`, `setBPS`, `saveConfigToFlash`, `posRangeProtect`, `resetStalledProtect`, `setKeySwitchMode`, `setStallProtection`, `setI2CAddress`, `startAngleCal`, `updateAngleCal`, `getCalBusyStatus`, plus matching getters (`getSpeed`, `getPos`, `getCurrent`, `getVin`, `getTemp`, `getSysStatus`, `getErrorCode`, `getFirmwareVersion`, …).

Examples: `repos/M5Unit-Roller/examples/{rs485,i2c}/motor/motor.ino`.

---

## 8. Safety / operational notes

- **Voltage:** never exceed 16 V on PWR-485. 18 V+ triggers `E:1 Over Voltage`, disables motor, LED red. Clear with `REMOVE_PROTECTION` (`0x06` or I2C reg `0x0B`).
- **Stall/jam protection:** enable via `setJamProtection(id, true)` or reg `0x0F`. Reset after trip.
- **Position over-range:** beyond ±2.1 × 10⁹ encoder counts triggers stop; clear via `REMOVE_PROTECTION`.
- **Continuous current budget:** plan around 0.5 A sustained. Bursting to 1 A is OK short-term; long holds will thermal-limit (monitor `getActualTemp`).
- **Absolute position error:** ±2° due to mechanical assembly. Calibrate zero per unit.
- **Flash wear:** `SAVE_FLASH` writes EEPROM-equivalent cells. Don't call in a loop.

---

## 9. Firmware (factory)

Source: `repos/M5Unit-Roller485-Internal-FW/`.
- `code/Bootloader_g431/` — STM32G431 bootloader
- `code/ROLLER485/` — application

Built on: [smartknob](https://github.com/scottbez1/smartknob) (FOC), [PID_Controller](https://github.com/tcleg/PID_Controller), [u8g2](https://github.com/olikraus/u8g2) (OLED).

Flashing: via SWD (pads broken out on the PCB). Bootloader supports serial update through PWR-485 — see the repo's README and M5Stack's Burner tool.

---

## 10. Reference links

| Link | |
|------|--|
| Product page | https://shop.m5stack.com/products/roller485-lite-unit-without-slip-ring-stm32 |
| Docs wiki | https://docs.m5stack.com/en/unit/Unit-Roller485%20Lite |
| Arduino library | https://github.com/m5stack/M5Unit-Roller |
| Factory firmware | https://github.com/m5stack/M5Unit-Roller485-Internal-FW |
| TLI5012BE datasheet | `processed/TLI5012BE1000.md` |
| DRV8311 datasheet | `processed/DRV8311HRRWR.md` |
| RS-485 protocol | `processed/Unit_Roller485_RS485_Protocol_EN.md` |
| I2C protocol | `processed/Unit_Roller485_I2C_Protocol_EN.md` |
| Schematic | `processed/sch_Unit-Roller485_V1.0.md` + `sources/sch_Unit-Roller485_V1.0.pdf` |
