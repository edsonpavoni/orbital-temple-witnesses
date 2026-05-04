## Unit-Roller485

## RS485  Control  Protocol

<!-- image -->

7.4 I2C Write Raw

## Table of Contents

| .................................................................................................................................................... 1   | .................................................................................................................................................... 1   |
|----------------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1 、                                                                                                                                                      | Communication Protocol Structure ........................................................................................ 3                              |
|                                                                                                                                                          | 1.1 Communication Protocol Parameters ...................................................................... 3                                           |
|                                                                                                                                                          | 1.1.1 Communication Interface Parameters ........................................................... 3                                                   |
|                                                                                                                                                          | 1.2 Data Packet Format ................................................................................................. 3                               |
|                                                                                                                                                          | 1.2.1 Data Packet Format ...................................................................................... 3                                        |
|                                                                                                                                                          | 1.2.2 Configuration Command Response Frame .................................................... 4                                                        |
|                                                                                                                                                          | 1.2.3 CRC8 ............................................................................................................ 4                                |
| 2 、 Configuration Command Set .................................................................................................. 5                       | 2 、 Configuration Command Set .................................................................................................. 5                       |
|                                                                                                                                                          | 2.1 Mode Switch ........................................................................................................... 5                            |
|                                                                                                                                                          | 2.2 Mode Setting .......................................................................................................... 5                            |
|                                                                                                                                                          | 2.3 Remove Protection ................................................................................................. 6                                |
|                                                                                                                                                          | 2.4 Save to Flash ........................................................................................................... 7                          |
|                                                                                                                                                          | 2.5 Encoder .................................................................................................................. 8                         |
|                                                                                                                                                          | 2.6 Button Switch Mode ............................................................................................... 9                                 |
|                                                                                                                                                          | 2.7 RGB LED Control ................................................................................................... 10                               |
|                                                                                                                                                          | 2.8 RS485 Baud Rate ................................................................................................... 11                               |
|                                                                                                                                                          | 2.9 Device ID .............................................................................................................. 11                          |
|                                                                                                                                                          | 2.10 Motor Jam Protection ......................................................................................... 12                                   |
|                                                                                                                                                          | 2.11 Motor Position Over Range Protection ................................................................ 13                                            |
| 3 、 Speed Loop Control Instruction Set ......................................................................................                            | 3 、 Speed Loop Control Instruction Set ......................................................................................                            |
|                                                                                                                                                          | 3.1 Speed Control ....................................................................................................... 14                             |
|                                                                                                                                                          | 3.2 Speed PID Configuration ....................................................................................... 15                                   |
| 4 、 Position Loop Control Instruction Set ................................................................................... 16                         | 4 、 Position Loop Control Instruction Set ................................................................................... 16                         |
|                                                                                                                                                          | 4.1 Position Control .................................................................................................... 16                             |
|                                                                                                                                                          | 4.2 Position PID Configuration .................................................................................... 17                                   |
| 5 、 Current Loop Control Instruction Set .................................................................................... 18                         | 5 、 Current Loop Control Instruction Set .................................................................................... 18                         |
|                                                                                                                                                          | 5.1 Current Control ..................................................................................................... 18                             |
| 6 、                                                                                                                                                      | Status Readback Instruction Set ......................................................................................... 19                             |
|                                                                                                                                                          | 6.1 Motor Status Readback ......................................................................................... 19                                   |
|                                                                                                                                                          | 6.2 Other Status Readback .......................................................................................... 21                                  |
| 7 、                                                                                                                                                      | RS485 to I2C Forwarding Control Instruction Set ................................................................ 22                                      |
|                                                                                                                                                          | 7.1 I2C Read Register .................................................................................................. 22                              |
|                                                                                                                                                          | 7.2 I2C Write Register ................................................................................................. 23                              |
|                                                                                                                                                          | 7.3 I2C Read Raw ........................................................................................................ 24                             |

.......................................................................................................

25

## 1 、 Communication Protocol Structure

## 1.1 Communication Protocol Parameters

## 1.1.1 Communication Interface Parameters

Half - duplex asynchronous serial communication is used.

Default baud rate: 115200 bps (other baud rates can be customized).

Data format: 8

- bit data (LSB first) with 1 stop bit, no parity bit.

## 1.2 Data Packet Format

Both  the  send  frame  and  response  frame  have  a  fixed  length  of  15  bytes.  The response  frame  starts  with  0xAA  0x55,  and  0xAA  0x55  is  not  included  in  the  CRC checksum calculation.

## 1.2.1 Data Packet Format

| Command      | Device ID   | Data1                                                                                                                                 | Data2                                                                                                                                 | Data3                                                                                                                                 | CRC8                                                                                                                                  |
|--------------|-------------|---------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------|
| 1 byte       | 1 byte      | 4 bytes                                                                                                                               | 4 bytes                                                                                                                               | 4 bytes                                                                                                                               | 1 byte                                                                                                                                |
| Explanation: | Command     | Configuration command.                                                                                                                | Configuration command.                                                                                                                | Configuration command.                                                                                                                | Configuration command.                                                                                                                |
| Explanation: | Device ID   | Default is 0x00, range 0x00 - 0xFF.                                                                                                   | Default is 0x00, range 0x00 - 0xFF.                                                                                                   | Default is 0x00, range 0x00 - 0xFF.                                                                                                   | Default is 0x00, range 0x00 - 0xFF.                                                                                                   |
| Explanation: | Data1 - 3   | Each data group is 4 bytes, unused bits can be set to 0.                                                                              | Each data group is 4 bytes, unused bits can be set to 0.                                                                              | Each data group is 4 bytes, unused bits can be set to 0.                                                                              | Each data group is 4 bytes, unused bits can be set to 0.                                                                              |
| Explanation: | CRC8        | Refer to the code below for the CRC calculation. All returned messages start with 0xAA 0x55, which are not included in the CRC check. | Refer to the code below for the CRC calculation. All returned messages start with 0xAA 0x55, which are not included in the CRC check. | Refer to the code below for the CRC calculation. All returned messages start with 0xAA 0x55, which are not included in the CRC check. | Refer to the code below for the CRC calculation. All returned messages start with 0xAA 0x55, which are not included in the CRC check. |

## 1.2.2 Configuration Command Response Frame

| Command     | Device ID   | Data1                                                                                                                                              | Data2                                                                                                                                              | Data3                                                                                                                                              | CRC8                                                                                                                                               |
|-------------|-------------|----------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------|
| 1 byte      | 1 byte      | 4 bytes                                                                                                                                            | 4 bytes                                                                                                                                            | 4 bytes                                                                                                                                            | 1 byte                                                                                                                                             |
| Explanation | Command     | Response command                                                                                                                                   | Response command                                                                                                                                   | Response command                                                                                                                                   | Response command                                                                                                                                   |
| Explanation | Device ID   | The default device ID is 0x00, and the supported range is 0x00 - 0xFF.                                                                             | The default device ID is 0x00, and the supported range is 0x00 - 0xFF.                                                                             | The default device ID is 0x00, and the supported range is 0x00 - 0xFF.                                                                             | The default device ID is 0x00, and the supported range is 0x00 - 0xFF.                                                                             |
| Explanation | Data1 - 3   | Each data group is 4 bytes, and unused data bits can be set to 0.                                                                                  | Each data group is 4 bytes, and unused data bits can be set to 0.                                                                                  | Each data group is 4 bytes, and unused data bits can be set to 0.                                                                                  | Each data group is 4 bytes, and unused data bits can be set to 0.                                                                                  |
| Explanation | CRC8        | Refer to the following code for CRC value calculation. All returned messages start with 0xAA 0x55, and 0xAA 0x55 is not included in the CRC check. | Refer to the following code for CRC value calculation. All returned messages start with 0xAA 0x55, and 0xAA 0x55 is not included in the CRC check. | Refer to the following code for CRC value calculation. All returned messages start with 0xAA 0x55, and 0xAA 0x55 is not included in the CRC check. | Refer to the following code for CRC value calculation. All returned messages start with 0xAA 0x55, and 0xAA 0x55 is not included in the CRC check. |

## 1.2.3 CRC8

```
uint8_t crc8(uint8_t * data , uint8_t len ) { uint8_t crc, i; crc = 0x00; while ( len --) { crc ^= * data ++; for (i = 0; i < 8; i++) { if (crc & 0x01) { crc = (crc >> 1) ^ 0x8c; } else crc >>= 1; } } return crc; }
```

## 2 、 Configuration Command Set

## 2.1 Mode Switch

- ⚫

- Function: Motor enable switch.

- ⚫ Input Parameter ：

Motor ID(1 byte)

： Device address.

Status(1 byte) ：

| Parameter   | Function      | description   |
|-------------|---------------|---------------|
| 0x00        | Motor Disable | Motor off     |
| 0x01        | Motor Enable  | Motor on      |

- ⚫ Command Code: 00H

- ⚫ Command Packet Format:

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x00      | Motor ID    | Status  | Reserve | Reserve | CRC8   |

## ⚫ Response Packet Format:

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x10      | Motor ID    | Status  | Reserve | Reserve | CRC8   |

## ⚫ Example of Send Packet:

00 00 01 00 00 00 00 00 00 00 00 00 00 00 68

- ⚫ Example of Response Packet:

10 00 01 00 00 00 00 00 00 00 00 00 00 00 9A

## 2.2 Mode Setting

- ⚫

- Function: Motor operating mode setting.

- ⚫ Input Parameter ：

Motor ID(1 byte) ： Device address.

Mode(1 byte) ：

| Parameter   | Function      | Description                                                                          |
|-------------|---------------|--------------------------------------------------------------------------------------|
| 0x01        | Speed Mode    | Speed control mode: The motor runs at the specified target speed.                    |
| 0x02        | Position Mode | Position control mode: The motor moves to the specified position.                    |
| 0x03        | Current Mode  | Current control mode: The motor runs with the specified target current.              |
| 0x04        | Encoder Mode  | Encoder mode: The device acts as an input device, collecting current encoder values. |

- ⚫ Command Code ：

01H

## ⚫ Command Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x01      | Motor ID    | Mode    | Reserve | Reserve | CRC8   |

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x11      | Motor ID    | Mode    | Reserve | Reserve | CRC8   |

## ⚫ Example of Send Packet ：

01 00 01 00 00 00 00 00 00 00 00 00 00 00 44

## ⚫ Example of Response Packet ：

11 00 01 00 00 00 00 00 00 00 00 00 00 00 B6

## 2.3 Remove Protection

- ⚫ Function ： Unlock Jam protection. When Jam lock protection is triggered, send this command to unlock.
- ⚫ Input Parameter ：
- ⚫ Command Packet Format ：
- ⚫ Response Packet Format ：
- ⚫ Example of Send Packet ：

```
Motor ID(1 byte) ： Device address Status (1 byte) ： 1 ： Remove Jam Protection
```

- ⚫ Command Code

- ：

06H

| Command   | Device ID   | Data1   | Data2   | Data3    | CRC8   |
|-----------|-------------|---------|---------|----------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytees | 1 byte |
| 0x06      | Motor ID    | Reserve | Status  | Reserve  | CRC8   |

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x16      | Motor ID    | Reserve | Status  | Reserve | CRC8   |

06 00 00 00 00 00 01 00 00 00 00 00 00 00 AB

- ⚫ Example of Response Packet ： 16 00 00 00 00 00 00 00 00 00 00 00 00 00 1A

## 2.4 Save to Flash

- ⚫ Function ： Save configuration parameters to internal Flash.
- ⚫ Input Parameter ：

Motor ID(1 byte) ： Device address

```
Status (1 byte) ： 1 ： Execute save
```

- ⚫ Command Code ：

07H

## ⚫ Command Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x07      | Motor ID    | Status  | Reserve | Reserve | CRC8   |

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x17      | Motor ID    | Status  | Reserve | Reserve | CRC8   |

- ⚫ Example of Send Packet ：

07 00 01 00 00 00 00 00 00 00 00 00 00 00 AC

- ⚫ Example of Response Packet ：

17 00 01 00 00 00 00 00 00 00 00 00 00 00 5E

## 2.5 Encoder

- ⚫ Function ： Set the current encoder value.
- ⚫ Input Parameter ：

Motor ID(1 byte) ： Device address

Encoder(4 byte) ：

Encoder value(int32\_t) Encoder = Encoder-byte0 + Encoder-byte1

```
* 256 + Encoder-byte2 * 65536 + Encoder-byte3 * 16777216
```

- ⚫ Command Code ：

08H

- ⚫ Command Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x08      | Motor ID    | Encoder | Reserve | Reserve | CRC8   |

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |

| 0x18   | Motor ID   | Encoder   | Reserve   | Reserve   | CRC8   |
|--------|------------|-----------|-----------|-----------|--------|

- ⚫ Example of Send Packet ：

08 00 64 00 00 00 00 00 00 00 00 00 00 00 06

- ⚫ Example of Response Packet ：

18 00 64 00 00 00 00 00 00 00 00 00 00 00 F4

## 2.6 Button Switch Mode

- ⚫ Function ： Enable button mode switching.
- ⚫ Input Parameter ：

Motor ID(1 byte) ： Device address

Status (1 byte) ：

0 ： Disable button mode switching

1 ： Enable button mode switching (press for 5 seconds  to switch motor working mode)

- ⚫ Command Code ：

09H

- ⚫ Command Packet Format ：
- ⚫ Response Packet Format ：
- ⚫ Example of Send Packet ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x09      | Motor ID    | Status  | Reserve | Reserve | CRC8   |

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x19      | Motor ID    | Status  | Reserve | Reserve | CRC8   |

09 00 01 00 00 00 00 00 00 00 00 00 00 00 3D

- ⚫ Example of Response Packet ：

19 00 01 00 00 00 00 00 00 00 00 00 00 00 CF

## 2.7 RGB LED Control

- ⚫ Function ： Control RGB LED color, brightness, and working mode.
- ⚫ Input Parameter ：
- ⚫ Command Packet Format ：

```
Motor ID(1 byte) ： Device address Status (4 byte) ： Byte0 ： RGB-R value Byte1 ： RGB-G value Byte2 ： RGB-B value Byte3 ： RGB Mode: 0: Default system state display 1: User-defined control Brightness (1 byte) ： 0-100: Brightness value
```

- ⚫ Command Code ：

0AH

| Command   | Device ID   | Data1   | Data2      | Data3   | CRC8   |
|-----------|-------------|---------|------------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes    | 4 bytes | 1 byte |
| 0x0A      | Motor ID    | Status  | Brightness | Reserve | CRC8   |

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1   | Data2      | Data3   | CRC8   |
|-----------|-------------|---------|------------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes    | 4 bytes | 1 byte |
| 0x1A      | Motor ID    | Status  | Brightness | Reserve | CRC8   |

- ⚫ Example of Send Packet ：

0A 00 FF 32 32 01 C8 00 00 00 00 00 00 00 3C

- ⚫ Example of Response Packet ：

1A 00 FF 32 32 01 C8 00 00 00 00 00 00 00 CE

## 2.8 RS485 Baud Rate

- ⚫ Function ： Configure the baud rate of the RS485 communication interface.
- ⚫ Input Parameter ：

Motor ID(1 byte) ： Device address

Baud (1 byte) ：

0 ： 115200 bps

1 ： 19200 bps

2 ： 9600 bps

- ⚫ Command Code ：

0BH

- ⚫ Command Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x0B      | Motor ID    | Baud    | Reserve | Reserve | CRC8   |

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x1B      | Motor ID    | Baud    | Reserve | Reserve | CRC8   |

- ⚫ Example of Send Packet ：

0B 00 00 00 00 00 00 00 00 00 00 00 00 00 0D

- ⚫ Example of Response Packet ：

1B 00 00 00 00 00 00 00 00 00 00 00 00 00 FF

## 2.9 Device ID

- ⚫ Function ： Configure the device ID.
- ⚫ Input Parameter ：

Motor ID(1 byte) ： Device address

```
ID (1 byte) ： 0-255 ： New device ID
```

- ⚫ Command Code ：

0CH

- ⚫ Command Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x0C      | Motor ID    | ID      | Reserve | Reserve | CRC8   |

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x1C      | Motor ID    | ID      | Reserve | Reserve | CRC8   |

- ⚫ Example of Send Packet ：

0C 00 01 00 00 00 00 00 00 00 00 00 00 00 A1

- ⚫ Example of Response Packet ： 1C 00 01 00 00 00 00 00 00 00 00 00 00 00 53

## 2.10 Motor Jam Protection

- ⚫ Function ： Enable motor jam protection. When jam protection is triggered, the motor will stop and lock. You will need to send the ' Remove Protection ' command to unlock the motor.
- ⚫ Input Parameter ：

```
Motor ID(1 byte) ： Device address Protection (1 byte) ： 0 ： Disable jam protection 1 ： Enable jam protection
```

- ⚫ Command Code

- ：

0DH

- ⚫ Command Packet Format ：

| Command   | Device ID   | Data1      | Data2   | Data3   | CRC8   |
|-----------|-------------|------------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes    | 4 bytes | 4 bytes | 1 byte |
| 0x0D      | Motor ID    | Protection | Reserve | Reserve | CRC8   |

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1      | Data2   | Data3   | CRC8   |
|-----------|-------------|------------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes    | 4 bytes | 4 bytes | 1 byte |
| 0x1D      | Motor ID    | Protection | Reserve | Reserve | CRC8   |

- ⚫ Example of Send Packet ：

0D 00 01 00 00 00 00 00 00 00 00 00 00 00 8D

- ⚫ Example of Response Packet ：

1D 00 01 00 00 00 00 00 00 00 00 00 00 00 7F

## 2.11 Motor Position Over Range Protection

- ⚫ Function ： Configure motor position range protection. When enabled, the motor  will  stop  and  enter  protection  mode  if  the  encoder  value is below -2,100,000,000 or above 2,100,000,000.
- ⚫ Input Parameter ：

Motor ID(1 byte) ： Device address

Protection (1 byte) ：

- 0 ： Disable position range protection
- 1 ： Enable position range protection
- ⚫ Command Packet Format ：

- ⚫ Command Code ：

0EH

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |

| 0x0E   | Motor ID   | Protection   | Reserve   | Reserve   | CRC8   |
|--------|------------|--------------|-----------|-----------|--------|

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1      | Data2   | Data3   | CRC8   |
|-----------|-------------|------------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes    | 4 bytes | 4 bytes | 1 byte |
| 0x1E      | Motor ID    | Protection | Reserve | Reserve | CRC8   |

- ⚫ Example of Send Packet ：

0E 00 01 00 00 00 00 00 00 00 00 00 00 00 F9

- ⚫ Example of Response Packet ：

1E 00 01 00 00 00 00 00 00 00 00 00 00 00 0B

## 3 、 Speed Loop Control Instruction Set

## 3.1 Speed Control

- ⚫ Function ： Configure the target running speed and maximum current limit. ⚫ Input Parameter ： Motor ID(1 byte) ： Device address Speed (4 byte) ： Speed Setting(int32\_t) = Speed Setting-byte0 + Speed Setting-byte1 * 256 + Speed Settingbyte2 * 65536 + Speed Setting-byte3 * 16777216 Actual Speed Setting (RPM) = Speed Setting / 100 Supported input range: -2100000000~+2100000000 Current (4 byte) ： Max Current = Max Current-byte0 + Max Current-byte1 * 256 + Max Current-byte2 * 65536 + Max Current-byte3 * 16777216 Actual Max Current (mA) = Max Current / 100 Supported input range: -120000~+120000
- ⚫ Command Code ： 20H

## ⚫ Command Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x20      | Motor ID    | Speed   | Current | Reserve | CRC8   |

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x30      | Motor ID    | Speed   | Current | Reserve | CRC8   |

- ⚫ Example of Send Packet ：

20 00 80 A9 03 00 C0 D4 01 00 00 00 00 00 7C

- ⚫ Example of Response Packet ： 30 00 80 A9 03 00 C0 D4 01 00 00 00 00 00 8E

## 3.2 Speed PID Configuration

- ⚫ Function ： Configure speed loop PID parameters.

```
⚫ Input Parameter ： Motor ID(1 byte) ： Device address Speed (4 byte) ： P/I/D(uint32_t): PID = PID-byte0 + PID-byte1 * 256 + PID-byte2 * 65536 + PID-byte3 * 16777216 P setting value = P * 10e5 = P * 100000 I setting value = I * 10e7 = I * 10000000 D setting value = D * 10e5 = D * 100000
```

- ⚫ Command Code

- ：

21H

- ⚫ Command Packet Format ：
- ⚫ Response Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x21      | Motor ID    | P       | I       | D       | CRC8   |

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x31      | Motor ID    | P       | I       | D       | CRC8   |

- ⚫ Example of Send Packet ：
- 21 00 60 E3 16 00 E8 03 00 00 00 5A 62 02 D8
- ⚫ Example of Response Packet ：

31 00 60 E3 16 00 E8 03 00 00 00 5A 62 02 2A

## 4 、 Position Loop Control Instruction Set

## 4.1 Position Control

- ⚫ Function ： Configure the number of pulses to control the target rotation to the specified position, while also setting a maximum current limit
- ⚫ Input Parameter ：

```
Motor ID(1 byte) ： Device address Position (4 byte) ： Position Setting(int32_t) = Position Setting-byte0 + Position Setting-byte1 * 256 + Position Setting-byte2 * 65536 + Position Setting-byte3 * 16777216 Actual Position Setting = Position Setting / 100 Input Range: -2100000000 ~ +2100000000 Current (4 byte) ： Max Current = Max Current-byte0 + Max Current-byte1 * 256 + Max Current-byte2 * 65536 + Max Current-byte3 * 16777216 Actual Max Current = Max Current / 100 Support Input Range: -120000 ~ +120000
```

- ⚫ Command Code ：

22H

- ⚫ Command Packet Format ：

| Command   | Device ID   | Data1    | Data2   | Data3   | CRC8   |
|-----------|-------------|----------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes  | 4 bytes | 4 bytes | 1 byte |
| 0x22      | Motor ID    | Position | Current | Reserve | CRC8   |

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1    | Data2   | Data3   | CRC8   |
|-----------|-------------|----------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes  | 4 bytes | 4 bytes | 1 byte |
| 0x32      | Motor ID    | Position | Current | Reserve | CRC8   |

- ⚫ Example of Send Packet ：

22 00 60 E3 16 00 C0 D4 01 00 00 00 00 00 67

- ⚫ Example of Response Packet ：

32 00 60 E3 16 00 C0 D4 01 00 00 00 00 00 95

## 4.2 Position PID Configuration

- ⚫ Function ： Configure the PID parameters of the position loop. ⚫ Input Parameter ： Motor ID(1 byte) ： Device address Speed (4 byte) ： P/I/D(uint32\_t): PID = PID-byte0 + PID-byte1 * 256 + PID-byte2 * 65536 + PID-byte3 * 16777216 P setting value = P * 10e5 = P * 100000 I setting value = I * 10e7 = I * 10000000 D setting value = D * 10e5 = D * 100000
- ⚫ Command Packet Format ：

- ⚫ Command Code

- ： 23H

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |

| 0x23   | Motor ID   | P   | I   | D   | CRC8   |
|--------|------------|-----|-----|-----|--------|

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x33      | Motor ID    | P       | I       | D       | CRC8   |

- ⚫ Example of Send Packet ：

23 00 60 E3 16 00 1E 00 00 00 00 5A 62 02 73

- ⚫ Example of Response Packet ：

33 00 60 E3 16 00 1E 00 00 00 00 5A 62 2 81

## 5 、 Current Loop Control Instruction Set

## 5.1 Current Control

- ⚫ Function ： Configure the target running current.
- ⚫ Command Packet Format ：

```
⚫ Input Parameter ： Motor ID (1 byte) ： Device address Current (4 byte) ： Current Setting(int32_t) = Current Setting-byte0 + Current Setting-byte1 * 256 + Current Setting-byte2 * 65536 + Current Setting-byte3 * 16777216 Actual Current Setting = Current Setting / 100 Input Range: -120000 ~ +120000
```

- ⚫ Command Code

- ： 24H

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x24      | Motor ID    | Current | Reserve | Reserve | CRC8   |

- ⚫ Response Packet Format ：
- ⚫ Example of Send Packet ： 24 00 C0 D4 01 00 00 00 00 00 00 00 00 00 54 ⚫ Example of Response Packet ： 34 00 C0 D4 01 00 00 00 00 00 00 00 00 00 A6

| Command   | Device ID   | Data1   | Data2   | Data3   | CRC8   |
|-----------|-------------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes | 1 byte |
| 0x34      | Motor ID    | Current | Reserve | Reserve | CRC8   |

## 6 、 Status Readback Instruction Set

The format of the request frame and response  frame for reading status differs from the configuration command set, with an additional 3 bytes used to represent operating mode, motor status, and error code.

## 6.1 Motor Status Readback

- ⚫ Function ： Read motor's current speed value, encoder position value, current value, operating mode, motor status, and error code information.
- ⚫ Input Parameter
- ⚫

```
Speed Readback = Speed Readback-byte0 + Speed Readback-byte1 * 256 + Speed Readback-byte2 * 65536 + Speed Readback-byte3 * 16777216 Actual Speed Readback = Speed Readback / 100 Position(4 byte) ：
```

```
： Motor ID (1 byte) ： Device address Read (1 byte) ： 0 Return Parameter ： Speed (4 byte) ： Current motor speed(RPM) Current motor position
```

Position Readback = Position Readback-byte0 + Position Readback-byte1 * 256 + Position Readback-byte2 * 65536 + Position Readback-byte3 * 16777216 Readback-byte1 * 256 + Current Readback-byte2 * 65536 + Current

Actual Position Readback = Position Readback / 100 Current(4 byte) ： Current motor current(mA) Current Readback = Current Readback-byte0 + Current Readback-byte3 * 16777216 Actual Current Readback = Current Readback / 100 Mode(1 byte) ： 1: Speed Mode 2: Position Mode 3: Current Mode 4: Encoder Mode Status(1 byte) ： Current motor status 0: Standby 1: Running 2: Error Error(1 byte) ： 1: Overvoltage 2: Stalled 4: Over Range

## ⚫ Command Code ： 40H

## ⚫ Command Packet Format ：

| Command   | Device ID   | Data1   | CRC8   |
|-----------|-------------|---------|--------|
| 1 byte    | 1 byte      | 1 byte  | 1 byte |
| 0x40      | Motor ID    | Read    | CRC8   |

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1   | Data2    | Data3   | Data4   | Data5   | Data6   | CRC8   |
|-----------|-------------|---------|----------|---------|---------|---------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes  | 4 bytes | 1 byte  | 1 byte  | 1 byte  | 1 byte |
| 0x50      | Motor ID    | Speed   | Position | Current | Mode    | Status  | Error   | CRC8   |

- ⚫ Example of Send Packet ： 40 00 00 31 ⚫ Example of Response Packet ： 50 00 01 00 00 00 78 FB FF FF F7 FF FF FF 01 00 00 8B

## 6.2 Other Status Readback

- ⚫ Function ： Read the motor's current input voltage, temperature value, current value, operating mode, motor status, and error code information.
- ⚫ Input Parameter ：
- ⚫

```
Motor ID(1 byte) ： Device address Read (1 byte) ： 0 Return Parameter ： VIN (4 byte) ： Input voltage value(V) VIN X100 = VIN X100-byte0 + VIN X100-byte1 * 256 + VIN X100-byte2 * 65536 + VIN X100-byte3 * 16777216 Actual VIN = VIN X100 / 100 Temp(4 byte) ： Internal reference temperature value( ℃ ) T e mp   =  Temp-byte0 + Temp-byte1 * 256 + Temp-byte2 * 65536 + Temp-byte3 * 16777216 Encoder Counter(4 byte) ： Encoder  counter  value  (used  in  Encoder  mode) E n co d er  Counter  =  Encoder  Counter-byte0  +  Encoder  Counter-byte1 * 256 + Encoder Counter-byte2 * 65536 + Encoder Counter-byte3 * 16777216 RGB Mode(1 byte) ： RGB mode 0: Default system display 1: User-defined control RGB Brightness(1 byte) ： RGB brightness
```

```
0-100: Brightness value
```

- ⚫ Command Code ：

41H

- ⚫ Command Packet Format ：
- ⚫ Response Packet Format ：
- ⚫ Example of Send Packet ：
- 41 00 00 9A
- ⚫ Example of Response Packet ：
- 51 00 1D 05 00 00 2B 00 00 00 00 00 00 00 01 64 00 CD

| Command   | Device ID   | Data1   | CRC8   |
|-----------|-------------|---------|--------|
| 1 byte    | 1 byte      | 1 byte  | 1 byte |
| 0x41      | Motor ID    | Read    | CRC8   |

| Command   | Device ID   | Data1   | Data2   | Data3           | Data4    | Data5          | Data6   | CRC8   |
|-----------|-------------|---------|---------|-----------------|----------|----------------|---------|--------|
| 1 byte    | 1 byte      | 4 bytes | 4 bytes | 4 bytes         | 1 byte   | 1 byte         | 1 byte  | 1 byte |
| 0x51      | Motor ID    | VIN     | Temp    | Encoder Counter | RGB Mode | RGB Brightness | Reserve | CRC8   |

## 7 、 RS485 to I2C Forwarding Control Instruction Set

This instruction set is used to send control instructions via the RS485 interface to achieve data reading and writing of the I2C port on the Roller485 motor.

## 7.1 I2C Read Register

- ⚫ Function ：
- ⚫ Input Parameter

```
I2C register reading command. ：
```

```
Motor ID(1 byte) ： Device address I2C Address (1 byte) ： Slave device address Register Address Length (1 byte) ： 0 ： The requested register address is 1 byte long. 1 ： The requested register address is 2 bytes long. Register (2 byte) ： Register address
```

Data Length (1 byte) ： Requested  data  length  to  be  read  (maximum  support for 16 bytes)

- ⚫ Return Parameter ：

Read Status (1 byte) ：

1 ： Read successful

0 ： Read failed

Data Length (1 byte) ： Requested data length

Data(16 byte) ： Requested data

- ⚫ Command Code
- ：

60H

- ⚫ Command Packet Format ：
- ⚫ Response Packet Format ：
- ⚫ Example of Send Packet ：

| Command   | Device ID   | Data1       | Data2                   | Data3            | Data4       | CRC8   |
|-----------|-------------|-------------|-------------------------|------------------|-------------|--------|
| 1 byte    | 1 byte      | 1 byte      | 1 byte                  | 2 bytes          | 1 byte      | 1 byte |
| 0x60      | Motor ID    | I2C Address | Register Address Length | Register Address | Data Length | CRC8   |

| Command   | Device ID   | Data1       | Data2   | Data3       | Data4   | Data5    | CRC8   |
|-----------|-------------|-------------|---------|-------------|---------|----------|--------|
| 1 byte    | 1 byte      | 1 byte      | 1 byte  | 1 byte      | 3 bytes | 16 bytes | 1 byte |
| 0x70      | Motor ID    | Read Status | Reserve | Data Length | Reserve | Data     | CRC8   |

60 00 29 00 14 00 0C 54

- ⚫ Example of Response Packet ：

70 00 01 00 0C 00 00 00 5F 06 05 00 FF FF 0B A6 00 00 00 4E 00 00 00 00 DC

## 7.2 I2C Write Register

- ⚫ Function ： I2C register writing command.
- ⚫ Input Parameter ：

Motor ID(1 byte) ： Device address

I2C Address (1 byte) ： Slave device address

Register Address Length (1 byte) ：

0 ： The register address is 1 byte long.

1 ： The register address is 2 bytes long.

Register (2 byte) ： Register address

Data  Length  (1  byte) ： Data  length  to  write  (maximum  support  for  16  bytes)

Data (16 byte) ： Data to write

- ⚫ Return Parameter ：

Write Status (1 byte) ：

1 ： Write successful

0 ： Write failed

- ⚫ Command Code

- ：

61H

- ⚫ Command Packet Format ：

| Command   | Device ID   | Data1       | Data2                   | Data3            | Data4       | Data5   | Data6    | CRC8   |
|-----------|-------------|-------------|-------------------------|------------------|-------------|---------|----------|--------|
| 1 byte    | 1 byte      | 1 byte      | 1 byte                  | 2 bytes          | 1 byte      | 3 bytes | 16 bytes | 1 byte |
| 0x61      | Motor ID    | I2C Address | Register Address Length | Register Address | Data Length | Reserve | Data     | CRC8   |

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1        | CRC8   |
|-----------|-------------|--------------|--------|
| 1 byte    | 1 byte      | 1 byte       | 1 byte |
| 0x71      | Motor ID    | Write Status | CRC8   |

- ⚫ Example of Send Packet ：

61 00 26 00 11 00 01 00 FF 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 15

- ⚫ Example of Response Packet ：

71 00 01 1A

## 7.3 I2C Read Raw

- ⚫ Function ： I2C raw data reading command.
- ⚫ Input Parameter ：

Motor ID(1 byte) ： Device address

I2C Address (1 byte) ： Slave device address

Data Length (1 byte) ： Requested  data  length  to  be  read  (maximum  support for 16 bytes)

- ⚫ Return Parameter ：

Read Status (1 byte) ：

1 ： Read successful

0 ： Read failed

Data Length (1 byte) ： Requested data length

Data(16 byte) ： Requested data

- ⚫ Command Code

- ：

62H

- ⚫ Command Packet Format ：
- ⚫ Response Packet Format ：
- ⚫ Example of Send Packet ：

| Command   | Device ID   | Data1       | Data2       | CRC8   |
|-----------|-------------|-------------|-------------|--------|
| 1 byte    | 1 byte      | 1 byte      | 1 byte      | 1 byte |
| 0x62      | Motor ID    | I2C Address | Data Length | CRC8   |

| Command   | Device ID   | Data1       | Data2   | Data3       | Data4   | Data5    | CRC8   |
|-----------|-------------|-------------|---------|-------------|---------|----------|--------|
| 1 byte    | 1 byte      | 1 byte      | 1 byte  | 1 byte      | 3 bytes | 16 bytes | 1 byte |
| 0x72      | Motor ID    | Read Status | Reserve | Data Length | Reserve | Data     | CRC8   |

62 00 57 03 6C

- ⚫ Example of Response Packet ： 72 00 01 00 03 00 00 00 00 B3 08 00 00 00 00 00 00 00 00 00 00 00 00 00 F1

## 7.4 I2C Write Raw

- ⚫ Function ： I2C raw data writing command.

- ⚫ Input Parameter ：

Motor ID(1 byte) ： Device address

I2C Address (1 byte) ： Slave device address

Data  Length  (1  byte) ： Data  length  to  write  (maximum  support  for  16  bytes)

Stop Bit(1 byte) ：

0 ： No stop bit

1 ： Stop bit included

Data (16 byte) ： Data to write

- ⚫ Return Parameter ：

Write Status (1 byte) ：

1 ： Write successful

0 ： Write failed

- ⚫ Command Code ：

63H

- ⚫ Command Packet Format ：

| Command   | Device ID   | Data1       | Data2       | Data3    | Data4   | Data5    | CRC8   |
|-----------|-------------|-------------|-------------|----------|---------|----------|--------|
| 1 byte    | 1 byte      | 1 byte      | 1 byte      | 1 byte   | 3 bytes | 16 bytes | 1 byte |
| 0x63      | Motor ID    | I2C Address | Data Length | Stop Bit | Reserve | Data     | CRC8   |

## ⚫ Response Packet Format ：

| Command   | Device ID   | Data1        | CRC8   |
|-----------|-------------|--------------|--------|
| 1 byte    | 1 byte      | 1 byte       | 1 byte |
| 0x73      | Motor ID    | Write Status | CRC8   |

- ⚫ Example of Send Packet ：

63 00 57 02 01 00 00 00 01 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 C1

- ⚫ Example of Response Packet ：

73 00 01 55