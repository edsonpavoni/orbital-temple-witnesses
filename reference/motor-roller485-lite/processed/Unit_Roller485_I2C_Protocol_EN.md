## Unit-Roller485

I2C  Control  Protocol

<!-- image -->

## Table of Contents

| ..........................................................................................................................................................                                                                                                                                                                                                                               | ..........................................................................................................................................................                                                                                                                                                                                                                               |
|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
|                                                                                                                                                                                                                                                                                                                                                                                          | 、 Communication Protocol Structure ............................................................................................ 3                                                                                                                                                                                                                                                        |
| 1.1 Communication Protocol Parameters ......................................................................... 3 2 、 Configuration Registers ............................................................................................................... 3                                                                                                                          | 1.1 Communication Protocol Parameters ......................................................................... 3 2 、 Configuration Registers ............................................................................................................... 3                                                                                                                          |
|                                                                                                                                                                                                                                                                                                                                                                                          | Switch(00H) ....................................................................................................... 3                                                                                                                                                                                                                                                                    |
|                                                                                                                                                                                                                                                                                                                                                                                          | 2.1 Mode 2.2 Mode Setting(01H) ...................................................................................................... 3                                                                                                                                                                                                                                                  |
|                                                                                                                                                                                                                                                                                                                                                                                          | 2.3 Motor Over Range Protection(0AH) ............................................................................ 4                                                                                                                                                                                                                                                                      |
|                                                                                                                                                                                                                                                                                                                                                                                          | 2.4 Remove Protection(0BH) ............................................................................................. 4                                                                                                                                                                                                                                                               |
|                                                                                                                                                                                                                                                                                                                                                                                          | 2.5 Motor Status(0CH) ...................................................................................................... 5                                                                                                                                                                                                                                                           |
|                                                                                                                                                                                                                                                                                                                                                                                          | 2.6 Motor Error(0DH) ........................................................................................................ 5                                                                                                                                                                                                                                                          |
|                                                                                                                                                                                                                                                                                                                                                                                          | 2.7 Button Switch Mode(0EH) ........................................................................................... 5                                                                                                                                                                                                                                                                |
|                                                                                                                                                                                                                                                                                                                                                                                          | 2.8 Motor Jam Protection(0FH) ......................................................................................... 6                                                                                                                                                                                                                                                                |
|                                                                                                                                                                                                                                                                                                                                                                                          | 2.9 Device ID (10H) 6                                                                                                                                                                                                                                                                                                                                                                    |
|                                                                                                                                                                                                                                                                                                                                                                                          | ............................................................................................................ 2.10 RS485 Baud Rate(11H) .............................................................................................. 6                                                                                                                                                  |
|                                                                                                                                                                                                                                                                                                                                                                                          | 2.11 RGB LED Brightness(12H) .......................................................................................... 7                                                                                                                                                                                                                                                                |
| 2.12 RGB LED Color(30H) .................................................................................................. 2.13 RGB LED Mode(33H) ................................................................................................. 、 Speed Loop Control Registers ..................................................................................................... | 2.12 RGB LED Color(30H) .................................................................................................. 2.13 RGB LED Mode(33H) ................................................................................................. 、 Speed Loop Control Registers ..................................................................................................... |
|                                                                                                                                                                                                                                                                                                                                                                                          | ...................................................................................................... 8                                                                                                                                                                                                                                                                                 |
| 7                                                                                                                                                                                                                                                                                                                                                                                        | 7                                                                                                                                                                                                                                                                                                                                                                                        |
|                                                                                                                                                                                                                                                                                                                                                                                          | 3.1 Speed Setting(40H)                                                                                                                                                                                                                                                                                                                                                                   |
|                                                                                                                                                                                                                                                                                                                                                                                          | 3.2 Speed Max Current Setting(50H) ................................................................................ 8                                                                                                                                                                                                                                                                    |
|                                                                                                                                                                                                                                                                                                                                                                                          | 3.3 Speed Readback(60H) ................................................................................................. 9                                                                                                                                                                                                                                                              |
|                                                                                                                                                                                                                                                                                                                                                                                          | 3.4 Speed PID Configuration(70H) .................................................................................... 9                                                                                                                                                                                                                                                                  |
|                                                                                                                                                                                                                                                                                                                                                                                          | 、 Position Loop Control Registers ................................................................................................ 10                                                                                                                                                                                                                                                    |
|                                                                                                                                                                                                                                                                                                                                                                                          | 4.1 Position Control(80H) ................................................................................................ 10                                                                                                                                                                                                                                                            |
|                                                                                                                                                                                                                                                                                                                                                                                          | 4.2 Position Max Current Setting(20H) ........................................................................... 10                                                                                                                                                                                                                                                                     |
|                                                                                                                                                                                                                                                                                                                                                                                          | 4.3 Position Readback(90H) ............................................................................................ 10                                                                                                                                                                                                                                                               |
|                                                                                                                                                                                                                                                                                                                                                                                          | 4.4 Position PID Configuration(A0H) ............................................................................... 11                                                                                                                                                                                                                                                                   |
|                                                                                                                                                                                                                                                                                                                                                                                          | 、 Current Loop Control Instruction Set ........................................................................................ 11                                                                                                                                                                                                                                                       |
|                                                                                                                                                                                                                                                                                                                                                                                          | 5.2 Current Readback(C0H) .............................................................................................                                                                                                                                                                                                                                                                  |
|                                                                                                                                                                                                                                                                                                                                                                                          | 12                                                                                                                                                                                                                                                                                                                                                                                       |
|                                                                                                                                                                                                                                                                                                                                                                                          | 、 Status Read Registers ................................................................................................................ 12                                                                                                                                                                                                                                              |
|                                                                                                                                                                                                                                                                                                                                                                                          | 6.1 Power Vin(34H) ......................................................................................................... 12                                                                                                                                                                                                                                                          |
|                                                                                                                                                                                                                                                                                                                                                                                          | 6.2 Internal Temperature(38H) ....................................................................................... 13                                                                                                                                                                                                                                                                 |
|                                                                                                                                                                                                                                                                                                                                                                                          | 6.3 Encoder Counter(3CH) .............................................................................................. 13                                                                                                                                                                                                                                                               |
|                                                                                                                                                                                                                                                                                                                                                                                          | 6.4 Save Flash(F0H) ......................................................................................................... 13                                                                                                                                                                                                                                                         |
|                                                                                                                                                                                                                                                                                                                                                                                          | 6.5 Firmware Version (FEH) ............................................................................................. 14                                                                                                                                                                                                                                                              |

1

## 1 、 Communication Protocol Structure

## 1.1 Communication Protocol Parameters

The I2C communication interface is used.

Default Address is 0x64

Recommended communication speed: 200-400KHz.

## 2 、 Configuration Registers

## 2.1 Mode Switch(00H)

- ⚫

- Function: Motor enable switch

- ⚫ Register Address:00H
- ⚫ Input Parameters:

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        00 | R/W   | 1 byte   | Status      |

Status (1byte) ：

| Parameter   | Function      | description   |
|-------------|---------------|---------------|
| 0x00        | Motor Disable | Motor off     |
| 0x01        | Motor Enable  | Motor on      |

## 2.2 Mode Setting(01H)

- ⚫ Function: Set motor operating mode.

- ⚫ Register Address:00H

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        01 | R/W   | 1 byte   | Mode        |

- ⚫ Input Parameter:

Mode (1byte) ：

| Parameter   | Function   | Description   |
|-------------|------------|---------------|

| 0x01   | Speed Mode    | Speed control loop: controls the motor to run at the target speed.                 |
|--------|---------------|------------------------------------------------------------------------------------|
| 0x02   | Position Mode | Position control loop: controls the motor to rotate to the specified position.     |
| 0x03   | Current Mode  | Current control loop: controls the motor to operate at the target working current. |
| 0x04   | Encoder Mode  | Encoder mode: device acts as an input device to collect current encoder values.    |

## 2.3 Motor Over Range Protection(0AH)

- ⚫ Function: Set motor rotation range protection. When enabled, if the encoder value is &lt; -2100000000 or &gt;2100000000, the motor stops and enters protection state.
- ⚫ Input Parameters:

- ⚫

- Register Address: 0AH

| Address   | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
| 0A        | W     | 1 byte   | Protection  |

Protection (1byte) ：

0 :

Disable rotation range protection

1 :

Enable rotation range protection

## 2.4 Remove Protection(0BH)

- ⚫
- Function: Remove Jam protection. When the jam protection is triggered, send this command to unlock.
- ⚫ Input Parameters:

- ⚫ Register Address: 0BH

| Address   | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
| 0B        | W     | 1 byte   | Status      |

Status (1 byte):

## 1: Remove Jam Protection

## 2.5 Motor Status(0CH)

- ⚫

Function: Motor working status.

- ⚫ Register Address: 0CH

| Address   | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
| 0C        | R     | 1 byte   | Status      |

- ⚫ Input Parameter ：

Status (1byte) ：

0: Standby

1: Running

2: Error

## 2.6 Motor Error(0DH)

- ⚫

Function: Motor error status codes.

- ⚫

- Register Address: 0DH

| Address   | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
| 0D        | R     | 1 byte   | Status      |

- ⚫ Input Parameter ：

Status (1byte) ：

1: Overvoltage

2: Jam

4: Over Range

## 2.7 Button Switch Mode(0EH)

- ⚫

Function: Enable button mode switching.

- ⚫ Register Address: 0EH

| Address   | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
| 0E        | R/W   | 1 byte   | Status      |

- ⚫ Input Parameter ：

Status (1byte) ：

0: Disable button mode switching

1: Enable button mode switching (long press for 5 seconds to switch motor working mode)

## 2.8 Motor Jam Protection(0FH)

- ⚫
- Function: Enable or disable motor jam protection. When jam protection is

triggered, the motor locks and stops rotating. Send the

Remove Protection command to unlock.

- ⚫ Register Address: 0FH

| Address   | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
| 0F        | R/W   | 1 byte   | Status      |

- ⚫ Input Parameters ：

Status (1byte) ：

0: Disable jam protection

1: Enable jam protection

## 2.9 Device ID (10H)

- ⚫

Function: Set device ID.

- ⚫ Register Address: 10H

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        10 | R/W   | 1 byte   | ID          |

- ⚫ Input Parameters:

ID (1byte) ：

0 -

255: New device ID

## 2.10 RS485 Baud Rate(11H)

- ⚫

- Function: Set RS485 communication baud rate.

- ⚫ Register Address: 11H

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        11 | R/W   | 1 byte   | Baud        |

- ⚫ Input Parameters:

Baud(1byte) ：

0 ：

115200 bps

1 ： 19200 bps

2 ： 9600 bps

## 2.11 RGB LED Brightness(12H)

- ⚫

- Function: Control the brightness of the RGB LED.

- ⚫ Register Address: 12H

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        12 | R/W   | 1 byte   | Brightness  |

- ⚫ Input Parameters:

Brightness (1byte) ：

0-100

## 2.12 RGB LED Color(30H)

- ⚫

- Function: Control the color of the RGB LED.

- ⚫ Register Address: 30H

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        30 | R/W   | 3 bytes  | Color       |

- ⚫ Input Parameters:

Color(3byte) ：

Byte0

： RGB-B value

Byte1

： RGB-G value

Byte2

： RGB-R value

## 2.13 RGB LED Mode(33H)

- ⚫ Function: Control the RGB LED operating mode.

- ⚫ Register Address: 33H

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        33 | R/W   | 1 byte   | Mode        |

- ⚫ Input Parameters:

```
Mode(1byte) ： 0: Default system state display 1: User -defined control
```

## 3 、 Speed Loop Control Registers

## 3.1 Speed Setting(40H)

- ⚫ Function: Configure the target speed (RPM)
- ⚫ Register Address: 40H
- ⚫ Parameters:

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        40 | R/W   | 4 bytes  | Speed       |

```
Speed (4byte) ： Speed Setting = Speed Setting-byte0 + Speed Setting-byte1 * 256 + Speed Settingbyte2 * 65536 + Speed Setting-byte3 * 16777216 Actual Speed Setting = Speed Setting / 100 Range: -2100000000 ~ +2100000000
```

## 3.2 Speed Max Current Setting(50H)

- ⚫ Function: Configure the maximum current limit for the target speed.
- ⚫ Register Address: 50H
- ⚫ Parameters:

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        50 | R/W   | 4 bytes  | Max Current |

```
Max Current (4byte) ： Max Current = Max Current-byte0 + Max Current-byte1 * 256 + Max Current-byte2 * 65536 + Max Current-byte3 * 16777216
```

```
Actual Max Current = Max Current / 100 Range: -120000 ~ +120000
```

## 3.3 Speed Readback(60H)

- ⚫ Function: Read the current motor speed value (RPM)
- ⚫ Register Address: 60H
- ⚫ Parameters:

|   Address | R/W   | Length   | Parameter      |
|-----------|-------|----------|----------------|
|        60 | R     | 4 bytes  | Speed Readback |

Speed Readback (4byte) ：

```
Speed Readback = Speed Readback-byte0 + Speed Readback-byte1 * 256 + Speed Readback-byte2 * 65536 + Speed Readback-byte3 * 16777216
```

Actual Speed Readback = Speed Readback / 100

## 3.4 Speed PID Configuration(70H)

- ⚫ Function: Configure PID parameters for the speed control loop.
- ⚫ Register Address: 70H
- ⚫ Parameters:

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        70 | R/W   | 4 bytes  | P           |
|        74 | R/W   | 4 bytes  | I           |
|        78 | R/W   | 4 bytes  | D           |

P / I / D (4 byte) ：

```
PID setting value = PID-byte0 + PID-byte1 * 256 + PID-byte2 * 65536 + PID-byte3 * 16777216 P setting value = P * 10e5 = P * 100000 I setting value = I * 10e7 = I * 10000000 D setting value = D * 10e5 = D * 100000
```

## 4 、 Position Loop Control Registers

## 4.1 Position Control(80H)

- ⚫ Function: Configure the target position.
- ⚫ Register Address: 80H
- ⚫ Parameters:

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        80 | R/W   | 4 bytes  | Position    |

Position (4byte) ：

```
Position Setting = Position Setting-byte0 + Position Setting-byte1 * 256 + Position Setting-byte2 * 65536 + Position Setting-byte3 * 16777216 Actual Position Setting = Position Setting / 100 Range: -2100000000 ~ +2100000000
```

## 4.2 Position Max Current Setting(20H)

- ⚫ Function: Configure the maximum current limit for the target position.
- ⚫ Register Address: 20H
- ⚫ Parameters:

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        20 | R/W   | 4 bytes  | Current     |

```
Max Current (4byte) ： Max Current = Max Current-byte0 + Max Current-byte1 * 256 + Max Current-byte2 * 65536 + Max Current-byte3 * 16777216 Actual Max Current = Max Current / 100 Range: -120000 ~ +120000
```

## 4.3 Position Readback(90H)

- ⚫ Function: Read the current position value.
- ⚫ Register Address: 90H

|   Address | R/W   | Length   | Parameter         |
|-----------|-------|----------|-------------------|
|        90 | R     | 4 bytes  | Position Readback |

- ⚫ Parameters ：

```
Position Readback (4byte) ： Position Readback = Position Readback-byte0 + Position Readback-byte1 * 256 + Position Readback-byte2 * 65536 + Position Readback-byte3 * 16777216 Actual Position Readback = Position Readback / 100
```

## 4.4 Position PID Configuration(A0H)

- ⚫ Function: Configure PID parameters for the position control loop.
- ⚫ Register Address: A0H
- ⚫ Parameters:

| Address   | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
| A0        | R/W   | 4 bytes  | P           |
| A4        | R/W   | 4 bytes  | I           |
| A8        | R/W   | 4 bytes  | D           |

P / I / D (4 byte) ：

```
PID setting value = PID-byte0 + PID-byte1 * 256 + PID-byte2 * 65536 + PID-byte3 * 16777216 P setting value = P * 10e5 = P * 100000 I setting value = I * 10e7 = I * 10000000 D setting value = D * 10e5 = D * 100000
```

## 5 、 Current Loop Control Instruction Set

## 5.1 Current Control(B0H)

- ⚫ Function: Configure the target operating current.
- ⚫ Register Address: B0H

| Address   | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|

| B0   | R/W   | 4 bytes   | Current   |
|------|-------|-----------|-----------|

- ⚫ Parameters ：

## Current (4byte) ：

```
Current Setting = Current Setting-byte0 + Current Setting-byte1 * 256 + Current Setting-byte2 * 65536 + Current Setting-byte3 * 16777216 Actual Current Setting = Current Setting / 100 Range: -120000 ~ +120000
```

## 5.2 Current Readback(C0H)

- ⚫ Function: Read the current operating current.
- ⚫ Register Address: C0H
- ⚫ Parameters:

| Address   | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
| C0        | R     | 4 bytes  | Current     |

## Current (4byte) ：

Current Readback = Current Readback-byte0 + Current Readback-byte1 * 256 + Current Readback-byte2 * 65536 + Current Readback-byte3 * 16777216

```
Actual Current Readback = Current Readback / 100
```

## 6 、 Status Read Registers

## 6.1 Power Vin(34H)

- ⚫ Function: Read the current input voltage value of the motor (V). The read value is VIN*100. Refer to the formula below to calculate the actual input voltage.
- ⚫ Parameters:

- ⚫ Register Address: 34H

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        34 | R/W   | 4 bytes  | VIN         |

VIN(4byte) ：

```
VIN X100 = VIN X100-byte0 + VIN X100-byte1 * 256 + VIN X100-byte2 * 65536 + VIN X100-byte3 * 16777216 Actual VIN = VIN X100 / 100
```

## 6.2 Internal Temperature(38H)

- ⚫
- Function: Internal reference temperature value register (°C). The

temperature is for internal status reference only and may not be accurate.

- ⚫ Register Address: 38H
- ⚫ Parameters:

|   Address | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
|        38 | R/W   | 4 bytes  | Temperature |

Temperature(4byte) ： Temp = Temp-byte0 + Temp-byte1 * 256 + Temp-byte2 * 65536 +

```
Temp-byte3 * 16777216
```

## 6.3 Encoder Counter(3CH)

- ⚫ Function: In encoder operating mode, read the encoder value.
- ⚫ Register Address: 3CH
- ⚫ Parameters:

| Address   | R/W   | Length   | Parameter       |
|-----------|-------|----------|-----------------|
| 3C        | R/W   | 4 bytes  | Encoder Counter |

Encoder Counter (4byte) ：

```
Encoder Counter = Encoder Counter-byte0 + Encoder Counter-byte1 * 256 + Encoder Counter-byte2 * 65536 + Encoder Counter-byte3 * 16777216
```

## 6.4 Save Flash(F0H)

- ⚫ Function: Save parameters to flash.
- ⚫ Register Address: F0H
- ⚫ Parameters:

| Address   | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
| F0        | W     | 1 byte   | Save        |

Save (1byte)

： 1

## 6.5 Firmware Version (FEH)

- ⚫ Function: Firmware version register.

- ⚫ Register Address: FEH

| Address   | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
| FE        | R     | 1 byte   | Version     |

- ⚫ Parameters:

Version(1byte) ：

1-127

## 6.6 I2C Address (FFH)

- ⚫

- Function: Change I2C address.

- ⚫

- Register Address: FFH

| Address   | R/W   | Length   | Parameter   |
|-----------|-------|----------|-------------|
| FF        | R/W   | 1 byte   | Address     |

- ⚫ Parameters:

Address(1byte) ：

1-127