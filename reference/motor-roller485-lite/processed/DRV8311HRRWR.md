<!-- image -->

<!-- image -->

<!-- image -->

<!-- image -->

<!-- image -->

[DRV8311](https://www.ti.com/product/DRV8311)

SLVSFN2B - SEPTEMBER 2021 - REVISED FEBRUARY 2022

## DRV8311 Three-Phase PWM Motor Driver

## 1 Features

- Three-phase PWM motor driver -3-Phase brushless DC motors
- 3-V to 20-V operating voltage
- -24-V Absolute maximum voltage
- High output current capability
- -5-A Peak current drive
- Low on-state resistance MOSFETs

-

210-mΩ typ RDS(ON) (HS + LS) at TA = 25°C

- Low power sleep mode
- -1.5-µA at VVM = 12-V, TA = 25°C
- Multiple control interface options
- -6x PWM control interface
- -3x PWM control interface
- -PWM generation mode (SPI/tSPI) with optional calibration between MCU and DRV8311
- tSPI interface (DRV8311P)
- -PWM duty and frequency update over SPI
- -Control multiple DRV8311P devices using standard 4-wire SPI interface
- Supports up to 200-kHz PWM frequency
- Integrated current sensing
- -No external resistor required
- -Sense amplifier output, one per 1/2-bridge
- SPI and hardware device variants
- -10-MHz SPI communication (SPI/tSPI)
- Supports 1.8-V, 3.3-V, and 5-V logic inputs
- Built-in 3.3-V ± 4.5%, 100-mA LDO regulator
- Integrated protection features
- -VM undervoltage lockout (UVLO)
- -Charge pump undervoltage (CPUV)
- -Overcurrent protection (OCP)
- -Thermal warning and shutdown (OTW/OTSD)
- -Fault condition indication pin (nFAULT)

## 2 Applications

- [Brushless-DC (BLDC) Motor Modules](https://www.ti.com/solution/dc-input-bldc-motor-drive)
- Drones and Handheld Gimbal
- [Coffee Machines](https://www.ti.com/solution/coffee-machine)
- [Vacuum Robots](https://www.ti.com/solution/vacuum-robot)
- [Washer and Dryer Pumps](https://www.ti.com/solution/washer-dryer)
- Laptop, Desktop, and Server Fans

<!-- image -->

## 3 Description

The  DRV8311  provides  three  integrated  MOSFET half-H-bridges for driving a three-phase brushless DC (BLDC) motor for 5-V, 9-V, 12-V, or 18-V DC rails or 1S  to  4S  battery  powered  applications.  The  device integrates  three  current-sense  amplifiers  (CSA)  with integrated current sense for sensing the three phase currents  of  BLDC  motors  to  achieve  optimum  FOC and current-control system implementation.

The DRV8311P device provides capability to generate and  configure  PWM  timers  over  Texas  Instruments SPI  (tSPI),  and  allows  the  control  of  multiple  BLDC motors  directly  over  the  tSPI  interface.  This  feature reduces  the  number  of  required  I/O  ports  from  the primary controller to control multiple motors.

## Device Information (1)

| PART NUMBER   | PACKAGE   | BODY SIZE (NOM)   |
|---------------|-----------|-------------------|
| DRV8311H      | WQFN (24) | 3.00 mm × 3.00 mm |
| DRV8311S (2)  | WQFN (24) | 3.00 mm × 3.00 mm |
| DRV8311P      | WQFN (24) | 3.00 mm × 3.00 mm |

- (1) For all available packages, see the orderable addendum at the end of the data sheet.
- (2) Device available for preview only.

<!-- image -->

## DRV8311H/S Simplified Schematic

DRV8311P Simplified Schematic

<!-- image -->

## Table of Contents

| 1 Features ............................................................................1                                                                                                                    |                                                                                         | 8.5 SPI Communication..................................................47   |
|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------|-----------------------------------------------------------------------------|
| 2 Applications .....................................................................1                                                                                                                       | 9 DRV8311 Registers ........................................................52          |                                                                             |
| 3 Description .......................................................................1                                                                                                                      | 10 Application and Implementation ................................75                    |                                                                             |
| 4 Revision History .............................................................. 2                                                                                                                         | 10.1 Application Information...........................................                 | 75                                                                          |
| 5 Device Comparison Table ...............................................3                                                                                                                                  | 10.2 Typical Applications................................................               | 76                                                                          |
| 6 Pin Configuration and Functions ...................................4                                                                                                                                      | 10.3 Three Phase Brushless-DC tSPI Motor Control......79                                |                                                                             |
| 7 Specifications ..................................................................7                                                                                                                        | 10.4 Alternate Applications.............................................81              |                                                                             |
| 7.1 Absolute Maximum Ratings........................................ 7                                                                                                                                      | 11 Power Supply Recommendations ..............................82                        |                                                                             |
| 7.2 ESD Ratings............................................................... 7                                                                                                                            | 11.1 Bulk Capacitance....................................................               | 82                                                                          |
| 7.3 Recommended Operating Conditions.........................7                                                                                                                                              | 12 Layout ...........................................................................83 |                                                                             |
| 7.4 Thermal Information....................................................8                                                                                                                                | 12.1 Layout Guidelines...................................................83             |                                                                             |
| 7.5 Electrical Characteristics.............................................8                                                                                                                                | 12.2 Layout Example......................................................84             |                                                                             |
| 7.6 SPI Timing Requirements.........................................14                                                                                                                                      | 12.3 Thermal Considerations..........................................85                 |                                                                             |
| 7.7 SPI Secondary Device Mode Timings.......................15                                                                                                                                              | 13 Device and Documentation Support ..........................86                        |                                                                             |
| 7.8 Typical Characteristics..............................................16                                                                                                                                 | 13.1 Support Resources.................................................86               |                                                                             |
| 8 Detailed Description ......................................................17                                                                                                                             | 13.2 Trademarks.............................................................86          |                                                                             |
| 8.1 Overview...................................................................17                                                                                                                           | 13.3 Electrostatic Discharge Caution..............................86                    |                                                                             |
| 8.2 Functional Block Diagram.........................................18                                                                                                                                     | 13.4 Glossary..................................................................86       |                                                                             |
| 8.3 Feature Description...................................................21                                                                                                                                | 14 Mechanical, Packaging, and Orderable                                                 |                                                                             |
| 8.4 Device Functional Modes..........................................46                                                                                                                                     | Information ....................................................................        | 86                                                                          |
| 4 Revision History                                                                                                                                                                                          | 4 Revision History                                                                      | 4 Revision History                                                          |
| Changes from Revision A (December 2021) to Revision B (February 2022)                                                                                                                                       | Changes from Revision A (December 2021) to Revision B (February 2022)                   | Page                                                                        |
| • Added DRV8311H to the data sheet...................................................................................................................1 Changes from Revision * (September 2021) to Revision | A (December 2021)                                                                       | Page                                                                        |

<!-- image -->

<!-- image -->

## 5 Device Comparison Table

| DEVICE   | PACKAGES                  | INTERFACE   | nSLEEP INPUT   |
|----------|---------------------------|-------------|----------------|
| DRV8311P | 24-pin WQFN (3 mm x 3 mm) | SPI / tSPI  | Yes            |
| DRV8311S | 24-pin WQFN (3 mm x 3 mm) | SPI         | No             |
| DRV8311H | 24-pin WQFN (3 mm x 3 mm) | Hardware    | Yes            |

## Table 5-1. DRV8311H vs. DRV8311S vs. DRV8311P Configuration Comparison

| Parameters                                         | DRV8311H                                                                                                                      | DRV8311S                                                                                                          | DRV8311P                                                                                                          |
|----------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------|
| PWM control mode settings                          | MODE pin (2 settings)                                                                                                         | PWM_MODE (3 settings)                                                                                             | PWM_MODE (3 settings)                                                                                             |
| Slew rate settings                                 | SLEW pin (4 settings)                                                                                                         | SLEW_RATE (4 settings)                                                                                            | SLEW_RATE (4 settings)                                                                                            |
| Current sense amplifier gain                       | GAIN pin (4 settings)                                                                                                         | CSA_GAIN (4 settings)                                                                                             | CSA_GAIN (4 settings)                                                                                             |
| Over current protection (OCP) level settings       | MODE pin (2 settings)                                                                                                         | OCP_LVL (2 settings)                                                                                              | OCP_LVL (2 settings)                                                                                              |
| OCP blanking time                                  | Fixed to 0.2 us                                                                                                               | OCP_TBLANK (4 settings)                                                                                           | OCP_TBLANK (4 settings)                                                                                           |
| OCP deglitch time                                  | Fixed to 1 us                                                                                                                 | OCP_DEG (4 settings)                                                                                              | OCP_DEG (4 settings)                                                                                              |
| OCP mode                                           | Fast retry with 5-ms automatic retry                                                                                          | OCP_MODE (4 settings), Configurable retry time                                                                    | OCP_MODE (4 settings), Configurable retry time                                                                    |
| Dead time                                          | Fixed based on SLEW pin setting                                                                                               | TDEAD_CTRL (8 settings)                                                                                           | TDEAD_CTRL (8 settings)                                                                                           |
| Propagation delay                                  | Fixed based on SLEW pin setting                                                                                               | Fixed based on SLEW pin setting                                                                                   | Fixed based on SLEW pin setting                                                                                   |
| Driver delay compensation                          | Disabled                                                                                                                      | DLYCMP_EN (2 settings)                                                                                            | DLYCMP_EN (2 settings)                                                                                            |
| Spread Spectrum Modulation for internal Oscillator | Enabled                                                                                                                       | SSC_DIS (2 settings)                                                                                              | SSC_DIS (2 settings)                                                                                              |
| Undervoltage lockout                               | VINAVDD, CP and AVDD undervoltage protection enabled, CSAREF_UV disabled, t RETRY is configured for 5 ms fast automatic retry | VINAVDD, CP and AVDD undervoltage protection enabled, CSAREF_UV (2 settings), Configurable t RETRY using UVP_MODE | VINAVDD, CP and AVDD undervoltage protection enabled, CSAREF_UV (2 settings), Configurable t RETRY using UVP_MODE |
| SPI fault mode                                     | NA                                                                                                                            | SPIFLT_MODE (2 settings)                                                                                          | SPIFLT_MODE (2 settings)                                                                                          |
| Texas Instruments SPI (tSPI)                       | NA                                                                                                                            | NA                                                                                                                | Available                                                                                                         |
| Over temperature shutdown (OTSD) mode              | Fast retry with 5-ms automatic retry                                                                                          | OTSD_MODE (2 settings)                                                                                            | OTSD_MODE (2 settings)                                                                                            |

## 6 Pin Configuration and Functions

<!-- image -->

Figure 6-1. DRV8311S 24-Pin WQFN With Exposed Thermal Pad Top View

<!-- image -->

Figure 6-2. DRV8311H 24-Pin WQFN With Exposed Thermal Pad Top View

<!-- image -->

<!-- image -->

Figure 6-3. DRV8311P 24-Pin WQFN With Exposed Thermal Pad Top View

<!-- image -->

Table 6-1. Pin Functions

| PIN    | 24-pin Package   | 24-pin Package   | 24-pin Package   | TYPE (1)   | DESCRIPTION                                                                                                                                                                 |
|--------|------------------|------------------|------------------|------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| NAME   | DRV8311 H        | DRV8311P         | DRV8311S         | TYPE (1)   | DESCRIPTION                                                                                                                                                                 |
| AD0    | -                | 15               | -                | I          | Only on tSPI device DRV8311P. Address selection for tSPI.                                                                                                                   |
| AD1    | -                | 14               | -                | I          | Only on tSPI device DRV8311P. Address selection for tSPI.                                                                                                                   |
| AGND   | 16               | 16               | 16               | PWR        | Device analog ground. Connect to system ground.                                                                                                                             |
| AVDD   | 17               | 17               | 17               | PWR        | 3.3V regulator output. Connect a X5R or X7R, 0.7-µF to 7-µF, 6.3-V ceramic capacitor between the AVDD and AGND pins. This regulator can source up to 100 mA externally.     |
| CP     | 6                | 6                | 6                | PWR        | Charge pump output. Connect a X5R or X7R, 0.1-µF, 16-V ceramic capacitor between the VCP and VM pins.                                                                       |
| CSAREF | 2                | 2                | 2                | PWR        | Current sense amplifier power supply input and reference. Connect a X5R or X7R, 0.1-µF, 6.3-V ceramic capacitor between the CSAREF and AGND pins.                           |
| GAIN   | 21               | -                | -                | I          | Only on Hardware devices (DRV8311H). Current sense amplifier gain setting. The pin is a 4 level input pin configured by an external resistor between GAIN and AVDD or AGND. |
| INHA   | 15               | -                | 15               | I          | High-side driver control input for OUTA. This pin controls the state of the high-side MOSFET in 6x/3x PWM Mode.                                                             |
| INHB   | 14               | -                | 14               | I          | High-side driver control input for OUTB. This pin controls the state of the high-side MOSFET in 6x/3x PWM Mode.                                                             |
| INHC   | 13               | -                | 13               | I          | High-side driver control input for OUTC. This pin controls the state of the high-side MOSFET in 6x/3x PWM Mode.                                                             |
| INLA   | 18               | -                | 18               | I          | Low-side driver control input for OUTA. This pin controls the state of the low-side MOSFET in 6x PWM Mode.                                                                  |
| INLB   | 19               | -                | 19               | I          | Low-side driver control input for OUTB. This pin controls the state of the low-side MOSFET in 6x PWM Mode.                                                                  |
| INLC   | 20               | -                | 20               | I          | Low-side driver control input for OUTC. This pin controls the state of the low-side MOSFET in 6x PWM Mode.                                                                  |
| MODE   | 23               | -                | -                | I          | Only on Hardware devices (DRV8311H). PWM mode setting. This pin is a 4 level input pin configured by an external resistor between MODE and AVDD or AGND.                    |
| nFAULT | 1                | 1                | 1                | O          | Fault indication pin. Pulled logic-low with fault condition; open-drain output requires an external pullup to AVDD.                                                         |

[DRV8311](https://www.ti.com/product/DRV8311)

## Table 6-1. Pin Functions (continued)

| PIN         | 24-pin Package   | 24-pin Package   | 24-pin Package   | TYPE (1)   |                                                                                                                                                                                                                                                                                               |
|-------------|------------------|------------------|------------------|------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| NAME        | DRV8311 H        | DRV8311P         | DRV8311S         | TYPE (1)   | DESCRIPTION                                                                                                                                                                                                                                                                                   |
| nSCS        | -                | 20               | 24               | I          | Only on SPI (DRV8311S) and tSPI (DRV8311P) devices. Serial chip select. A logic low on this pin enables serial interface communication (SPI devices).                                                                                                                                         |
| nSLEEP      | 24               | 24               | -                | I          | Only on DRV8311H and DRV8311P devices. When this pin is logic low the device goes to a low-power sleep mode. A 15 to 50-µs low pulse on nSLEEP pin can be used to reset fault conditions without entering sleep mode.                                                                         |
| OUTA        | 10               | 10               | 10               | O          | Half bridge output A. Connect to motor winding.                                                                                                                                                                                                                                               |
| OUTB        | 11               | 11               | 11               | O          | Half bridge output B. Connect to motor winding.                                                                                                                                                                                                                                               |
| OUTC        | 12               | 12               | 12               | O          | Half bridge output C. Connect to motor winding.                                                                                                                                                                                                                                               |
| PGND        | 9                | 9                | 9                | PWR        | Device power ground. Connect to system ground.                                                                                                                                                                                                                                                |
| PWM_SY NC   | -                | 19               | -                | I          | Only on tSPI device DRV8311P. Connect to the MCU signal to synchronize the internally-generated PWM signals from DRV8311 to the MCU in PWM generation mode.                                                                                                                                   |
| SCLK        | -                | 23               | 23               | I          | Only on SPI (DRV8311S) and tSPI (DRV8311P) devices. Serial clock input. Serial data is shifted out on the rising edge and captured on the falling edge of SCLK (SPI devices).                                                                                                                 |
| SDI         | -                | 22               | 22               | I          | Only on SPI (DRV8311S) and tSPI (DRV8311P) devices. Serial data input. Data is captured on the falling edge of the SCLK pin (SPI devices).                                                                                                                                                    |
| SDO         | -                | 21               | 21               | O          | Only on SPI (DRV8311S) and tSPI (DRV8311P) devices. Serial data output. Data is shifted out on the rising edge of the SCLK pin.                                                                                                                                                               |
| SLEW        | 22               | -                | -                | I          | Only on DRV8311H device. OUTx voltage slew rate control setting. This pin is a 4 level input pin set by an external resistor between SLEW pin and AVDD or AGND.                                                                                                                               |
| SOA         | 5                | 5                | 5                | O          | Current sense amplifier output for OUTA.                                                                                                                                                                                                                                                      |
| SOB         | 4                | 4                | 4                | O          | Current sense amplifier output for OUTB.                                                                                                                                                                                                                                                      |
| SOC         | 3                | 3                | 3                | O          | Current sense amplifier output for OUTC.                                                                                                                                                                                                                                                      |
| VM          | 8                | 8                | 8                | PWR        | Power supply for the motor. Connect to motor supply voltage. Connect a X5R or X7R, 0.1-uF VM-rated ceramic bypass capacitor as well as a >=10-uF, VM-rated bulk capacitor between VM and PGND. Additionally, connect a X5R or X7R, 0.1-uF, 16-V ceramic capacitor between the VM and CP pins. |
| VIN_AVDD    | 7                | 7                | 7                | PWR        | Supply input for AVDD. Bypass to AGND with a X5R or X7R, 0.1-uF, VIN_AVDD-rated ceramic capacitor as well as a >=10-uF, VIN_AVDD-rated rated bulk capacitor between VIN_AVDD and PGND.                                                                                                        |
| Thermal pad |                  |                  |                  | PWR        | Must be connected to PGND.                                                                                                                                                                                                                                                                    |
| NC          | -                | 13,18            | -                | -          | No connect. Leave the pin floating.                                                                                                                                                                                                                                                           |

(1) I = input, O = output, PWR = power, NC = no connect

<!-- image -->

<!-- image -->

## 7 Specifications

## 7.1 Absolute Maximum Ratings

over operating ambient temperature range (unless otherwise noted) (1)

|                                                                                                |   MIN | MAX         | UNIT   |
|------------------------------------------------------------------------------------------------|-------|-------------|--------|
| Power supply pin voltage (VM)                                                                  |  -0.3 | 24          | V      |
| AVDD regulator input pin voltage (VIN_AVDD)                                                    |  -0.3 | 24          | V      |
| Voltage difference between ground pins (PGND, AGND)                                            |  -0.3 | 0.3         | V      |
| Charge pump voltage (CP)                                                                       |  -0.3 | V M + 6     | V      |
| Analog regulator output pin voltage (AVDD)                                                     |  -0.3 | 4           | V      |
| Logic pin input voltage (INHx, INLx, nSCS, nSLEEP, SCLK, SDI, ADx, GAIN, MODE, SLEW, PWM_SYNC) |  -0.3 | 6           | V      |
| Logic pin output voltage (nFAULT, SDO)                                                         |  -0.3 | 6           | V      |
| Open drain output current range (nFAULT)                                                       |     0 | 5           | mA     |
| Current sense amplifier reference supply input (CSAREF)                                        |  -0.3 | 4           | V      |
| Current sense amplifier output (SOx)                                                           |  -0.3 | 4           | V      |
| Peak Output Current (OUTA, OUTB, OUTC)                                                         |       | 5           | A      |
| Output pin voltage (OUTA, OUTB, OUTC)                                                          |    -1 | V M + 1 (2) | V      |
| Ambient temperature, T A                                                                       |   -40 | 125         | °C     |
| Junction temperature, T J                                                                      |   -40 | 150         | °C     |
| Storage tempertaure, T stg                                                                     |   -65 | 150         | °C     |

## 7.2 ESD Ratings

|         |                         |                                                            | VALUE   | UNIT   |
|---------|-------------------------|------------------------------------------------------------|---------|--------|
| V (ESD) | Electrostatic discharge | Human body model (HBM), per ANSI/ESDA/JEDEC JS-001 (1)     | ±1500   | V      |
| V (ESD) | Electrostatic discharge | Charged device model (CDM), per ANSI/ESDA/JEDEC JS-002 (2) | ±750    | V      |

## 7.3 Recommended Operating Conditions

over operating ambient temperature range (unless otherwise noted)

|           |                                  |                                                                      |   MIN |   NOM |   MAX | UNIT   |
|-----------|----------------------------------|----------------------------------------------------------------------|-------|-------|-------|--------|
| V VM      | Power supply voltage             | V VM                                                                 |     3 |    12 |    20 | V      |
| VIN_AVDD  | AVDD regulator input pin voltage | V VIN_AVDD                                                           |     3 |    12 |    20 | V      |
| f PWM     | Output PWM frequency             | OUTA, OUTB, OUTC                                                     |       |       |   200 | kHz    |
| I OUT (1) | Peak output current              | OUTA, OUTB, OUTC                                                     |       |       |     5 | A      |
| V IN      | Logic input voltage              | INHx, INLx, nSCS, nSLEEP, SCLK, SDI, ADx, GAIN, MODE, SLEW, PWM_SYNC |  -0.1 |       |   5.5 | V      |
| V OD      | Open drain pullup voltage        | nFAULT                                                               |  -0.1 |       |   5.5 | V      |
| I OD      | Open drain output sink current   | nFAULT                                                               |       |       |     5 | mA     |
| V CSAREF  | CSA refernce input Voltage       | CSAREF                                                               |     2 |       |   3.6 | V      |
| I CSAREF  | CSA refernce input Current       | CSAREF                                                               |       |   2.5 |   7.5 | mA     |

<!-- image -->

over operating ambient temperature range (unless otherwise noted)

|    |                                |    |   MIN | NOM   |   MAX | UNIT   |
|----|--------------------------------|----|-------|-------|-------|--------|
| T  | Operating ambient temperature  | A  |   -40 |       |   125 | °C     |
| T  | Operating Junction temperature | J  |   -40 |       |   150 | °C     |

## 7.4 Thermal Information

| THERMAL METRIC (1)   | THERMAL METRIC (1)                           |   DRV8311 QFN (RRW) 24 Pins | UNIT   |
|----------------------|----------------------------------------------|-----------------------------|--------|
| R θJA                | Junction-to-ambient thermal resistance       |                        42.6 | °C/W   |
| R θJC(top)           | Junction-to-case (top) thermal resistance    |                        37.9 | °C/W   |
| R θJB                | Junction-to-board thermal resistance         |                        15.7 | °C/W   |
| Ψ JT                 | Junction-to-top characterization parameter   |                         0.5 | °C/W   |
| Ψ JB                 | Junction-to-board characterization parameter |                        15.6 | °C/W   |
| R θJC(bot)           | Junction-to-case (bottom) thermal resistance |                         4.8 | °C/W   |

## 7.5 Electrical Characteristics

at T J  = -40°C to +150°C, VVM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A  = 25°C, V VM  = 12 V

| PARAMETER      | PARAMETER                               | TEST CONDITIONS                                                    | MIN            | TYP            | MAX            | UNIT           |
|----------------|-----------------------------------------|--------------------------------------------------------------------|----------------|----------------|----------------|----------------|
| POWER SUPPLIES | POWER SUPPLIES                          | POWER SUPPLIES                                                     | POWER SUPPLIES | POWER SUPPLIES | POWER SUPPLIES | POWER SUPPLIES |
| I VMQ          | VM sleep mode current                   | V VM = 12 V, nSLEEP = 0, T A = 25 °C                               |                | 1.5            | 3              | µA             |
| I VMQ          | VM sleep mode current                   | nSLEEP = 0, T A = 125 °C                                           |                |                | 9              | µA             |
| I VMS          | VM standby mode current                 | V VM = 12 V, nSLEEP = 1, INHx = INLx = 0, SPI = 'OFF', T A = 25 °C |                | 7              | 12             | mA             |
| I VMS          | VM standby mode current                 | nSLEEP = 1, INHx = INLx = 0, SPI = 'OFF'                           |                | 8              | 12             | mA             |
| I VM           | VM operating mode current               | V VM = 12 V, nSLEEP = 1, f PWM = 25 kHz, T A = 25 °C               |                | 10             | 13             | mA             |
| I VM           | VM operating mode current               | V VM = 12 V, nSLEEP = 1, f PWM = 200 kHz, T A = 25 °C              |                | 12             | 14             | mA             |
| I VM           | VM operating mode current               | nSLEEP =1, f PWM = 25 kHz                                          |                | 10             | 15             | mA             |
| I VM           | VM operating mode current               | nSLEEP =1, f PWM = 200 kHz                                         |                | 12             | 15             | mA             |
| V AVDD         | Analog regulator voltage                | V VM > 4V, V VIN_AVDD > 4.5V, 0 mA ≤ I AVDD ≤ 100 mA               | 3.15           | 3.3            | 3.45           | V              |
| V AVDD         | Analog regulator voltage                | V VM > 3.5V, 3.5V ≤V VIN_AVDD ≤ 4.5V, 0 mA ≤ I AVDD ≤ 35 mA        | 3              | 3.3            | 3.6            | V              |
| V AVDD         | Analog regulator voltage                | 2.5V ≤V VIN_AVDD ≤ 3.5V, 0 mA ≤ I AVDD ≤ 10 mA                     | 2.2            | VIN_AVDD -0.3  | 3.4            | V              |
| V AVDD         | Analog regulator voltage                | V VM < 4V, V VIN_AVDD > 4.5V, 0 mA ≤ I AVDD ≤ 40 mA                | 3              | 3.3            | 3.6            | V              |
| V AVDD         | Analog regulator voltage                | V VM < 3.5V, 3.5V ≤V VIN_AVDD ≤ 4.5V, 0 mA ≤ I AVDD ≤ 20 mA        | 3              | 3.3            | 3.6            | V              |
| I AVDD_LIM     | External analog regulator current limit |                                                                    | 148            | 200            | 250            | mA             |

<!-- image -->

<!-- image -->

| at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   | at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   | at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   | at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   | at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   | at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   | at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   |
|-------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|
| PARAMETER                                                                                                               | PARAMETER                                                                                                               | TEST CONDITIONS                                                                                                         | MIN                                                                                                                     | TYP                                                                                                                     | MAX                                                                                                                     | UNIT                                                                                                                    |
|                                                                                                                         |                                                                                                                         | V VM > 4V, V VIN_AVDD > 4.5V                                                                                            |                                                                                                                         |                                                                                                                         | 100                                                                                                                     | mA                                                                                                                      |
|                                                                                                                         |                                                                                                                         | V VM < 4V, V VIN_AVDD > 4.5V                                                                                            |                                                                                                                         |                                                                                                                         | 40                                                                                                                      | mA                                                                                                                      |
| I AVDD                                                                                                                  | External analog regulator load                                                                                          | V VM > 3.5V, 3.6V ≤V VIN_AVDD ≤ 4.5V                                                                                    |                                                                                                                         |                                                                                                                         | 35                                                                                                                      | mA                                                                                                                      |
|                                                                                                                         |                                                                                                                         | V VM < 3.5V, 3.6V ≤V VIN_AVDD ≤ 4.5V                                                                                    |                                                                                                                         |                                                                                                                         | 20                                                                                                                      | mA                                                                                                                      |
|                                                                                                                         |                                                                                                                         | 2.5V ≤V VIN_AVDD ≤ 3.6V                                                                                                 |                                                                                                                         |                                                                                                                         | 10                                                                                                                      | mA                                                                                                                      |
| C                                                                                                                       | Capacitance on AVDD pin                                                                                                 | I AVDD ≤ 25 mA;                                                                                                         | 0.7                                                                                                                     | 1                                                                                                                       | 7                                                                                                                       | µF                                                                                                                      |
| AVDD                                                                                                                    |                                                                                                                         | I AVDD ≥ 25 mA;                                                                                                         | 3.3                                                                                                                     | 4.7                                                                                                                     | 7                                                                                                                       | µF                                                                                                                      |
| R AVDD                                                                                                                  | AVDD Output Voltage Regulation                                                                                          | V VIN_AVDD > 4.5V; I AVDD ≤ 20 mA; V VIN_AVDD > 4.5V; 20 mA ≤ I AVDD ≤ 40                                               | -3                                                                                                                      |                                                                                                                         | 3                                                                                                                       | %                                                                                                                       |
|                                                                                                                         |                                                                                                                         | mA;                                                                                                                     | -2                                                                                                                      |                                                                                                                         | 2                                                                                                                       | %                                                                                                                       |
|                                                                                                                         |                                                                                                                         | V VIN_AVDD > 4.5V; I AVDD ≥ 40 mA;                                                                                      | -3                                                                                                                      |                                                                                                                         | 3                                                                                                                       | %                                                                                                                       |
| V VCP                                                                                                                   | Charge pump regulator voltage                                                                                           | VCP with respect to VM                                                                                                  | 3                                                                                                                       | 5                                                                                                                       | 5.6                                                                                                                     | V                                                                                                                       |
| t WAKE                                                                                                                  | Wakeup time                                                                                                             | V VM > V UVLO , nSLEEP = 1 to Output ready                                                                              |                                                                                                                         | 1                                                                                                                       | 3                                                                                                                       | ms                                                                                                                      |
| t WAKE_CSA                                                                                                              | Wakeup time for CSA                                                                                                     | V CSAREF > V CSAREF_UV to SOx ready, when nSLEEP = 1                                                                    |                                                                                                                         | 30                                                                                                                      | 50                                                                                                                      | µs                                                                                                                      |
| t SLEEP                                                                                                                 | Turn-off time                                                                                                           | nSLEEP = 0 to driver tri-stated                                                                                         |                                                                                                                         | 100                                                                                                                     | 200                                                                                                                     | µs                                                                                                                      |
| t RST                                                                                                                   | Reset Pulse time                                                                                                        | nSLEEP = 0 period to reset faults                                                                                       | 10                                                                                                                      |                                                                                                                         | 65                                                                                                                      | µs                                                                                                                      |
| LOGIC-LEVEL INPUTS (INHx, INLx, nSLEEP, SCLK, SDI)                                                                      | LOGIC-LEVEL INPUTS (INHx, INLx, nSLEEP, SCLK, SDI)                                                                      | LOGIC-LEVEL INPUTS (INHx, INLx, nSLEEP, SCLK, SDI)                                                                      | LOGIC-LEVEL INPUTS (INHx, INLx, nSLEEP, SCLK, SDI)                                                                      | LOGIC-LEVEL INPUTS (INHx, INLx, nSLEEP, SCLK, SDI)                                                                      | LOGIC-LEVEL INPUTS (INHx, INLx, nSLEEP, SCLK, SDI)                                                                      | LOGIC-LEVEL INPUTS (INHx, INLx, nSLEEP, SCLK, SDI)                                                                      |
| V IL                                                                                                                    | Input logic low voltage                                                                                                 |                                                                                                                         | 0                                                                                                                       |                                                                                                                         | 0.6                                                                                                                     | V                                                                                                                       |
| V IH                                                                                                                    | Input logic high voltage                                                                                                |                                                                                                                         | 1.65                                                                                                                    |                                                                                                                         | 5.5                                                                                                                     | V                                                                                                                       |
| V HYS                                                                                                                   | Input logic hysteresis                                                                                                  |                                                                                                                         | 100                                                                                                                     | 300                                                                                                                     | 660                                                                                                                     | mV                                                                                                                      |
| I IL                                                                                                                    | Input logic low current                                                                                                 | V PIN (Pin Voltage) = 0 V                                                                                               | -1                                                                                                                      |                                                                                                                         | 1                                                                                                                       | µA                                                                                                                      |
| I                                                                                                                       | Input logic high current                                                                                                | nSLEEP, V PIN (Pin Voltage) = 5 V                                                                                       |                                                                                                                         |                                                                                                                         | 30                                                                                                                      | µA                                                                                                                      |
| IH                                                                                                                      |                                                                                                                         | Other pins, V PIN (Pin Voltage) = 5 V                                                                                   |                                                                                                                         |                                                                                                                         | 50                                                                                                                      | µA                                                                                                                      |
| R                                                                                                                       | Input pulldown resistance                                                                                               | nSLEEP                                                                                                                  |                                                                                                                         | 230                                                                                                                     | 300                                                                                                                     | kΩ                                                                                                                      |
| PD                                                                                                                      |                                                                                                                         | Other pins                                                                                                              |                                                                                                                         | 160                                                                                                                     | 200                                                                                                                     | kΩ                                                                                                                      |
| C ID                                                                                                                    | Input capacitance                                                                                                       |                                                                                                                         |                                                                                                                         | 30                                                                                                                      |                                                                                                                         | pF                                                                                                                      |
| LOGIC-LEVEL INPUTS (nSCS)                                                                                               | LOGIC-LEVEL INPUTS (nSCS)                                                                                               | LOGIC-LEVEL INPUTS (nSCS)                                                                                               | LOGIC-LEVEL INPUTS (nSCS)                                                                                               | LOGIC-LEVEL INPUTS (nSCS)                                                                                               | LOGIC-LEVEL INPUTS (nSCS)                                                                                               | LOGIC-LEVEL INPUTS (nSCS)                                                                                               |
| V IL                                                                                                                    | Input logic low voltage                                                                                                 |                                                                                                                         | 0                                                                                                                       |                                                                                                                         | 0.6                                                                                                                     | V                                                                                                                       |
| V IH                                                                                                                    | Input logic high voltage                                                                                                |                                                                                                                         | 1.5                                                                                                                     |                                                                                                                         | 5.5                                                                                                                     | V                                                                                                                       |
| V HYS                                                                                                                   | Input logic hysteresis                                                                                                  |                                                                                                                         | 200                                                                                                                     |                                                                                                                         | 500                                                                                                                     | mV                                                                                                                      |
| I IL                                                                                                                    | Input logic low current                                                                                                 | V PIN (Pin Voltage) = 0 V                                                                                               |                                                                                                                         |                                                                                                                         | 90                                                                                                                      | µA                                                                                                                      |
| I IH                                                                                                                    | Input logic high current                                                                                                | V PIN (Pin Voltage) = 5 V                                                                                               |                                                                                                                         |                                                                                                                         | 70                                                                                                                      | µA                                                                                                                      |
| R PU                                                                                                                    | Input pullup resistance                                                                                                 |                                                                                                                         |                                                                                                                         | 48                                                                                                                      | 90                                                                                                                      | kΩ                                                                                                                      |
| C ID                                                                                                                    | Input capacitance                                                                                                       |                                                                                                                         |                                                                                                                         | 30                                                                                                                      |                                                                                                                         | pF                                                                                                                      |
| FOUR-LEVEL INPUTS (GAIN, MODE, SLEW)                                                                                    | FOUR-LEVEL INPUTS (GAIN, MODE, SLEW)                                                                                    | FOUR-LEVEL INPUTS (GAIN, MODE, SLEW)                                                                                    | FOUR-LEVEL INPUTS (GAIN, MODE, SLEW)                                                                                    | FOUR-LEVEL INPUTS (GAIN, MODE, SLEW)                                                                                    | FOUR-LEVEL INPUTS (GAIN, MODE, SLEW)                                                                                    | FOUR-LEVEL INPUTS (GAIN, MODE, SLEW)                                                                                    |
| V L1                                                                                                                    | Input mode 1 voltage                                                                                                    | Tied to AGND                                                                                                            | 0                                                                                                                       |                                                                                                                         | 0.21*AV DD                                                                                                              | V                                                                                                                       |
| V L2                                                                                                                    | Input mode 2 voltage                                                                                                    | 47 kΩ +/- 5% tied to GND                                                                                                | 0.25*AV DD                                                                                                              | 0.5*AVDD                                                                                                                | 0.55*AV DD                                                                                                              | V                                                                                                                       |
| V L3                                                                                                                    | Input mode 3 voltage                                                                                                    | Hi-Z                                                                                                                    | 0.606*AV DD                                                                                                             | 0.757*AVD D                                                                                                             | 0.909*AV DD                                                                                                             | V                                                                                                                       |
| V L4                                                                                                                    | Input mode 4 voltage                                                                                                    | Tied to AVDD                                                                                                            | 0.94*AV DD                                                                                                              |                                                                                                                         | AVDD                                                                                                                    | V                                                                                                                       |
| R PU                                                                                                                    | Input pullup resistance                                                                                                 | To AVDD                                                                                                                 |                                                                                                                         | 48                                                                                                                      | 70                                                                                                                      | kΩ                                                                                                                      |
| R PD                                                                                                                    | Input pulldown resistance                                                                                               | To AGND                                                                                                                 |                                                                                                                         | 160                                                                                                                     | 200                                                                                                                     | kΩ                                                                                                                      |

## [DRV8311](https://www.ti.com/product/DRV8311)

<!-- image -->

| J -40°C to +150°C, V VM = 3 to 20 V (unless   | PARAMETER                                                                 | TEST CONDITIONS                                                                            | A MIN                       | VM TYP                      | MAX                         | UNIT                        |
|-----------------------------------------------|---------------------------------------------------------------------------|--------------------------------------------------------------------------------------------|-----------------------------|-----------------------------|-----------------------------|-----------------------------|
| OPEN-DRAIN OUTPUTS (nFAULT)                   | OPEN-DRAIN OUTPUTS (nFAULT)                                               | OPEN-DRAIN OUTPUTS (nFAULT)                                                                | OPEN-DRAIN OUTPUTS (nFAULT) | OPEN-DRAIN OUTPUTS (nFAULT) | OPEN-DRAIN OUTPUTS (nFAULT) | OPEN-DRAIN OUTPUTS (nFAULT) |
| V OL                                          | Output logic low voltage                                                  | I OD = -5 mA                                                                               |                             |                             | 0.4                         | V                           |
| I OH                                          | Output logic high current                                                 | V OD = 5 V                                                                                 | -1                          |                             | 1                           | µA                          |
| C OD                                          | Output capacitance                                                        |                                                                                            |                             | 30                          |                             | pF                          |
| PUSH-PULL OUTPUTS (SDO)                       | PUSH-PULL OUTPUTS (SDO)                                                   | PUSH-PULL OUTPUTS (SDO)                                                                    | PUSH-PULL OUTPUTS (SDO)     | PUSH-PULL OUTPUTS (SDO)     | PUSH-PULL OUTPUTS (SDO)     | PUSH-PULL OUTPUTS (SDO)     |
| V OL                                          | Output logic low voltage                                                  | I OP = -5 mA, 2.2V ≤ V AVDD ≤ 3V                                                           | 0                           |                             | 0.55                        | V                           |
| V OL                                          | Output logic low voltage                                                  | I OP = -5 mA, 3V ≤ V AVDD ≤ 3.6V                                                           | 0                           |                             | 0.5                         | V                           |
| V OH                                          | Output logic high voltage                                                 | I OP = 5 mA, 2.2V ≤ V AVDD ≤ 3V                                                            | V AVDD - 0.86               |                             | 3                           | V                           |
| V OH                                          | Output logic high voltage                                                 | I OP = 5 mA, 3V ≤ V AVDD ≤ 3.6V                                                            | V AVDD - 0.5                |                             | 3.6                         | V                           |
| I OL                                          | Output logic low current                                                  | V OP = 0 V                                                                                 | -1                          |                             | 1                           | µA                          |
| I OH                                          | Output logic high current                                                 | V OP = 5 V                                                                                 | -1                          |                             | 1                           | µA                          |
| C OD                                          | Output capacitance                                                        |                                                                                            |                             | 30                          |                             | pF                          |
| DRIVER OUTPUTS                                | DRIVER OUTPUTS                                                            | DRIVER OUTPUTS                                                                             | DRIVER OUTPUTS              | DRIVER OUTPUTS              | DRIVER OUTPUTS              | DRIVER OUTPUTS              |
| R DS(ON)                                      | Total MOSFET on resistance (High-side + Low-side)                         | 6V ≥ V VM ≥ 3 V, I OUT = 1 A, T J = 25°C                                                   |                             | 300                         | 350                         | mΩ                          |
| R DS(ON)                                      | Total MOSFET on resistance (High-side + Low-side)                         | 6V ≥ V VM ≥ 3 V, I OUT = 1 A, T J = 150°C                                                  |                             | 450                         | 500                         | mΩ                          |
| R DS(ON)                                      | Total MOSFET on resistance (High-side + Low-side)                         | V VM ≥ 6 V, I OUT = 1 A, T J = 25°C                                                        |                             | 210                         | 250                         | mΩ                          |
| R DS(ON)                                      | Total MOSFET on resistance (High-side + Low-side)                         | V VM ≥ 6 V, I OUT = 1 A, T J = 150°C                                                       |                             | 330                         | 375                         | mΩ                          |
| SR                                            | Phase pin slew rate switching low to high (Rising from 20 %to 80 %of VM)  | V VM = 12V; SLEW = 00b (SPI Variant) or SLEW pin tied to AGND (HW Variant)                 | 18                          | 35                          | 55                          | V/us                        |
| SR                                            | Phase pin slew rate switching low to high (Rising from 20 %to 80 %of VM)  | V VM = 12V; SLEW = 01b (SPI Variant) or SLEW pin to 47 kΩ +/- 5% tied to AGND (HW Variant) | 35                          | 75                          | 100                         | V/us                        |
| SR                                            | Phase pin slew rate switching low to high (Rising from 20 %to 80 %of VM)  | V VM = 12V; SLEW = 10b (SPI Variant) or SLEW pin to Hi-Z (HW Variant)                      | 90                          | 180                         | 225                         | V/us                        |
| SR                                            | Phase pin slew rate switching low to high (Rising from 20 %to 80 %of VM)  | V VM = 12V; SLEW = 11b (SPI Variant) or SLEW pin tied to AVDD (HW Variant)                 | 140                         | 230                         | 355                         | Vus                         |
| SR                                            | Phase pin slew rate switching high to low (Falling from 80 %to 20 %of VM) | V VM = 12V; SLEW = 00b (SPI Variant) or SLEW pin tied to AGND (HW Variant)                 | 20                          | 35                          | 50                          | V/us                        |
| SR                                            | Phase pin slew rate switching high to low (Falling from 80 %to 20 %of VM) | V VM = 12V; SLEW = 01b (SPI Variant) or SLEW pin to 47 kΩ +/- 5% tied to AGND (HW Variant) | 35                          | 75                          | 100                         | V/us                        |
| SR                                            | Phase pin slew rate switching high to low (Falling from 80 %to 20 %of VM) | V VM = 12V; SLEW = 10b (SPI Variant) or SLEW pin to Hi-Z (HW Variant)                      | 80                          | 180                         | 225                         | V/us                        |
| SR                                            | Phase pin slew rate switching high to low (Falling from 80 %to 20 %of VM) | V VM = 12V; SLEW = 11b (SPI Variant) or SLEW pin tied to AVDD (HW Variant)                 | 125                         | 270                         | 350                         | V/us                        |

<!-- image -->

## [www.ti.com](https://www.ti.com/)

| at T J = -40°C          | to +150°C, V VM = 3 to 20 V (unless PARAMETER   | otherwise noted). Typical limits apply for TEST CONDITIONS                                                                  | T A = 25°C, MIN         | V VM = TYP              | V MAX                   | UNIT                    |
|-------------------------|-------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------|-------------------------|-------------------------|-------------------------|-------------------------|
|                         | Output dead time (high to low / low to          | V VM = 12V, SLEW = 00b (SPI Variant) or SLEW pin tied to AGND (HW Variant), DEADTIME = 000b, Handshake only                 |                         | 500                     | 1200                    | ns                      |
|                         |                                                 | V VM = 12V, SLEW = 01b (SPI Variant) or SLEW pin to 47 kΩ +/- 5% tied to AGND (HW Variant), DEADTIME = 000b, Handshake only |                         | 450                     | 760                     | ns                      |
|                         |                                                 | V VM = 12V, SLEW = 10b (SPI Variant) or SLEW pin to Hi-Z (HW Variant), DEADTIME = 000b, Handshake only                      |                         | 425                     | 720                     | ns                      |
| t DEAD                  | high)                                           | V VM = 12V, SLEW = 11b (SPI Variant) or SLEW pin tied to AVDD (HW Variant), DEADTIME = 000b; Handshake only                 |                         | 425                     | 710                     | ns                      |
| t DEAD                  |                                                 | V VM = 12 V, DEADTIME = 001b                                                                                                |                         | 200                     | 540                     | ns                      |
| t DEAD                  |                                                 | V VM = 12 V, DEADTIME = 010b                                                                                                |                         | 400                     | 550                     | ns                      |
| t DEAD                  |                                                 | V VM = 12 V, DEADTIME = 011b                                                                                                |                         | 600                     | 760                     | ns                      |
| t DEAD                  |                                                 | V VM = 12 V, DEADTIME = 100b                                                                                                |                         | 800                     | 900                     | ns                      |
| t DEAD                  |                                                 | V VM = 12 V, DEADTIME = 101b                                                                                                |                         | 1000                    | 1100                    | ns                      |
| t DEAD                  |                                                 | V VM = 12 V, DEADTIME = 110b                                                                                                |                         | 1200                    | 1300                    | ns                      |
|                         |                                                 | V VM = 12 V, DEADTIME = 111b                                                                                                |                         | 1400                    | 1500                    | ns                      |
| t PD                    | Propagation delay (high-side / ON/OFF)          | INHx = 1 to OUTx transisition, V VM = 12V, SLEW = 00b (SPI Variant) or SLEW pin tied to AGND (HW Variant)                   |                         | 1000                    | 1500                    | ns                      |
| t PD                    | low-side                                        | INHx = 1 to OUTx transisition, V VM = 12V, SLEW = 01b (SPI Variant) or SLEW pin to 47 kΩ +/- 5% tied to AGND (HW Variant)   |                         | 650                     | 1100                    | ns                      |
| t PD                    |                                                 | INHx = 1 to OUTx transisition, V VM = 12V, SLEW = 10b (SPI Variant) or SLEW pin to Hi-Z (HW Variant)                        |                         | 550                     | 950                     | ns                      |
| t PD                    |                                                 | INHx = 1 to OUTx transisition, V VM = 12V, SLEW = 11b (SPI Variant) or SLEW pin tied to AVDD (HW Variant)                   |                         | 500                     | 910                     | ns                      |
| t MIN_PULSE             | Minimum output pulse width                      | SLEW = 11b                                                                                                                  | 500                     |                         |                         | ns                      |
| CURRENT SENSE AMPLIFIER | CURRENT SENSE AMPLIFIER                         | CURRENT SENSE AMPLIFIER                                                                                                     | CURRENT SENSE AMPLIFIER | CURRENT SENSE AMPLIFIER | CURRENT SENSE AMPLIFIER | CURRENT SENSE AMPLIFIER |
| G CSA                   | Current sense gain (SPI Device)                 | CSA_GAIN = 00 (SPI Variant) or GAIN pin tied to AGND (HW Variant)                                                           |                         | 0.25                    |                         | V/A                     |
| G CSA                   | Current sense gain (SPI Device)                 | CSA_GAIN = 01 (SPI Variant) or GAIN pin to 47 kΩ +/- 5% tied to GND (HW Variant)                                            |                         | 0.5                     |                         | V/A                     |
| G CSA                   |                                                 | CSA_GAIN = 10 (SPI Variant) or GAIN pin to Hi-Z (HW Variant)                                                                |                         | 1                       |                         | V/A                     |
| G CSA                   |                                                 | CSA_GAIN = 11 (SPI Variant) or GAIN pin tied to AVDD (HW Variant)                                                           |                         | 2                       |                         | V/A                     |
| G CSA_ERR               | Current sense gain error                        | T J = 25°C, I PHASE < 2.5 A                                                                                                 | -4                      |                         | 4                       | %                       |
|                         |                                                 | T J = 25°C, I PHASE > 2.5 A                                                                                                 | -5                      |                         | 5                       | %                       |
|                         |                                                 | I PHASE < 2.5 A                                                                                                             | -5.5                    |                         | 5.5                     | %                       |
|                         |                                                 | I PHASE > 2.5 A                                                                                                             | -7                      |                         | 7                       | %                       |
|                         | Current sense gain error matching               | T J = 25°C                                                                                                                  | -5                      |                         | 5                       | %                       |
| I MATCH                 | between phases A, B and C                       |                                                                                                                             | -5                      |                         | 5                       | %                       |
| FS POS                  | Full scale positive current measurement         |                                                                                                                             | 5                       |                         |                         | A A                     |
| FS NEG                  | Full scale negative current measurement         |                                                                                                                             |                         |                         | -5                      |                         |

## [DRV8311](https://www.ti.com/product/DRV8311)

SLVSFN2B - SEPTEMBER 2021 - REVISED FEBRUARY 2022

<!-- image -->

| at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   | at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   | at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   | at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   | at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   | at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   | at T J = -40°C to +150°C, V VM = 3 to 20 V (unless otherwise noted). Typical limits apply for T A = 25°C, V VM = 12 V   |
|-------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|
| PARAMETER                                                                                                               | PARAMETER                                                                                                               | TEST CONDITIONS                                                                                                         | MIN                                                                                                                     | TYP                                                                                                                     | MAX                                                                                                                     | UNIT                                                                                                                    |
| V LINEAR                                                                                                                | SOX output voltage linear range                                                                                         |                                                                                                                         | 0.25                                                                                                                    |                                                                                                                         | V CSAREF - 0.25                                                                                                         | V                                                                                                                       |
| I OFFSET_RT                                                                                                             |                                                                                                                         | T J = 25°C, Phase current = 0 A, G CSA = 0.25 V/A                                                                       | -50                                                                                                                     |                                                                                                                         | 50                                                                                                                      | mA                                                                                                                      |
|                                                                                                                         | Current sense offset low side current in (Room Temperature)                                                             | T J = 25°C, Phase current = 0 A, G CSA = 0.5 V/A                                                                        | -50                                                                                                                     |                                                                                                                         | 50                                                                                                                      | mA                                                                                                                      |
|                                                                                                                         |                                                                                                                         | T J = 25°C, Phase current = 0 A, G CSA = 1 V/A                                                                          | -30                                                                                                                     |                                                                                                                         | 30                                                                                                                      | mA                                                                                                                      |
|                                                                                                                         |                                                                                                                         | T J = 25°C, Phase current = 0 A, G CSA = 2 V/A                                                                          | -30                                                                                                                     |                                                                                                                         | 30                                                                                                                      | mA                                                                                                                      |
| I OFFSET                                                                                                                | Current sense offset referred to low side current in                                                                    | Phase current = 0 A, G CSA = 0.25 V/A                                                                                   | -70                                                                                                                     |                                                                                                                         | 70                                                                                                                      | mA                                                                                                                      |
|                                                                                                                         |                                                                                                                         | Phase current = 0 A, G CSA = 0.5 V/A                                                                                    | -50                                                                                                                     |                                                                                                                         | 50                                                                                                                      | mA                                                                                                                      |
|                                                                                                                         |                                                                                                                         | Phase current = 0 A, G CSA = 1 V/A                                                                                      | -50                                                                                                                     |                                                                                                                         | 50                                                                                                                      | mA                                                                                                                      |
|                                                                                                                         |                                                                                                                         | Phase current = 0 A, G CSA = 2 V/A                                                                                      | -50                                                                                                                     |                                                                                                                         | 50                                                                                                                      | mA                                                                                                                      |
| t SET                                                                                                                   | Settling time to ±1%, 30 pF on SOx pin                                                                                  | Step on SOx = 1.2 V, G CSA = 0.25 V/A                                                                                   |                                                                                                                         |                                                                                                                         | 1                                                                                                                       | μs                                                                                                                      |
|                                                                                                                         |                                                                                                                         | Step on SOx = 1.2 V, G CSA = 0.5 V/A                                                                                    |                                                                                                                         |                                                                                                                         | 1                                                                                                                       | μs                                                                                                                      |
|                                                                                                                         |                                                                                                                         | Step on SOx = 1.2 V, G CSA = 1 V/A                                                                                      |                                                                                                                         |                                                                                                                         | 1                                                                                                                       | μs                                                                                                                      |
|                                                                                                                         |                                                                                                                         | Step on SOx = 1.2 V, G CSA = 2 V/A                                                                                      |                                                                                                                         |                                                                                                                         | 1                                                                                                                       | μs                                                                                                                      |
| V DRIFT                                                                                                                 | Drift offset                                                                                                            | Phase current = 0 A                                                                                                     | -150                                                                                                                    |                                                                                                                         | 150                                                                                                                     | µA/ ℃                                                                                                                   |
| I CSAREF                                                                                                                | CSAREF input current                                                                                                    | CSAREF = 3.0 V                                                                                                          |                                                                                                                         | 1.7                                                                                                                     | 3                                                                                                                       | mA                                                                                                                      |
| PROTECTION CIRCUITS                                                                                                     | PROTECTION CIRCUITS                                                                                                     | PROTECTION CIRCUITS                                                                                                     | PROTECTION CIRCUITS                                                                                                     | PROTECTION CIRCUITS                                                                                                     | PROTECTION CIRCUITS                                                                                                     | PROTECTION CIRCUITS                                                                                                     |
| V UVLO                                                                                                                  | Supply undervoltage lockout (UVLO)                                                                                      | VM rising                                                                                                               | 2.6                                                                                                                     | 2.7                                                                                                                     | 2.8                                                                                                                     | V                                                                                                                       |
|                                                                                                                         |                                                                                                                         | VM falling                                                                                                              | 2.5                                                                                                                     | 2.6                                                                                                                     | 2.7                                                                                                                     | V                                                                                                                       |
| V UVLO_HYS                                                                                                              | Supply undervoltage lockout hysteresis                                                                                  | Rising to falling threshold                                                                                             | 60                                                                                                                      | 125                                                                                                                     | 210                                                                                                                     | mV                                                                                                                      |
| t UVLO                                                                                                                  | Supply undervoltage deglitch time                                                                                       |                                                                                                                         | 5                                                                                                                       | 7.5                                                                                                                     | 13                                                                                                                      | µs                                                                                                                      |
| V                                                                                                                       | AVDD supply input undervoltage lockout                                                                                  | VIN_AVDD rising                                                                                                         | 2.6                                                                                                                     | 2.7                                                                                                                     | 2.8                                                                                                                     | V                                                                                                                       |
| VINAVDD_UV                                                                                                              | (VINAVDD_UV)                                                                                                            | VIN_AVDD falling                                                                                                        | 2.5                                                                                                                     | 2.6                                                                                                                     | 2.7                                                                                                                     | V                                                                                                                       |
| V VINAVDD_UV _HYS                                                                                                       | AVDD supply input undervoltage lockout hysteresis                                                                       | Rising to falling threshold                                                                                             | 100                                                                                                                     | 125                                                                                                                     | 150                                                                                                                     | mV                                                                                                                      |
| t VINAVDD_UV                                                                                                            | AVDD supply input undervoltage deglitch time                                                                            |                                                                                                                         | 2.5                                                                                                                     | 4                                                                                                                       | 5                                                                                                                       | µs                                                                                                                      |
| V CPUV                                                                                                                  | Charge pump undervoltage lockout                                                                                        | V CP rising                                                                                                             | 2                                                                                                                       | 2.3                                                                                                                     | 2.5                                                                                                                     | V                                                                                                                       |
|                                                                                                                         | (voltage with respect to VM)                                                                                            | V CP falling                                                                                                            | 2                                                                                                                       | 2.2                                                                                                                     | 2.4                                                                                                                     | V                                                                                                                       |
| V CPUV_HYS                                                                                                              | Charge pump undervoltage lockout hysteresis                                                                             | Rising to falling threshold                                                                                             | 65                                                                                                                      | 100                                                                                                                     | 125                                                                                                                     | mV                                                                                                                      |
| t CPUV                                                                                                                  | Charge pump undervoltage lockout deglitch time                                                                          |                                                                                                                         |                                                                                                                         | 0.2                                                                                                                     | 0.5                                                                                                                     | µs                                                                                                                      |
| V CSAREF_UV                                                                                                             | CSA reference undervoltage lockout                                                                                      | V CSAREF rising                                                                                                         | 1.68                                                                                                                    | 1.8                                                                                                                     | 1.95                                                                                                                    | V                                                                                                                       |
| V CSAREF_UV                                                                                                             | CSA reference undervoltage lockout                                                                                      | V CSAREF falling                                                                                                        | 1.6                                                                                                                     | 1.7                                                                                                                     | 1.85                                                                                                                    | V                                                                                                                       |
| V CSAREF_ UV_HYS                                                                                                        | CSA reference undervoltage lockout hysteresis                                                                           | Rising to falling threshold                                                                                             | 70                                                                                                                      | 90                                                                                                                      | 110                                                                                                                     | mV                                                                                                                      |
|                                                                                                                         | Analog regulator undervoltage lockout                                                                                   | V AVDD rising                                                                                                           | 1.8                                                                                                                     | 2                                                                                                                       | 2.2                                                                                                                     | V                                                                                                                       |
| V AVDD_UV                                                                                                               |                                                                                                                         | V AVDD falling                                                                                                          | 1.7                                                                                                                     | 1.8                                                                                                                     | 1.95                                                                                                                    | V                                                                                                                       |
| I OCP                                                                                                                   | Overcurrent protection trip point                                                                                       | OCP_LVL = 0 (SPI Variant) or MODE pin tied to AGND or MODE pin to Hi-Z (HW Variant)                                     | 5.8                                                                                                                     | 9                                                                                                                       | 11.5                                                                                                                    | A                                                                                                                       |
| I OCP                                                                                                                   |                                                                                                                         | OCP_LVL = 1 (SPI Variant) or MODE pin tied to AVDD or MODE pin 47 kΩ +/- 5% tied to AGND (HW Variant)                   | 3.4                                                                                                                     | 5                                                                                                                       | 7.5                                                                                                                     | A                                                                                                                       |

<!-- image -->

| PARAMETER   | PARAMETER                                          | TEST CONDITIONS               |   MIN |   TYP |   MAX | UNIT   |
|-------------|----------------------------------------------------|-------------------------------|-------|-------|-------|--------|
| t BLANK     | Overcurrent protection blanking time (SPI Variant) | OCP_TBLANK = 00b              |       |   0.2 |       | µs     |
| t BLANK     |                                                    | OCP_TBLANK = 01b              |       |   0.5 |       | µs     |
| t BLANK     |                                                    | OCP_TBLANK = 10b              |       |   0.8 |       | µs     |
| t BLANK     |                                                    | OCP_TBLANK = 10b              |       |     1 |       | µs     |
| t BLANK     | Overcurrent protection blanking time (HW Variant)  |                               |       |   0.2 |       | µs     |
| t OCP_DEG   | Overcurrent protection deglitch time (SPI Variant) | OCP_DEG = 00b                 |       |   0.2 |       | µs     |
| t OCP_DEG   | Overcurrent protection deglitch time (SPI Variant) | OCP_DEG = 01b                 |       |   0.5 |       | µs     |
| t OCP_DEG   | Overcurrent protection deglitch time (SPI Variant) | OCP_DEG = 10b                 |       |   0.8 |       | µs     |
| t OCP_DEG   |                                                    | OCP_DEG = 11b                 |       |     1 |       | µs     |
| t OCP_DEG   | Overcurrent protection deglitch time (HW Variant)  |                               |       |     1 |       | µs     |
| t RETRY     | Overcurrent protection retry time (SPI Variant)    | FAST_RETRY = 00b              |  0.24 |   0.5 |  0.65 | ms     |
| t RETRY     | Overcurrent protection retry time (SPI Variant)    | FAST_RETRY = 01b              |   0.7 |     1 |   1.2 | ms     |
| t RETRY     | Overcurrent protection retry time (SPI Variant)    | FAST_RETRY = 10b              |   1.6 |     2 |   2.2 | ms     |
| t RETRY     | Overcurrent protection retry time (SPI Variant)    | FAST_RETRY = 11b              |   4.4 |     5 |   5.3 | ms     |
| t RETRY     | Overcurrent protection retry time (SPI Variant)    | SLOW_RETRY = 00b              |   390 |   500 |   525 | ms     |
| t RETRY     | Overcurrent protection retry time (SPI Variant)    | SLOW_RETRY = 01b              |   840 |  1000 |  1050 | ms     |
| t RETRY     | Overcurrent protection retry time (SPI Variant)    | SLOW_RETRY = 10b              |  1700 |  2000 |  2200 | ms     |
| t RETRY     | Overcurrent protection retry time (SPI Variant)    | SLOW_RETRY = 11b              |  4400 |  5000 |  5400 | ms     |
| t RETRY     | Overcurrent protection retry time (HW Variant)     |                               |       |     5 |       | ms     |
| T OTW       | Thermal warning temperature                        | Die temperature (T J ) Rising |   170 |   178 |   185 | °C     |
| T OTW_HYS   | Thermal warning hysteresis                         | Die temperature (T J )        |       |    25 |    30 | °C     |
| T TSD       | Thermal shutdown temperature                       | Die temperature (T J ) Rising |   180 |   190 |   200 | °C     |
| T TSD_HYS   | Thermal shutdown hysteresis                        | Die temperature (T J )        |       |    25 |    30 | °C     |
| T TSD       | Thermal shutdown temperature (LDO)                 | Die temperature (T J ) Rising |   180 |   190 |   200 | °C     |
| T TSD_HYS   | Thermal shutdown hysteresis (LDO)                  | Die temperature (T J )        |       |    25 |    30 | °C     |

[DRV8311](https://www.ti.com/product/DRV8311)

<!-- image -->

| PARAMETER                  | PARAMETER                  | TEST CONDITIONS                                 | MIN                        | TYP                        | MAX                        | UNIT                       |
|----------------------------|----------------------------|-------------------------------------------------|----------------------------|----------------------------|----------------------------|----------------------------|
| PWM OUTPUT ACCURACY (tSPI) | PWM OUTPUT ACCURACY (tSPI) | PWM OUTPUT ACCURACY (tSPI)                      | PWM OUTPUT ACCURACY (tSPI) | PWM OUTPUT ACCURACY (tSPI) | PWM OUTPUT ACCURACY (tSPI) | PWM OUTPUT ACCURACY (tSPI) |
| R PWM                      | Output PWM Resolution      | PWM FREQUENCY = 20 kHz                          |                            | 10                         |                            | bits                       |
| A PWM                      | Output PWM Accuracy        | V VM < 4.5V, PWM_SYNC and Clock Tuning Disabled | -7.5                       |                            | 7.5                        | %                          |
| A PWM                      | Output PWM Accuracy        | V VM >4.5V, PWM_SYNC and Clock Tuning Disabled  | -4                         |                            | 4                          | %                          |
| A PWM                      | Output PWM Accuracy        | PWM_SYNC Enabled and Clock Tuning Disabled      | -1                         |                            | 1                          | %                          |
| A PWM                      | Output PWM Accuracy        | PWM_SYNC Disabled and SPISYNC_ACRCY = 11b       | -2                         |                            | 2                          | %                          |
| A PWM                      | Output PWM Accuracy        | PWM_SYNC Disabled and SPISYNC_ACRCY = 10b       | -1                         |                            | 1                          | %                          |
| A PWM                      | Output PWM Accuracy        | PWM_SYNC Disabled and SPISYNC_ACRCY = 01b       | -1                         |                            | 1                          | %                          |
| A PWM                      | Output PWM Accuracy        | PWM_SYNC Disabled and SPISYNC_ACRCY = 00b       | -1                         |                            | 1                          | %                          |

## 7.6 SPI Timing Requirements

|           |                            |   MIN | NOM   |   MAX | UNIT   |
|-----------|----------------------------|-------|-------|-------|--------|
| t READY   | SPI ready after power up   |       |       |     1 | ms     |
| t HI_nSCS | nSCS minimum high time     |   300 |       |       | ns     |
| t SU_nSCS | nSCS input setup time      |    25 |       |       | ns     |
| t HD_nSCS | nSCS input hold time       |    25 |       |       | ns     |
| t SCLK    | SCLK minimum period        |   100 |       |       | ns     |
| t SCLKH   | SCLK minimum high time     |    50 |       |       | ns     |
| t SCLKL   | SCLK minimum low time      |    50 |       |       | ns     |
| t SU_SDI  | SDI input data setup time  |    25 |       |       | ns     |
| t HD_SDI  | SDI input data hold time   |    25 |       |       | ns     |
| t DLY_SDO | SDO output data delay time |       |       |    75 | ns     |
| t EN_SDO  | SDO enable delay time      |       |       |    65 | ns     |
| t DIS_SDO | SDO disable delay time     |       |       |    50 | ns     |

<!-- image -->

## 7.7 SPI Secondary Device Mode Timings

Figure 7-1. SPI Secondary Device Mode Timing Diagram

<!-- image -->

<!-- image -->

## 7.8 Typical Characteristics

<!-- image -->

Figure 7-2. RDS(ON) (high and low side combined) for MOSFETs over temperature, VM ≥ 6V

<!-- image -->

Figure 7-4. Sleep current over supply voltage

<!-- image -->

Figure 7-3. Operating mode current over supply voltage

<!-- image -->

Figure 7-5. AVDD regulator output voltage over load current

<!-- image -->

<!-- image -->

[www.ti.com](https://www.ti.com/)

## 8 Detailed Description

## 8.1 Overview

The DRV8311 is an integrated MOSFET driver for 3-phase motor-drive applications. The combined high-side and low-side FET's on-state resistance is 210-mΩ typical. The device reduces system component count, cost, and complexity by integrating three half-bridge MOSFETs, gate drivers, charge pump, current sense amplifier and linear regulator for an external load. For the DRV8311S, a standard serial peripheral interface (SPI) provides a simple method for configuring the various device settings and reading fault diagnostic information through an external controller. For the DRV8311H, a hardware interface (H/W) allows for configuring the most commonly used settings  through  fixed  external  resistors.  For  the  DRV8311P,  Texas  Instruments  SPI  (tSPI)  provides  the ability  to  configure  various  device  settings  and  adjust  the  PWM  duty  cycle  and  frequency  to  control  multiple motors at a time.

The architecture uses an internal state machine to protect against short-circuit events, and protect against dV/dt parasitic turn on of the internal power MOSFETs.

The DRV8311 device integrates three bidirectional low side current-shunt amplifiers for monitoring the current through  each  of  the  half-bridges  using  a  built-in  current  sense  and  no  external  current  sense  resistors  are needed. The gain setting of the shunt amplifier can be adjusted through the SPI, tSPI or hardware interface.

In  addition  to  the  high  level  of  device  integration,  the  DRV8311  device  provides  a  wide  range  of  integrated protection features. These  features  include  power  supply  undervoltage  lockout  (UVLO),  charge  pump undervoltage  lockout  (CPUV),  overcurrent  protection  (OCP),  AVDD  undervoltage  lockout  (AVDD\_UV)  and overtemperature  shutdown  (OTW  and  OTSD).  Fault  events  are  indicated  by  the  nFAULT  pin  with  detailed information available in the registers on the SPI and tSPI device versions.

The DRV8311H, DRV8311P and DRV8311S devices are available in 0.4-mm pin pitch, WQFN surface-mount packages. The WQFN package size is 3.00 mm × 3.00 mm.

## 8.2 Functional Block Diagram

<!-- image -->

Figure 8-1. DRV8311S Block Diagram

<!-- image -->

<!-- image -->

Figure 8-2. DRV8311H Block Diagram

<!-- image -->

[DRV8311](https://www.ti.com/product/DRV8311)

<!-- image -->

Figure 8-3. DRV8311P Block Diagram

<!-- image -->

<!-- image -->

## 8.3 Feature Description

Table 8-1 lists the recommended values of the external components for the driver.

Table 8-1. DRV8311 External Components

| COMPONENTS   | PIN 1    | PIN 2        | RECOMMENDED                                  |
|--------------|----------|--------------|----------------------------------------------|
| C VM1        | VM       | PGND         | X5R or X7R, 0.1-µF, VM-rated capacitor       |
| C VM2        | VM       | PGND         | ≥ 10-µF, VM-rated electrolytic capacitor     |
| C VIN_AVDD1  | VIN_AVDD | AGND         | X5R or X7R, 0.1-µF, VIN_AVDD-rated capacitor |
| C VIN_AVDD2  | VIN_AVDD | AGND         | ≥ 10-µF, VIN_AVDD-rated capacitor            |
| C CP         | CP       | VM           | X5R or X7R, 16-V, 0.1-µF capacitor           |
| C AVDD       | AVDD     | AGND         | X5R or X7R, 0.7 to 7-µF, 6.3-V capacitor     |
| R nFAULT     | AVDD     | nFAULT       | 5.1-kΩ, Pullup resistor                      |
| R SDO        | AVDD     | SDO          | 5.1-kΩ, Pullup resistor (Optional)           |
| R MODE       | MODE     | AGND or AVDD | Section 8.3.3.2                              |
| R SLEW       | SLEW     | AGND or AVDD | Section 8.3.3.2                              |
| R GAIN       | GAIN     | AGND or AVDD | Section 8.3.3.2                              |
| C CSAREF     | CSAREF   | AGND         | X5R or X7R, 0.1-µF, CSAREF-rated capacitor   |

## 8.3.1 Output Stage

The DRV8311 device consists of integrated NMOS MOSFETs connected in a three-phase bridge configuration. A doubler charge pump provides the proper gate-bias voltage to the high-side NMOS MOSFETs across a wide operating voltage range in addition to providing 100% duty-cycle support. An internal linear regulator operating from the VM supply provides the gate-bias voltage (VLS) for the low-side MOSFETs.

## 8.3.2 Control Modes

The  DRV8311  family  of  devices  provides  three  different  control  modes  to  support  various  commutation  and control methods. Table 8-2 shows the various modes of the DRV8311 device.

Table 8-2. PWM Control Modes

| MODE Type   | MODE Pin (DRV8311H)                                     | MODE Bits (DRV8311S)             | MODE Bits (DRV8311P)   | MODE                |
|-------------|---------------------------------------------------------|----------------------------------|------------------------|---------------------|
| Mode 1      | Mode pin tied to AGND or Mode pin to 47 kΩ tied to AGND | PWM_MODE = 00b or PWM_MODE = 01b | NA                     | 6x Mode             |
| Mode 2      | Mode pin Hi-Z or Mode pin tied to AVDD                  | PWM_MODE = 10b                   | NA                     | 3x Mode             |
| Mode 3      | NA                                                      | PWM_MODE = 11b                   | PWM_MODE = 11b         | PWM Generation Mode |

## Note

Texas Instruments do not recommend changing the MODE pin or MODE register during power up of  the  device (i.e.  during  tWAKE). The MODE setting on DRV8311H is latched at power up, so set nSLEEP = 0 before changing the MODE pin configuration on the DRV8311H. In DRV8311S, set all INHx and INLx pins to logic low before changing the MODE register.

## 8.3.2.1 6x PWM Mode (DRV8311S and DRV8311H variants only)

In  6x  PWM  mode,  each  half-bridge  supports  three  output  states:  low,  high,  or  high-impedance  (Hi-Z).  To configure  DRV8311H  in  6x  PWM  mode,  connect  the  MODE  pin  to  AGND  or  connect  the  MODE  pin  to  47 kΩ tied to AGND. To enable 6x PWM mode in DRV8311S configure the MODE bits with PWM\_MODE = 00b or 01b. The corresponding INHx and INLx signals control the output state as listed in Table 8-3.

Table 8-3. 6x PWM Mode Truth Table

|   INLx |   INHx | OUTx   |
|--------|--------|--------|
|      0 |      0 | Hi-Z   |
|      0 |      1 | H      |
|      1 |      0 | L      |
|      1 |      1 | Hi-Z   |

Figure 8-4 shows the application diagram of DRV8311 configured in 6x PWM mode.

<!-- image -->

Figure 8-4. 6x PWM Mode

<!-- image -->

<!-- image -->

## 8.3.2.2 3x PWM Mode (DRV8311S and DRV8311H variants only)

In  3x  PWM  mode,  the  INHx  pin  controls  each  half-bridge  and  supports  two  output  states:  low  or  high.  To configure  DRV8311H in 3x PWM mode, connect the MODE pin to AVDD or keep the MODE pin to Hi-Z. To enable 3x PWM mode in DRV8311S configure the MODE bits with PWM\_MODE = 10b. The INLx pin is used to put the half bridge in the Hi-Z state. If the Hi-Z state is not required, tie all INLx pins to logic high (for example, by tying them to AVDD). The corresponding INHx and INLx signals control the output state as listed in Table 8-4.

Table 8-4. 3x PWM Mode Truth Table

|   INLx | INHx   | OUTx   |
|--------|--------|--------|
|      0 | X      | Hi-Z   |
|      1 | 0      | L      |
|      1 | 1      | H      |

Figure 8-5 shows the typical application diagram of the DRV8311 configured in 3x PWM mode.

Figure 8-5. 3x PWM Mode

<!-- image -->

## 8.3.2.3 PWM Generation Mode (DRV8311S and DRV8311P Variants)

In PWM generation mode, the PWM signals are generated internally in the DRV8311 and can be controlled via a SPI (DRV8311S) or tSPI (DRV8311P) register read/write. This operation mode removes the need for controlling the motor through the INHx and INLx pins. The PWM period, frequency, and duty cycle for each phase can be configured over the serial interface. A PWM\_SYNC pin functionality allows synchronization between the MCU and  DRV8311. The PWM modes can be configured to enable or disable the high-side or low-side  MOSFET PWM control  for  each  phase  in  order  to  allow  for  continuous  or  discontinuous  switching  whenever  required. When using the DRV8311S in PWM Generation mode, connect the PWM\_SYNC signal from MCU to the INLB pin of DRV8311S. The DRV8311S does not care about the state of all other INHx and INLx pins in this mode. Trapezoidal, sinusoidal, and FOC control are all possible using PWM generation mode.

<!-- image -->

Figure 8-6. PWM Generation Mode - DRV8311P

<!-- image -->

<!-- image -->

[www.ti.com](https://www.ti.com/)

Figure 8-7. PWM Generation Mode - DRV8311S

<!-- image -->

PWM  generation  mode  has  three  different  options:  up/down  mode,  up  mode,  and  down  mode.  The  PWM generation mode can be configured using PWMCNTR\_MODE bits in the PWMG\_CTRL register. The duty cycle defined  by  the  PWM\_DUTY\_OUTx  bits  in  the  PWMG\_x\_DUTY  register  (x  for  each  phase  A,  B,  C)  of  each phase is compared against the reference counter signal to generate the high side MOSFET PWM. The PWM generation uses a reference counter signal generated internally based on the configuration of PWM\_PRD\_OUT bits  (PWMG\_PERIOD register)  and  PWMCNTR\_MODE bits. If  PWM\_EN bit is  high,  the  high  side  MOSFET PWM output is high when PWM\_DUTY\_OUTx is greater than the reference counter. For PWM\_EN being low, the output is always held low. To achieve 100% duty cycle for the high side MOSFET [HS\_ON for entire cycle], the PWM\_DUTY\_OUTx value must be higher than the PWM\_PRD\_OUT value.

In up/down mode [PWMCNTR\_MODE = 0h], the reference counter waveform resembles a V shape, counting down from the PWM\_PRD\_OUT value when enabled and then counting up again once counter reaches zero. Configure the PWM\_PRD\_OUT bits to generate a PWM frequency (FPWM) using the relation PWM\_PRD\_OUT =  0.5  x  (FSYS /FPWM). FSYS is the internal system clock frequency (approximately 20MHz) of DRV8311P and DRV8311S.

<!-- image -->

<!-- image -->

Figure 8-8. PWM Generation - Up/Down Mode

<!-- image -->

In up mode [PWMCNTR\_MODE = 1h], the counter counts up from zero until it reaches the PWM\_PRD\_OUT value and then resets to zero. PWM\_PRD\_OUT = FSYS /FPWM

Figure 8-9. PWM Generation - Up Mode

<!-- image -->

In  down  mode  [PWMCNTR\_MODE = 2h],  the  counter  counts  down  from  the  PWM\_PRD\_OUT  value  until  it reaches zero and then resets to PWM\_PRD\_OUT value. PWM\_PRD\_OUT = FSYS /FPWM

Figure 8-10. PWM Generation - Down Mode

<!-- image -->

<!-- image -->

The dead time configured by the TDEAD\_CTRL register is inserted between the LS\_ON falling edge and the HS\_ON rising edge as well as between HS\_ON falling edge and LS\_ON rising edge.

## PWM Synchronization in PWM Generation Mode

When  there  is  no  dedicated  INHx  or  INLx  control  signals,  the  external  MCU  can  lose  synchronization  with PWM signal generated by the  DRV8311.  For  synchronization,  the  external  MCU  sends  one  reference  signal to the PWM\_SYNC pin. PWM synchronization helps to generate the DRV8311 PWM output with the accuracy of  the  MCU  clock  and  aligns  PWM  outputs  with  the  MCU's  ADC  sampling  the  current  sense  outputs.  The PWM\_SYNC signal can also help to measure the DRV8311 internal oscillator frequency. DRV8311 also support auto-calibration  of  internal  oscillator  to  calibrate  the  oscillator  at  20MHz  regardless  of  operating  conditions. The DRV8311 allows five different methods of synchronizing between MCU and DRV8311 by configuring the PWM\_OSC\_SYNC bits of PWMG\_CTRL register. The different synchronization methods are outlined below.

PWM\_OSC\_SYNC = 1h : The DRV8311 measures the PWM\_SYNC signal period (PWM\_SYNC\_PRD) in counts of DRV8311 system clock FSYS (approximately 20MHz). The MCU reads the register PWM\_SYNC\_PRD and can calibrate the PWM period. For example, assume that the MCU generate a 50% duty PWM\_SYNC signal using an MCU timer with a period count of N and clock frequency FMCU. The MCU read the PWM\_SYNC\_PERIOD register  value  say  M,  generated  by  DRV8311.  The  DRV8311  generates  the  PWM\_SYNC\_PERIOD  using  the DRV8311 system clock FSYS(DRV). Now the MCU timer clock and the DRV8311 system clock are related by the equation FMCU x M = FSYS(DRV) x N.

The PWM\_SYNC\_PRD is 12bit and with DRV8311 internal system clock of approximately 20MHz, the minimum PWM\_SYNC frequency that can be read without saturation is approximately 4.885 kHz (FSYS/4095).

PWM\_OSC\_SYNC = 2h :  The PWM\_SYNC signal from the MCU is used to set the PWM period of DRV8311 and  PWMG\_PERIOD  register  setting  is  ignored.  DRV8311  resets  the  PWM  counter  on  rising  edge  of  the PWM\_SYNC.

Figure 8-11. PWM Synchronization in Up/down Mode (PWM\_OSC\_SYNC = 2h)

<!-- image -->

<!-- image -->

<!-- image -->

Figure 8-12. PWM Synchronization in Up Mode (PWM\_OSC\_SYNC = 2h)

<!-- image -->

Figure 8-13. PWM Synchronization in Down Mode (PWM\_OSC\_SYNC = 2h)

<!-- image -->

PWM\_OSC\_SYNC = 5h :  PWM\_SYNC  is  used  for  DRV8311  internal  oscillator  synchronization  (only  20  kHz frequency  supported).  For  a  PWM\_SYNC  signal  of  20kHz,  DRV8311  counts  the  number  of  internal  system oscillator clock pulses between the rising edges of PWM\_SYNC signal. For DRV8311 system clock at 20MHz, the number of clock pulses are expected to be 1000 in the ideal case. Deviation from this number implies an error in either the oscillator frequency generated by DRV8311 or the PWM\_SYNC frequency from the MCU. The PWM\_SYNC frequency from MCU is assumed accurate and DRV8311 does oscillator calibration internally to calibrate the frequency at 20MHz and hence align PWM frequency generated with PWM\_SYNC.

PWM\_OSC\_SYNC = 6h :  PWM\_SYNC is used for DRV8311 internal system oscillator  calibration  and  setting PWM period  (only  20  kHz  frequency  supported).  The  PWMG\_PERIOD  register  setting  is  ignored.  DRV8311 resets the PWM reference counter on rising edge of the PWM\_SYNC.

PWM\_OSC\_SYNC  =  7h :  The  SPI  Clock  pin  SCLK  is  used  for  the  DRV8311  internal  system  oscillator calibration  to  20MHz.  In  this  mode,  the  user  has  to  configure  the  SPI  clock  frequency  for  synchronizing  the oscillator (SPICLK\_FREQ\_SYNC) and the number of SPI clock cycles required for synchronizing the oscillator

<!-- image -->

(SPISYNC\_ACRCY) by configuring the PWMG\_CTRL Register. The DRV8311 measures the total time for the entire  SPI  clock  cycles  (configured  by  SPISYNC\_ACRCY)  in  counts  of  DRV8311  internal  system  clock  F SYS and  calibrates  the  internal  system  clock  to  match  the  counts  expected  for  20MHz  frequency.  The  DRV8311 system  oscillator  frequency  accuracy  after  calibration  compared  to  20MHz  depends  on  the  configuration  of SPISYNC\_ACRCY.

## 8.3.3 Device Interface Modes

The  DRV8311  family  of  devices  supports  three  different  interface  modes  (SPI,  tSPI  and  hardware)  to  offer either  increased  simplicity  (hardware  interface)  or  greater  flexibility  and  diagnostics  (SPI  interface).  The  SPI (DRV8311S)  and  hardware  (DRV8311H)  interface  modes  share  the  same  four  pins,  allowing  the  different versions to be pin-to-pin compatible. Designers are encouraged to evaluate with the SPI interface version due to ease of changing settings, and may consider switching to the hardware interface with minimal modifications to the design.

## 8.3.3.1 Serial Peripheral Interface (SPI)

The SPI/tSPI devices support a serial communication bus that lets an external controller send and receive data with  the  DRV8311.  This  support  allows  the  external  controller  to  configure  device  settings  and  read  detailed fault  information.  The interface is a four wire interface using the SCLK, SDI, SDO, and nSCS pins which are described as follows:

- The SCLK (serial clock) pin is an input that accepts a clock signal to determine when data is captured and propagated on the SDI and SDO pins.
- The SDI (serial data in) pin is the data input.
- The SDO (serial data out) pin is the data output.
- The nSCS (serial chip select) pin is the chip select input. A logic low signal on this pin enables SPI communication with the DRV8311.

For more information on the SPI, see Section 8.5 .

## 8.3.3.2 Hardware Interface

Hardware  interface  devices  omit  the  four  SPI  pins  and  in  their  place  have  nSLEEP  pin  and  three  resistorconfigurable inputs which are GAIN, SLEW and MODE.

Common  device  settings  can  be  adjusted  on  the  hardware  interface  by  tying  the  pin  logic  low,  logic  high, or  pulling  up  or  pulling  down  with  a  resistor.  Fault  conditions  are  reported  on  the  nFAULT  pin,  but  detailed diagnostic information is not available.

- The GAIN pin configures the gain of the current sense amplifier.
- The SLEW pin configures the slew rate of the output voltage to motor.
- The MODE pin configures the PWM control mode and OCP level.

For more information on the hardware interface, see Section 8.3.9 .

## Table 8-5. Hardware Pins Decode

| Configuration             | GAIN     | SLEW     | MODE                         |
|---------------------------|----------|----------|------------------------------|
| Pin tied to AGND          | 0.25 V/A | 35 V/us  | 6x PWM Mode and 9A OCP Level |
| Pin to 47 kΩ tied to AGND | 0.5 V/A  | 75 V/us  | 6x PWM Mode and 5A OCP Level |
| Pin to Hi-Z               | 1 V/A    | 180 V/us | 3x PWM Mode and 9A OCP Level |
| Pin tied to AVDD          | 2 V/A    | 230 V/us | 3x PWM Mode and 5A OCP Level |

<!-- image -->

Figure 8-14. DRV8311 SPI Interface

<!-- image -->

<!-- image -->

Figure 8-15. DRV8311 Hardware Interface

<!-- image -->

<!-- image -->

## 8.3.4 AVDD Linear Voltage Regulator

A  3.3-V,  100mA  linear  regulator  is  integrated  into  the  DRV8311  family  of  devices  and  is  available  to  power external circuits. The AVDD regulator is used for powering up the internal digital functions of the DRV8311 and can also provide the supply voltage for a low-power MCU or another circuitry up to 100 mA. The output of the AVDD regulator should be bypassed near the AVDD and AGND pins with a X5R or X7R, up to 4.7-µF, 6.3-V ceramic capacitor routed directly back to the adjacent AGND ground pin.

The AVDD nominal, no-load output voltage is 3.3 V.

Figure 8-16. AVDD Linear Regulator Block Diagram

<!-- image -->

Use Equation 1 to calculate the power dissipated in the device by the AVDD linear regulator.

<!-- formula-not-decoded -->

The supply input voltage for AVDD regulator (VIN\_AVDD) can be same as VM supply voltage, or lower or higher than VM supply voltage.

## 8.3.5 Charge Pump

Because the output stages use N-channel FETs, the device requires a gate-drive voltage higher than the VM power supply to enhance the high-side FETs fully. The DRV8311 integrates a charge pump circuit that generates a voltage above the VM supply for this purpose.

The charge pump requires a single external capacitor for operation. See Table 8-1 for details on the capacitor value.

The charge pump shuts down when nSLEEP is low.

<!-- image -->

Figure 8-17. DRV8311 Charge Pump

<!-- image -->

<!-- image -->

## 8.3.6 Slew Rate Control

An adjustable gate-drive current control to the MOSFETs allows for easy slew rate control. The MOSFET VDS slew rates are a critical factor for optimizing radiated emissions, energy and duration of diode recovery spikes and switching voltage transients related to parasitic. These slew rates are predominantly determined by the rate of gate charge to internal MOSFETs as shown in Figure 8-18.

Figure 8-18. Slew Rate Circuit Implementation

<!-- image -->

The slew rate of each half-bridge can be adjusted by SLEW pin in hardware device variant or by using SLEW register settings in SPI device variant. The slew rate is calculated by the rise-time and fall-time of the voltage on OUTx pin as shown in Figure 8-19.

Figure 8-19. Slew Rate Timings

<!-- image -->

## 8.3.7 Cross Conduction (Dead Time)

The device  is  fully  protected  from  cross  conduction  of  MOSFETs.  The  high-side  and  low-side  MOSFETs  are operated to avoid any shoot through currents by inserting a dead time (tDEAD). This is implemented by sensing the  gate-source  voltage  (VGS)  of  the  high-side  and  low-side  MOSFETs  and  ensured  that  VGS  of  high-side MOSFET  has  reached  below  turn-off  levels  before  switching  on  the  low-side  MOSFET  of  same  half-bridge as  shown  in  Figure  8-20  and  Figure  8-21.  The  VGS  of  the  high-side  and  low-side  MOSFETs  (VGS\_HS  and VGS\_LS) shown in Figure 8-21 are DRV8311 internal signals.

<!-- image -->

Figure 8-20. Cross Conduction Protection

<!-- image -->

Figure 8-21. Dead Time

<!-- image -->

<!-- image -->

## 8.3.8 Propagation Delay

The  propagation  delay  time  (t pd )  is  measured  as  the  time  between  an  input  logic  edge  to  change  in  OUTx voltage. The propagation delay time includes the input deglitch delay, analog driver delay, and depends on the slew rate setting . The input deglitcher prevents high-frequency noise on the input pins from affecting the output state of the gate drivers. To support multiple control modes, a small digital delay is added as the input command propagates through the device.

Figure 8-22. Propagation Delay

<!-- image -->

## 8.3.9 Pin Diagrams

This section presents the I/O structure of all digital input and output pins.

## 8.3.9.1 Logic Level Input Pin (Internal Pulldown)

Figure 8-23 shows the input structure for the logic levels pins INHx, INLx, nSLEEP, SCLK and SDI. The input can be driven with an external resistor to GND or an external logic voltage supply. It is recommended to pull these pins low in device sleep mode to reduce leakage current through the internal pull-down resistors.

Figure 8-23. Logic-Level Input Pin Structure

<!-- image -->

## 8.3.9.2 Logic Level Input Pin (Internal Pullup)

Figure 8-24 shows the input structure for the logic level pin nSCS. The input can be driven with an external resistor to GND or an external logic voltage supply .

Figure 8-24. nSCS Input Pin Structure

<!-- image -->

## 8.3.9.3 Open Drain Pin

Figure 8-25 shows the structure of the open-drain output pin nFAULT. The open-drain output requires an external pullup resistor to a logic voltage supply to function properly.

Figure 8-25. Open Drain Output Pin Structure

<!-- image -->

<!-- image -->

<!-- image -->

## 8.3.9.4 Push Pull Pin

Figure 8-26 shows the structure of the push-pull pin SDO.

Figure 8-26. Push-Pull Output Pin Structure

<!-- image -->

## 8.3.9.5 Four Level Input Pin

Figure  8-27  shows  the  structure  of  the  four  level  input  pins  GAIN,  MODE  and  SLEW  on  hardware  interface devices. The input can be set by tying the pin to AGND or AVDD, leaving the pin unconnected, or connecting an external resistor from the pin to ground.

Figure 8-27. Four Level Input Pin Structure

<!-- image -->

## 8.3.10 Current Sense Amplifiers

The  DRV8311  integrate  three  high-performance  low-side  current  sense  amplifiers  for  current  measurements using  built-in  current  sense.  Low-side  current  measurements  are  commonly  used  to  implement  overcurrent protection, external torque control, or brushless DC commutation with the external controller. All three amplifiers can  be  used  to  sense  the  current  in  each  of  the  half-bridge  legs  (low-side  MOSFETs).  The  current  sense amplifiers include features such as programmable gain and an external voltage reference (VREF) provided on the pin CSAREF.

## 8.3.10.1 Current Sense Amplifier Operation

The SOx pin on the DRV8311 outputs an analog voltage proportional to current flowing in the low side FETs (I OUTx) multiplied by the gain setting (G CSA ). The gain setting is adjustable between four different levels which can be set by the GAIN pin (hardware device variant) or the CSA\_GAIN bits (SPI or tSPI device variant).

Figure 8-28 shows the internal architecture of the current sense amplifiers. The current sense is implemented with a sense FET on each low-side FET of the DRV8311 device. This current information is converted in to a voltage,  which generates the CSA output voltage on the SOx pin, based on the voltage on the CSAREF pin (VREF) and the gain setting. The CSA output voltage can be calculated using Equation 2

<!-- image -->

<!-- formula-not-decoded -->

Figure 8-28. Integrated Current Sense Amplifier

<!-- image -->

<!-- image -->

Figure 8-29 and Figure 8-30 show the details of the amplifier operational range. In bi-directional operation, the amplifier output for 0-V input is set at VREF/2. Any change in the differential input results in a corresponding change in the output times the GCSA factor. The amplifier has a defined linear region in which it can maintain operation.

<!-- image -->

0 V

Figure 8-29. Bidirectional Current Sense Output

<!-- image -->

Figure 8-30. Bidirectional Current Sense Regions

## Note

The current sense amplifiers uses the external voltage reference (VREF) provided at the CSAREF pin.

## 8.3.10.2 Current Sense Amplifier Offset Correction

CSA output has an offset induced due to ground differences between the sense FET and output FET. When running trapezoidal control or another single-shunt based control (sensored sine, for example) this CSA offset has  no  impact  to  operation.  When  running  sensorless  sinusoidal  or  FOC  control  where  two  or  three  current sense  are  required,  some  current  distortion  and  noise  may  occur  unless  the  user  implements  the  corrective action below.

Corrective Action : Implement the below equations in firmware to correct for any current induced offset:

<!-- image -->

<!-- formula-not-decoded -->

<!-- formula-not-decoded -->

<!-- formula-not-decoded -->

<!-- formula-not-decoded -->

<!-- formula-not-decoded -->

<!-- formula-not-decoded -->

<!-- formula-not-decoded -->

<!-- formula-not-decoded -->

<!-- formula-not-decoded -->

<!-- formula-not-decoded -->

<!-- image -->

## 8.3.11 Protections

The DRV8311 family of devices is protected against VM, VIN\_AVDD, AVDD and CP undervoltage, overcurrent and thermal events. Table 8-6 summarizes various faults details.

Table 8-6. Fault Action and Response

| FAULT                             | CONDITION                 | CONFIGURATION    | REPORT   | H-BRIDGE              | LOGIC                 | RECOVERY                                                          |
|-----------------------------------|---------------------------|------------------|----------|-----------------------|-----------------------|-------------------------------------------------------------------|
| VM undervoltage (NPOR)            | V VM < V UVLO             | -                | -        | Hi-Z                  | Disabled              | Automatic: V VM > V UVLO_R CLR_FLT, nSLEEP Reset Pulse (NPOR bit) |
| VINAVDD undervoltage (VINAVDD_UV) | V VINAVDD < V VINAVDD_UV  | -                | nFAULT   | Hi-Z                  | Active (SPI disabled) | Configured using UVP_MODE                                         |
| AVDD undervoltage (AVDD_UV)       | V AVDD < V AVDD_UV        | -                | nFAULT   | Hi-Z                  | Active (SPI disabled) | Configured using UVP_MODE                                         |
| Charge pump undervoltage (CP_UV)  | V CP < V CPUV             | -                | nFAULT   | Hi-Z                  | Active                | Configured using UVP_MODE                                         |
| CSAREF undervoltage (CSAREF_UV)   | V CSAREF < V CSAREF_UV    | CSAREFUV_EN= 1b  | nFAULT   | Active (CSA disabled) | Active                | Configured using UVP_MODE                                         |
| CSAREF undervoltage (CSAREF_UV)   | V CSAREF < V CSAREF_UV    | CSAREFUV_EN= 0b  | None     | Active                | Active                | No Action                                                         |
| Overcurrent protection (OCP)      | I PHASE > I OCP           | OCP_MODE = 000b  | nFAULT   | Hi-Z                  | Active                | Automatic Retry: SLOW_TRETRY                                      |
| Overcurrent protection (OCP)      | I PHASE > I OCP           | OCP_MODE = 001b  | nFAULT   | Hi-Z                  | Active                | Automatic Retry: FAST_TRETRY                                      |
| Overcurrent protection (OCP)      | I PHASE > I OCP           | OCP_MODE = 010b  | nFAULT   | Hi-Z                  | Active                | Latched: CLR_FLT, nSLEEP Reset Pulse                              |
| Overcurrent protection (OCP)      | I PHASE > I OCP           | OCP_MODE = 011b  | nFAULT   | Active                | Active                | No action                                                         |
| Overcurrent protection (OCP)      | I PHASE > I OCP           | OCP_MODE = 111b  | None     | Active                | Active                | No action                                                         |
| SPI fault (SPI_FLT)               | SCLK fault and ADDR fault | SPIFLT_MODE = 0b | nFAULT   | Active                | Active                | Automatic                                                         |
| SPI fault (SPI_FLT)               | SCLK fault and ADDR fault | SPIFLT_MODE = 1b | None     | Active                | Active                | No action                                                         |
| Thermal warning (OTW)             | T J > T OTW               | OTW_EN = 0b      | None     | Active                | Active                | No action                                                         |
| Thermal warning (OTW)             | T J > T OTW               | OTW_EN = 1b      | nFAULT   | Active                | Active                | Automatic: T J < T OTW - T HYS                                    |
| Thermal shutdown (OTSD)           | T J > T OTSD              | OTSD_MODE = 00b  | nFAULT   | Hi-Z                  | Active                | Automatic SLOW_TRETRY after T J < T OTSD - T HYS                  |
| Thermal shutdown (OTSD)           | T J > T OTSD              | OTSD_MODE = 01b  | nFAULT   | Hi-Z                  | Active                | Automatic FAST_TRETRY after T J < T OTSD - T HYS                  |

## 8.3.11.1 VM Supply Undervoltage Lockout (NPOR)

If  at  any  time  the  input  supply  voltage  on  the  VM  pin  falls  lower  than  the  VUVLO threshold (VM UVLO falling threshold), all of the integrated FETs, driver charge-pump and digital logic controller are disabled as shown in Figure 8-31. Normal operation resumes (driver operation) when the VM undervoltage condition is removed. The NPOR bit is reset and latched low in the device status (DEV\_STS1) register once the device presumes VM. The NPOR bit remains in reset condition until cleared through the CLR\_FLT bit or an nSLEEP pin reset pulse (tRST).

Figure 8-31. VM Supply Undervoltage Lockout

<!-- image -->

## 8.3.11.2 Under Voltage Protections (UVP)

Other  than  VM  ULVO,  DRV8311  family  of  devices  has  under  voltage  protections  for  VIN\_AVDD,  CSAREF, AVDD and CP pins. VINAVDD\_UV, CP\_UV and AVDD\_UV under voltage protections are enabled and cannot be  disabled,  while  CSAREF\_UV  is  disabled  by  default  and  can  be  enabled  in  SPI  variant  by  configuring CSAREFUV\_EN in SYSF\_CTRL register.

In hardware device variants, AVDD\_UV, VINAVDD\_UV, CP\_UV protections are enabled, while CSAREF\_UV is disabled and the t RETRY  is configured for fast automatic retry time to 5 ms.

t RETRY configuration for SPI device variant for all UV protections

- Slow retry time SLOW\_TRETRY can be used for tRETRY period by configuring UVP\_MODE to 000b
- Fast retry time FAST\_TRETRY can be used for tRETRY period by configuring UVP\_MODE to 001b

## VINAVDD Under Voltage Protections (VINAVDD\_UV)

If at any time the voltage on VIN\_AVDD pin falls lower than the V VINAVDD\_UV  threshold, all of the integrated FETs, SPI communication is disabled, nFAULT pin is driven low, FAULT and UVP in DEV\_STS1 and VINAVDD\_UV in  SUP\_STS  are  set  high.  Normal  operation  starts  again  automatically  (driver  operation,  the  nFAULT  pin  is released and VINAVDD\_UV bit is cleared) after VIN\_AVDD pin rises above the VVINAVDD\_UV threshold and the tRETRY time elapses. The FAULT and UVP bits stay latched high until a clear fault command is issued either through the CLR\_FLT bit or an nSLEEP reset pulse (tRST).

## AVDD Under Voltage Protections (AVDD\_UV)

If  at  any  time  the  voltage  on  AVDD  pin  falls  lower  than  the  V AVDD\_UV  threshold,  all  of  the  integrated  FETs, SPI  communication  is  disabled,  nFAULT  pin  is  driven  low,  FAULT  and  UVP  in  DEV\_STS1  and  AVDD\_UV in  SUP\_STS  are  set  high.  Normal  operation  starts  again  automatically  (driver  operation,  the  nFAULT  pin  is released and AVDD\_UV bit is cleared) after AVDD pin rises above the VAVDD\_UV threshold and the tRETRY time elapses. The FAULT and UVP bits stay latched high until a clear fault command is issued either through the CLR\_FLT bit or an nSLEEP reset pulse (tRST).

<!-- image -->

<!-- image -->

[www.ti.com](https://www.ti.com/)

## CSAREF Under Voltage Protections (CSAREF\_UV)

If at any time the voltage on CSAREF pin falls lower than the V CSAREF\_UV  threshold, CSAREF\_UV is recognized. CSA\_UV can be enabled or disabled by configuring CSAREFUV\_EN. When enabled, after CSAREF\_UV event, CSA are disabled, nFAULT pin is driven low, FAULT and UVP in DEV\_STS1 and CSAREF\_UV in SUP\_STS are  set  high.  Normal  operation  starts  again  automatically  (CSA  operation,  the  nFAULT  pin  is  released  and CSAREF\_UV bit is cleared) after CSAREF\_UV condition is cleared and the tRETRY time elapses. The FAULT and UVP bits stay latched high until a clear fault command is issued either through the CLR\_FLT bit or an nSLEEP reset pulse (t RST).

## Note

CSAREF\_UV is disabled in hardware variant and by default in SPI variants

## CP Under Voltage Protections (CP\_UV)

If at any time the voltage on CP pin falls lower than the V CP\_UV  threshold, all of the integrated FETs and charge pump operation is disabled, nFAULT pin is driven low, FAULT and UVP in DEV\_STS1 and CP\_UV in SUP\_STS are set high. Normal operation starts again automatically (driver and charge pump operation, the nFAULT pin is released and CP\_UV bit is cleared) after CP pin rises above the VCP\_UV threshold and the tRETRY time elapses. The FAULT and UVP bits stay latched high until a clear fault command is issued either through the CLR\_FLT bit or an nSLEEP reset pulse (tRST).

## 8.3.11.3 Overcurrent Protection (OCP)

A MOSFET overcurrent event is sensed by monitoring the current flowing through FETs. If the current through a FET exceeds the IOCP threshold for longer than the tOCP deglitch time, a OCP event is recognized and action is done according to the OCP\_MODE bit. In order to avoid false trigger of OCP during PWM transition due to ringing in phase voltage, there is t BLANK  blanking time applied at each edge of PWM signals in digital. During blanking time, OCP events are ignored.

On hardware device variants, the IOCP threshold is 5A or 9A (typ) based on MODE pin, the tOCP\_DEG is fixed at 1-µs, tBLANK is fixed at 0.2-µs and the OCP\_MODE bit is configured with fast retry with 5-ms automatic retry. On SPI devices, the IOCP threshold is set through the OCP\_LVL, the tOCP\_DEG is set through the OCP\_DEG, the tBLANK is set through the OCP\_TBLANK and the OCP\_MODE bit can operate in four different modes: OCP latched shutdown, OCP automatic retry with fast and slow retry times, OCP report only, and OCP disabled.

## 8.3.11.3.1 OCP Latched Shutdown (OCP\_MODE = 010b)

After a OCP event in this mode, all MOSFETs are disabled and the nFAULT pin is driven low. The FAULT, OCP, and corresponding FET's OCP bits are latched high in the SPI registers. Normal operation starts again (driver operation, FAULT, OCP, and corresponding FET's OCP bits are cleared and the nFAULT pin is released) when the OCP condition clears and a clear faults command is issued either through the CLR\_FLT bit or an nSLEEP reset pulse (t RST).

<!-- image -->

<!-- image -->

Figure 8-32. Overcurrent Protection - Latched Shutdown Mode

<!-- image -->

## 8.3.11.3.2 OCP Automatic Retry (OCP\_MODE = 000b or 001b)

After a OCP event in this mode, all the FETs are disabled and the nFAULT pin is driven low. The FAULT, OCP, and corresponding FET's OCP bits are set high in the SPI registers. Normal operation starts again automatically (driver operation, the nFAULT pin is released and corresponding FET's OCP bits are cleared) after the tRETRY time elapses. The FAULT and OCP stays latched high until clear faults command is issued either through the CLR\_FLT bit or an nSLEEP reset pulse (tRST).

## tRETRY configuration

- Slow retry time SLOW\_TRETRY can be used for tRETRY period by configuring OCP\_MODE to 000b
- Fast retry time FAST\_TRETRY can be used for tRETRY period by configuring OCP\_MODE to 001b

Figure 8-33. Overcurrent Protection - Automatic Retry Mode

<!-- image -->

<!-- image -->

[www.ti.com](https://www.ti.com/)

## 8.3.11.3.3 OCP Report Only (OCP\_MODE = 011b)

No protective action occurs after a OCP event in this mode. The overcurrent event is reported by driving the nFAULT pin  low  and  setting  the  FAULT,  OCP,  and  corresponding  FET's  OCP  bits  high  in  the  SPI  registers. DRV8311 continue to operate  as  usual.  The  external  controller  manages  the  overcurrent  condition  by  acting appropriately. The reporting clears (nFAULT pin is released, FAULT, OCP, and corresponding FET's OCP bits are cleared) when the OCP condition clears and a clear faults command is issued either through the CLR\_FLT bit or an nSLEEP reset pulse (tRST).

## 8.3.11.3.4 OCP Disabled (OCP\_MODE = 111b)

No action occurs after a OCP event in this mode.

## 8.3.11.4 Thermal Protections

DRV8311 family of devices has over temperature warning (OTW) and over temperature shutdown (OTSD) for over temperature events.

## 8.3.11.4.1 Thermal Warning (OTW)

If  the  die  temperature  exceeds  the  trip  point  of  the  thermal  warning  (T OTW),  the  OT  bit  in  the  device  status (DEV\_STS1) register and OTW bit in the OT\_STS status register is set. The reporting of OTW on the nFAULT pin can be enabled by setting the over-temperature warning reporting (OTW\_EN) bit in the configuration control register.  The  device  performs  no  additional  action  and  continues  to  function.  In  this  case,  the  nFAULT  pin releases and OTW bit cleared when the die temperature decreases below the hysteresis point of the thermal warning (TOTW\_HYS). The OT bit remains latched until  cleared  through  the  CLR\_FLT  bit  or  an  nSLEEP  reset pulse (t RST) and the die temperature is lower than thermal warning trip (T OTW ).

In hardware device variants, Over Temperature warning is not reported on nFAULT pin by default.

## 8.3.11.4.2 Thermal Shutdown (OTSD)

If the die temperature exceeds the trip point of the thermal shutdown limit (T OTS ), all the FETs are disabled, the charge pump is shut down, and the nFAULT pin is driven low. In addition, the FAULT and OT bit in the OT bit in the device status (DEV\_STS1) register and OTSD bit in the OT\_STS status register is set. Normal operation starts  again  (driver  operation  the  nFAULT  pin  is  released  and  OTSD  bit  cleared)  when  the  overtemperature condition clears and after the t RETRY  time elapses. The OT and FAUTL bits stay latched high indicating that a thermal event occurred until a clear fault command is issued either through the CLR\_FLT bit or an nSLEEP reset pulse (t RST). This protection feature cannot be disabled.

On hardware device variants the tRETRY period is fixed to fast retry time of 5ms t RETRY configuration for SPI device variant

- Slow retry time SLOW\_TRETRY can be used for tRETRY period by configuring OTSD\_MODE to 00b
- Fast retry time FAST\_TRETRY can be used for tRETRY period by configuring OTSD\_MODE to 01b

## 8.4 Device Functional Modes

## 8.4.1 Functional Modes

## 8.4.1.1 Sleep Mode

The nSLEEP pin manages the state of the DRV8311 family of devices. When the nSLEEP pin is low, the device goes to a low-power sleep mode. In sleep mode, all FETs are disabled, sense amplifiers are disabled, the charge pump is disabled, the AVDD regulator is disabled, and the SPI bus is disabled. The t SLEEP  time must elapse after a falling edge on the nSLEEP pin before the device goes to sleep mode. The device comes out of sleep mode automatically if the nSLEEP pin is pulled high. The t WAKE  time must elapse before the device is ready for inputs.

In sleep mode and when VVM &lt; VUVLO, all MOSFETs are disabled.

## Note

During power up and power down of the device through the nSLEEP pin, the nFAULT pin is held low as the internal regulators are enabled or disabled. After the regulators have enabled or disabled, the nFAULT pin is automatically released. The duration that the nFAULT pin is low does not exceed the t SLEEP or tWAKE time.

## 8.4.1.2 Operating Mode

When  the  nSLEEP  pin  is  high  and  the  V VM   voltage  is  greater  than  the  V UVLO  voltage,  the  device  goes  to operating  mode.  The  t WAKE   time  must  elapse  before  the  device  is  ready  for  inputs.  In  this  mode  the  charge pump, AVDD regulator and SPI bus are active.

## 8.4.1.3 Fault Reset (CLR\_FLT or nSLEEP Reset Pulse)

In  the  case  of  device  latched  faults,  the  DRV8311  family  of  devices  goes  to  a  partial  shutdown  state  to  help protect the power MOSFETs and system.

When the fault condition clears, the device can go to the operating state again by either setting the CLR\_FLT SPI  bit  on  SPI  devices  or  issuing  a  reset  pulse  to  the  nSLEEP  pin  on  either  interface  variant.  The  nSLEEP reset pulse (t RST) consists of a high-to-low-to-high transition on the nSLEEP pin. The low period of the sequence should fall with the tRST time window or else the device will start the complete shutdown sequence. The reset pulse has no effect on any of the regulators, device settings, or other functional blocks

<!-- image -->

<!-- image -->

## 8.5 SPI Communication

## 8.5.1 Programming

## 8.5.1.1 SPI and tSPI Format

## SPI Format - with Parity

The SDI input data word is 24 bits long and consists of the following format:

- 1 read or write bit, W (bit B23)
- 6 address bits, A (bits B22 through B17)
- Parity bit, P (bit B16)
- 15 data bits with 1 parity bit, D (bits B15 through B0)

The SDO output data word is 24 bits long. The most significant bits are status bits and the least significant 16 bits are the data content of the register being accessed.

Table 8-7. SDI Input Data Word Format for SPI

| R/W   | ADDRESS   | ADDRESS   | ADDRESS   | ADDRESS   | ADDRESS   | PAR ITY PAR ITY   | PAR ITY PAR ITY   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   |
|-------|-----------|-----------|-----------|-----------|-----------|-------------------|-------------------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|
| B23   | B22       | B21       | B20       | B19       | B18       | B17               | B16               | B15    | B14    | B13    | B12    | B11    | B10    | B9     | B8     | B7     | B6     | B5     |        | B4     | B3     | B2     | B1     | B0     |
| W0    | A5        | A4        | A3        | A2        | A1        | A0                | P                 | P      | D14    | D13    | D12    | D11    | D10    | D9     | D8     | D7     | D6     |        | D5     | D4 D3  |        | D2     | D1     | D0     |

## Table 8-8. SDO Output Data Word Format

| STATUS   | STATUS   | STATUS   | STATUS   | STATUS   | STATUS   | STATUS   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   | DATA   |
|----------|----------|----------|----------|----------|----------|----------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------|
| B23      | B22      | B21      | B20      | B19      | B18      | B17      | B16    | B15    | B14    | B13    | B12    | B11    | B10    | B9     | B8     | B7     | B6     | B5     | B3     | B2     |        | B1     | B0     |
| S7       | S6       | S5       | S4       | S3       | S2       | S1       | S0     | D15    | D14    | D13    | D12    | D11    | D10    | D9     | D8     | D7     | D6     | D5     | D3     |        | D2     | D1     | D0     |

## tSPI Format - with Parity

The SDI input data word is 32 bits long and consists of the following format:

- 1 read or write bit, W (bit B31)
- 4 secondary device ID bits, AD (bits B30 through B27)
- 8 address bits, A (bits B26 through B19)
- 2 reserved bits, 0 (bits B18, bit B17)
- Parity bit, P (bit B16)
- 15 data bits with 1 parity bit, D (bits B15 through B0)

The SDO output data word is 24 bits long. The first 8 bits are status bits and the last 16 bits are the data content of the register being accessed. The format is same as standard SPI shown in Table 8-8

## Table 8-9. tSPI with Parity - SDI Input Data Word Format

| R/W   | Secondary device ID   | Secondary device ID   | Secondary device ID   | Secondary device ID   | ADDRESS   | 00   | 00   | PARITY   | PARITY   | DATA     |
|-------|-----------------------|-----------------------|-----------------------|-----------------------|-----------|------|------|----------|----------|----------|
| B31   | B30                   | B29                   | B28                   | B27                   | B26 - B19 | B18  | B17  | B16      | B15      | B14 - B0 |
| W0    | 0                     | 0                     | AD1                   | AD0                   | A7 - A0   | 0    | 0    | P        | P        | D14 - D0 |

The details of the bits used in SPI and tSPI frame format are detailed below.

Read/Write Bit (R/W) : R/W (W0) bit being 0 indicates a SPI/tSPI Write transaction. For a read operation RW bit needs to be 1.

Secondary device ID Bits (AD) : Each tSPI secondary device on the same chip select should have a unique identifier. Secondary device ID field is the 4-bit unique identifier of the tSPI secondary device. For a successful Read/Write  transaction  the  secondary  device  ID  field  should  match  with  the  secondary  device  address.  In DRV8311 the two most significant bits of secondary device addresses are set to 00. The least two significant bits  of  the  secondary  device address can be configured using the AD1 and AD0 pins. The secondary device address 15 (0xF) is reserved for general call, all the devices on the same bus accept a write operation when

<!-- image -->

the secondary device ID field is set to 15. Hence the valid tSPI secondary device addresses for DRV8311 range from 0 to 3 and 15 (general call address).

Address Bits (A) : A tSPI secondary device takes 8-bit register address whereas SPI secondary device takes 6-bit register address. Each tSPI secondary device has two dedicated 8-bit address pointers, one for read and one for  write.  During  a  sequential  read  transaction,  the  read  address  pointer  gets  incremented  automatically. During a sequential write transaction, both write address pointer and read address pointer will be incremented automatically.

Parity Bit (P) : Both header and data fields of a SPI/tSPI input data frame include a parity bit for single bit error detection. The parity scheme used is even parity i.e., the number of ones in a block of 16-bits (including the parity bit) is even. Data will be written to the internal registers only if the parity check is successful. During a read operation, the tSPI secondary device inserts a parity bit at the MSB of read data. Parity checks can be enabled or disabled by configuring the SPI\_PEN bit of SYS\_CTRL register. Parity checks are disabled by default.

Note

Though parity checks are disabled by default, TI recommends enabling parity checks to safeguard against single-bit errors.

## Error Handling

Parity Error :  Upon detecting a parity error, the secondary device responds in the following ways. Parity error gets latched and reported on nFAULT. The error status is available for read on SPI\_PARITY field of SYS\_STS register.  A  parity  error  in  the  header  will  not  prevent  the  secondary  device  from  responding  with  data.  The SDO will be driven by the secondary device being addressed. Updates to write address pointer and the device registers will be ignored when parity error is detected. In a sequential write, upon detection of parity error any subsequent register writes will be ignored.

Frame Error :Any incomplete tSPI Frame will be reported as Frame error. If the number of tSPI clock cycles is not a multiple of 16, then the transfer is considered to be incomplete. Frame errors will be latched in FRM\_ERR field of SYS\_STS register and indicated on nFAULT.

## SPI Read / Write Sequence

SPI Read Sequence :  The  SPI  read  transaction  comprises  of  an  8-bit  header  (R/W  -  1  bit,  Address  -  6  bits, and party -1 bit) followed by 16-bit dummy data words. Upon receiving the first byte of header, the secondary device  responds  with  an  8-bit  device  status  information.  The  read  address  pointer  gets  updated  immediately after receiving the address field of the header. The read address from the header acts as the starting address for the register reads. The read address pointer gets incremented automatically upon completion of a 16-bit transfer. The length of data transfer is not restricted by the secondary device. The secondary device responds with data as long as the primary device transmits dummy words. If parity error check is enabled, the MSB of read data will be replaced with computed parity bit

SPI Write Sequence :  SPI  write  transaction comprises of an 8-bit header followed by 16-bit data words to be written into the register bank. Similar to a read transaction, the addressed secondary device responds with an 8-bit  device  status  information  upon  receiving  the  first  byte  of  header.  Once  the  header  bytes  are  received, the  write  address  pointer  gets  updated.  The  write  address  from  the  header  acts  as  the  starting  address  for sequential  register  writes.  The  read  address  pointer  will  retain  the  address  of  the  register  being  read  in  the previous tSPI transaction. The length of data transfer is not restricted by the secondary device. Both read and write address pointers will be incremented automatically upon completion of a 16-bit transfer. While receiving data from the primary device, the SDO will be driven with the register data addressed by read address pointer.

## tSPI Communication Sequence

The tSPI interface is similar to regular SPI interface in functionality but add support for multiple devices under the  same Chip Select (nSCS). Any existing SPI primary device would be able to communicate with the tSPI secondary devices with modifications in the frame format. A valid tSPI frame must meet the following conditions (similar to SPI interface):

<!-- image -->

## [www.ti.com](https://www.ti.com/)

- The SCLK pin should be low when the nSCS pin transitions from high to low and from low to high. A high to low transition at the nSCS pin is the start of frame and a low to high transition is the end of the frame.
- When the nSCS pin is pulled high, any signals at the SCLK and SDI pins are ignored and the SDO pin is placed in the Hi-Z state.
- Data is captured on the falling edge of the SCLK signal and data is driven on the rising edge of the SCLK signal.
- The most significant bit (MSB) is shifted in and out first.
- A minimum of 16 SCLK cycles must occur for transaction to be valid &amp; the number of SCLK cycles in a single transaction must me a multiple of 16.
- If the data word sent to the SDI pin is not a multiple of 16 bits, a frame error occurs and the excess SCLK cycles are ignored.

Figure 8-34. tSPI Block Diagram with Multiple Devices on Same Chip Select

<!-- image -->

Figure 8-35. tSPI with PWM\_SYNC

<!-- image -->

tSPI Read Sequence : A tSPI read transaction has a 16-bit header (R/W - 1 bit, Secondary device ID - 4 bits, Address - 8 bits, reserved -2 bits and party -1 bit) followed by 16-bit dummy data words. Upon receiving the first byte of header, the secondary device being addressed with matching secondary device ID field (configured using AD0 and AD1 pins), responds with an 8-bit device status information. The read address from the header acts as the starting address for the register reads. The address gets incremented automatically upon completion of a 16-bit transfer. The length of data transfer is not restricted by the secondary device. The secondary device responds with data as long as the primary device transmits dummy words. If parity error check is enabled, the MSB of read data will be replaced with computed parity bit.

tSPI Write Sequence : A tSPI write transaction has a 16-bit header followed by 16-bit data words to be written into  the  register  bank.  Similar  to  a  read  transaction,  the  addressed  secondary  device  responds  with  an  8-bit device  status  information  upon  receiving  the  first  byte  of  header.  The  write  address  from  the  header  acts  as the starting address for sequential register writes. The length of data transfer is not restricted by the secondary device.  Both  write  and  read  address  pointers  will  be  incremented  automatically  upon  completion  of  a  16-bit transfer. While receiving data from the primary device, the SDO will be driven with the register data addressed by read address pointer tSPI  Read  Address  Update  Sequence :  The  independent  read  and  write  address  pointers  in  the  secondary device  would  allow  reading  data  from  one  set  of  registers  while  writing  data  to  another  set  of  registers.  To achieve this, the primary device should first send a read address update frame before the tSPI write transaction. A read address frame is nothing but just the tSPI read sequence with just the header. The first tSPI transaction

<!-- image -->

<!-- image -->

updates the read address pointer to desired register address. The second tSPI transaction is a register write sequence.  During  this  sequence,  the  data  send  on  SDO  by  the  secondary  device  will  be  from  the  register pointed by read address pointer which was initialized in the previous tSPI read sequence.

The  tSPI  read/write  sequence  with  parity  is  shown  in  Figure  8-36.  The  SPI  frame  header  is  marked  as CMD[15:8] and CMD[7:0].

Figure 8-36. tSPI Read/Write with Parity

<!-- image -->

## 9 DRV8311 Registers

DRV8311 Registers lists the memory-mapped registers for the DRV8311 registers. All register offset addresses not listed in DRV8311 Registers should be considered as reserved locations and the register contents should not be modified.

Table 9-1. DRV8311 Registers

| Offset   | Acronym      | Register Name                    | Section      |
|----------|--------------|----------------------------------|--------------|
| 0h       | DEV_STS1     | Device Status 1 Register         | Section 9.1  |
| 4h       | OT_STS       | Over Temperature Status Register | Section 9.2  |
| 5h       | SUP_STS      | Supply Status Register           | Section 9.3  |
| 6h       | DRV_STS      | Driver Status Register           | Section 9.4  |
| 7h       | SYS_STS      | System Status Register           | Section 9.5  |
| Ch       | PWM_SYNC_PRD | PWM Sync Period Register         | Section 9.6  |
| 10h      | FLT_MODE     | Fault Mode Register              | Section 9.7  |
| 12h      | SYSF_CTRL    | System Fault Control Register    | Section 9.8  |
| 13h      | DRVF_CTRL    | Driver Fault Control Register    | Section 9.9  |
| 16h      | FLT_TCTRL    | Fault Timing Control Register    | Section 9.10 |
| 17h      | FLT_CLR      | Fault Clear Register             | Section 9.11 |
| 18h      | PWMG_PERIOD  | PWM_GEN Period Register          | Section 9.12 |
| 19h      | PWMG_A_DUTY  | PWM_GEN A Duty Register          | Section 9.13 |
| 1Ah      | PWMG_B_DUTY  | PWM_GEN B Duty Register          | Section 9.14 |
| 1Bh      | PWMG_C_DUTY  | PWM_GEN C Duty Register          | Section 9.15 |
| 1Ch      | PWM_STATE    | PWM State Register               | Section 9.16 |
| 1Dh      | PWMG_CTRL    | PWM_GEN Control Register         | Section 9.17 |
| 20h      | PWM_CTRL1    | PWM Control Register 1           | Section 9.18 |
| 22h      | DRV_CTRL     | Predriver control Register       | Section 9.19 |
| 23h      | CSA_CTRL     | CSA Control Register             | Section 9.20 |
| 3Fh      | SYS_CTRL     | System Control Register          | Section 9.21 |

Complex bit access types are encoded to fit into small  table  cells.  DRV8311  Access  Type  Codes  shows  the codes that are used for access types in this section.

Table 9-2. DRV8311 Access Type Codes

| Access Type            | Code                   | Description                            |
|------------------------|------------------------|----------------------------------------|
| Read Type              | Read Type              | Read Type                              |
| R                      | R                      | Read                                   |
| R-0                    | R -0                   | Read Returns 0s                        |
| Write Type             | Write Type             | Write Type                             |
| W                      | W                      | Write                                  |
| Reset or Default Value | Reset or Default Value | Reset or Default Value                 |
| - n                    |                        | Value after reset or the default value |

<!-- image -->

<!-- image -->

## 9.1 DEV\_STS1 Register (Offset = 0h) [Reset = 0080h]

DEV\_STS1 is shown in DEV\_STS1 Register and described in DEV\_STS1 Register Field Descriptions.

Return to the DRV8311 Registers.

Device Status 1 Register

Figure 9-1. DEV\_STS1 Register

| 15         | 14       | 13       | 12       | 11       | 10       | 9        | 8       |
|------------|----------|----------|----------|----------|----------|----------|---------|
| Parity_bit | RESERVED | RESERVED | RESERVED | RESERVED | RESERVED | RESERVED | OTP_FLT |
| R-0h       | R-0-0h   | R-0-0h   | R-0-0h   | R-0-0h   | R-0-0h   | R-0-0h   | R-0h    |
| 7          | 6        | 5        | 4        | 3        | 2        | 1        | 0       |
| RESET      | SPI_FLT  | OCP      | RESERVED | RESERVED | UVP      | OT       | FAULT   |
| R-1h       | R-0h     | R-0h     | R-0h     | R-0h     | R-0h     | R-0h     | R-0h    |

## Table 9-3. DEV\_STS1 Register Field Descriptions

| Bit   | Field      | Type   | Reset   | Description                                                                                                                                                                   |
|-------|------------|--------|---------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                                                                                        |
| 14-9  | RESERVED   | R-0    | 0h      | Reserved                                                                                                                                                                      |
| 8     | OTP_FLT    | R      | 0h      | OTP read fault 0h = No OTP read fault is detected 1h = OTP read fault detected                                                                                                |
| 7     | RESET      | R      | 1h      | Supply Power On Reset Status 0h = No power on reset condition is detected 1h = Power-on-reset condition is detected                                                           |
| 6     | SPI_FLT    | R      | 0h      | SPI Fault Status 0h = No SPI communication fault is detected 1h = SPI communication fault is detected                                                                         |
| 5     | OCP        | R      | 0h      | Driver Overcurrent Protection Status 0h = No overcurrent condition is detected 1h = Overcurrent condition is detected                                                         |
| 4-3   | RESERVED   | R      | 0h      | Reserved                                                                                                                                                                      |
| 2     | UVP        | R      | 0h      | Supply Undervoltage Status 0h = No undervoltage voltage condition is detected on CP, AVDD or VIN_AVDD 1h = Undervoltage voltage condition is detected on CP, AVDD or VIN_AVDD |
| 1     | OT         | R      | 0h      | Overtemperature Fault Status 0h = No overtemperature warning / shutdown is detected 1h = Overtemperature warning / shutdown is detected                                       |
| 0     | FAULT      | R      | 0h      | Device Fault Status 0h = No fault condition is detected 1h = Fault condition is detected                                                                                      |

## 9.2 OT\_STS Register (Offset = 4h) [Reset = 0000h]

OT\_STS is shown in OT\_STS Register and described in OT\_STS Register Field Descriptions.

Return to the DRV8311 Registers.

Over Temperature Status Register

## Figure 9-2. OT\_STS Register

| 15         |   14 | 13       |   12 | 11       | 10       | 9    | 8    |
|------------|------|----------|------|----------|----------|------|------|
| Parity_bit |      |          |      | RESERVED |          |      |      |
| R-0h       |      |          |      | R-0-0h   |          |      |      |
| 7          |    6 | 5        |    4 | 3        | 2        | 1    | 0    |
|            |      | RESERVED |      |          | OTS_AVDD | OTW  | OTSD |
|            |      | R-0-0h   |      |          | R-0h     | R-0h | R-0h |

## Table 9-4. OT\_STS Register Field Descriptions

| Bit   | Field      | Type   | Reset   | Description                                                                                                                                      |
|-------|------------|--------|---------|--------------------------------------------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                                                           |
| 14-3  | RESERVED   | R-0    | 0h      | Reserved                                                                                                                                         |
| 2     | OTS_AVDD   | R      | 0h      | AVDD LDO Overtemperature Fault Status 0h = No overtemperature shutdown near AVDD is detected 1h = Overtemperature shutdown near AVDD is detected |
| 1     | OTW        | R      | 0h      | Overtemperature Warning Status 0h = No overtemperature warning is detected 1h = Overtemperature warning is detected                              |
| 0     | OTSD       | R      | 0h      | Overtemperature Shutdown Fault Status 0h = No overtemperature shutdown is detected 1h = Overtemperature shutdown is detected                     |

<!-- image -->

<!-- image -->

## 9.3 SUP\_STS Register (Offset = 5h) [Reset = 0000h]

SUP\_STS is shown in SUP\_STS Register and described in SUP\_STS Register Field Descriptions.

Return to the DRV8311 Registers.

Supply Status Register

## Figure 9-3. SUP\_STS Register

| 15         | 14       | 13        | 12    | 11       | 10      | 9        | 8          |
|------------|----------|-----------|-------|----------|---------|----------|------------|
| Parity_bit |          |           |       | RESERVED |         |          |            |
| R-0h       |          |           |       | R-0-0h   |         |          |            |
| 7          | 6        | 5         | 4     | 3        | 2       | 1        | 0          |
| RESERVED   | RESERVED | CSAREF_UV | CP_UV | RESERVED | AVDD_UV | RESERVED | VINAVDD_UV |
| R-0-0h     | R-0-0h   | R-0h      | R-0h  | R-0-0h   | R-0h    | R-0-0h   | R-0h       |

## Table 9-5. SUP\_STS Register Field Descriptions

| Bit   | Field      | Type   | Reset   | Description                                                                                                                           |
|-------|------------|--------|---------|---------------------------------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                                                |
| 14-6  | RESERVED   | R-0    | 0h      | Reserved                                                                                                                              |
| 5     | CSAREF_UV  | R      | 0h      | CSA REF Undervoltage Fault Status 0h = No CSAREF undervoltage is detected 1h = CSAREF undervoltage is detected                        |
| 4     | CP_UV      | R      | 0h      | Charge Pump Undervoltage Fault Status 0h = No charge pump undervoltage is detected 1h = Charge pump undervoltage is detected          |
| 3     | RESERVED   | R-0    | 0h      | Reserved                                                                                                                              |
| 2     | AVDD_UV    | R      | 0h      | AVDD LDO Undervoltage Fault Status 0h = No AVDD output undervoltage is detected 1h = AVDD output undervoltage is detected             |
| 1     | RESERVED   | R-0    | 0h      | Reserved                                                                                                                              |
| 0     | VINAVDD_UV | R      | 0h      | VIN_AVDD Undervoltage Fault Status 0h = No AVDD supply input undervoltage is detected 1h = AVDD supply input undervoltage is detected |

## 9.4 DRV\_STS Register (Offset = 6h) [Reset = 0000h]

DRV\_STS is shown in DRV\_STS Register and described in DRV\_STS Register Field Descriptions.

Return to the DRV8311 Registers.

Driver Status Register

| 15         | 14       | 13       | 12       | 11       | 10       | 9        | 8        |
|------------|----------|----------|----------|----------|----------|----------|----------|
| Parity_bit | RESERVED | RESERVED | RESERVED | RESERVED | RESERVED | RESERVED | RESERVED |
| R-0h       | R-0-0h   | R-0-0h   | R-0-0h   | R-0-0h   | R-0-0h   | R-0-0h   | R-0-0h   |
| 7          | 6        | 5        | 4        | 3        | 2        | 1        | 0        |
| RESERVED   | OCPC_HS  | OCPB_HS  | OCPA_HS  | RESERVED | OCPC_LS  | OCPB_LS  | OCPA_LS  |
| R-0-0h     | R-0h     | R-0h     | R-0h     | R-0-0h   | R-0h     | R-0h     | R-0h     |

## Figure 9-4. DRV\_STS Register

## Table 9-6. DRV\_STS Register Field Descriptions

| Bit   | Field      | Type   | Reset   | Description                                                                                                                                                   |
|-------|------------|--------|---------|---------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                                                                        |
| 14-7  | RESERVED   | R-0    | 0h      | Reserved                                                                                                                                                      |
| 6     | OCPC_HS    | R      | 0h      | Overcurrent Status on High-side MOSFET of OUTC 0h = No overcurrent detected on high-side MOSFET of OUTC 1h = Overcurrent detected on high-side MOSFET of OUTC |
| 5     | OCPB_HS    | R      | 0h      | Overcurrent Status on High-side MOSFET of OUTB 0h = No overcurrent detected on high-side MOSFET of OUTB 1h = Overcurrent detected on high-side MOSFET of OUTB |
| 4     | OCPA_HS    | R      | 0h      | Overcurrent Status on High-side MOSFET of OUTA 0h = No overcurrent detected on high-side MOSFET of OUTA 1h = Overcurrent detected on high-side MOSFET of OUTA |
| 3     | RESERVED   | R-0    | 0h      | Reserved                                                                                                                                                      |
| 2     | OCPC_LS    | R      | 0h      | Overcurrent Status on Low-side MOSFET of OUTC 0h = No overcurrent detected on low-side MOSFET of OUTC 1h = Overcurrent detected on low-side MOSFET of OUTC    |
| 1     | OCPB_LS    | R      | 0h      | Overcurrent Status on Low-side MOSFET of OUTB 0h = No overcurrent detected on low-side MOSFET of OUTB 1h = Overcurrent detected on low-side MOSFET of OUTB    |
| 0     | OCPA_LS    | R      | 0h      | Overcurrent Status on Low-side MOSFET of OUTA 0h = No overcurrent detected on low-side MOSFET of OUTA 1h = Overcurrent detected on low-side MOSFET of OUTA    |

<!-- image -->

<!-- image -->

## 9.5 SYS\_STS Register (Offset = 7h) [Reset = 0000h]

SYS\_STS is shown in SYS\_STS Register and described in SYS\_STS Register Field Descriptions.

Return to the DRV8311 Registers.

System Status Register

| 15         | 14       |   13 | 12        | 11       | 10         | 9       | 8       |
|------------|----------|------|-----------|----------|------------|---------|---------|
| Parity_bit |          |      |           | RESERVED |            |         |         |
| R-0h       |          |      |           | R-0-0h   |            |         |         |
| 7          | 6        |    5 | 4         | 3        | 2          | 1       | 0       |
|            | RESERVED |      | OTPLD_ERR | RESERVED | SPI_PARITY | BUS_CNT | FRM_ERR |
|            | R-0-0h   |      | R-0h      | R-0-0h   | R-0h       | R-0h    | R-0h    |

## Figure 9-5. SYS\_STS Register

## Table 9-7. SYS\_STS Register Field Descriptions

| Bit   | Field      | Type   | Reset   | Description                                                                                                     |
|-------|------------|--------|---------|-----------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                          |
| 14-5  | RESERVED   | R-0    | 0h      | Reserved                                                                                                        |
| 4     | OTPLD_ERR  | R      | 0h      | OTP Read Error 0h = No OTP read error is detected 1h = OTP read error is detected                               |
| 3     | RESERVED   | R-0    | 0h      | Reserved                                                                                                        |
| 2     | SPI_PARITY | R      | 0h      | SPI Parity Error 0h = No SPI Parity Error is detected 1h = SPI Parity Error is detected                         |
| 1     | BUS_CNT    | R      | 0h      | SPI Bus Contention Error 0h = No SPI Bus Contention Error is detected 1h = SPI Bus Contention Error is detected |
| 0     | FRM_ERR    | R      | 0h      | SPI Frame Error 0h = No SPI Frame Error is detected 1h = SPI Frame Error is detected                            |

## 9.6 PWM\_SYNC\_PRD Register (Offset = Ch) [Reset = 0000h]

PWM\_SYNC\_PRD is shown in PWM\_SYNC\_PRD Register and described in PWM\_SYNC\_PRD Register Field Descriptions.

Return to the DRV8311 Registers.

PWM Sync Period Register

## Figure 9-6. PWM\_SYNC\_PRD Register

| 15           | 14           | 13           | 12           | 11           | 10           | 9            | 8            |
|--------------|--------------|--------------|--------------|--------------|--------------|--------------|--------------|
| Parity_bit   | RESERVED     |              |              |              | PWM_SYNC_PRD |              |              |
| R-0h         | R-0-0h       |              |              |              | R-0h         |              |              |
| 7            | 6            | 5            | 4            | 3            | 2            | 1            | 0            |
| PWM_SYNC_PRD | PWM_SYNC_PRD | PWM_SYNC_PRD | PWM_SYNC_PRD | PWM_SYNC_PRD | PWM_SYNC_PRD | PWM_SYNC_PRD | PWM_SYNC_PRD |
| R-0h         | R-0h         | R-0h         | R-0h         | R-0h         | R-0h         | R-0h         | R-0h         |

## Table 9-8. PWM\_SYNC\_PRD Register Field Descriptions

| Bit   | Field        | Type   | Reset   | Description                                            |
|-------|--------------|--------|---------|--------------------------------------------------------|
| 15    | Parity_bit   | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved |
| 14-12 | RESERVED     | R-0    | 0h      | Reserved                                               |
| 11-0  | PWM_SYNC_PRD | R      | 0h      | 12-bit output indicating period of PWM_SYNC signal     |

<!-- image -->

<!-- image -->

## 9.7 FLT\_MODE Register (Offset = 10h) [Reset = 0115h]

FLT\_MODE is shown in FLT\_MODE Register and described in FLT\_MODE Register Field Descriptions.

Return to the DRV8311 Registers.

Fault Mode Register

## Figure 9-7. FLT\_MODE Register

| 15          | 14       | 13       | 12       | 11       | 10       | 9         | 8            |
|-------------|----------|----------|----------|----------|----------|-----------|--------------|
| Parity_bit  | RESERVED | RESERVED | RESERVED | RESERVED | RESERVED | RESERVED  | OTPFLT_MOD E |
| R-0h        | R-0-0h   | R-0-0h   | R-0-0h   | R-0-0h   | R-0-0h   | R-0-0h    | R/W-1h       |
| 7           | 6        | 5        | 4        | 3        | 2        | 1         | 0            |
| SPIFLT_MODE | OCP_MODE | OCP_MODE | OCP_MODE | UVP_MODE | UVP_MODE | OTSD_MODE | OTSD_MODE    |
| R/W-0h      | R/W-1h   | R/W-1h   | R/W-1h   | R/W-1h   | R/W-1h   | R/W-1h    | R/W-1h       |

## Table 9-9. FLT\_MODE Register Field Descriptions

| Bit   | Field       | Type   | Reset   | Description                                                                                                                                                                                                                                                                                                                                                   |
|-------|-------------|--------|---------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit  | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                                                                                                                                                                                                                                                                        |
| 14-9  | RESERVED    | R-0    | 0h      | Reserved                                                                                                                                                                                                                                                                                                                                                      |
| 8     | OTPFLT_MODE | R/W    | 1h      | System Fault Mode. 0h = OTP read fault is enabled 1h = OTP read fault is disabled                                                                                                                                                                                                                                                                             |
| 7     | SPIFLT_MODE | R/W    | 0h      | SPI Fault mode 0h = SPI Fault is enabled 1h = SPI Fault is disabled                                                                                                                                                                                                                                                                                           |
| 6-4   | OCP_MODE    | R/W    | 1h      | Overcurrent Protection Fault mode 0h = Report on nFault, predriver HiZ, auto recovery with Slow Retry time (in ms) 1h = Report on nFault, predriver HiZ, auto recovery with Fast Retry time (in ms) 2h = Report on nFault, predriver HiZ, Latched Fault 3h = Report on nFault, No action on predriver 4h = Reserved 5h = Reserved 6h = Reserved 7h = Disabled |
| 3-2   | UVP_MODE    | R/W    | 1h      | Undervoltage Protection Fault mode 0h = Report on nFault, predriver HiZ, auto recovery with Slow Retry time (in ms) 1h = Report on nFault, predriver HiZ, auto recovery with Fast Retry time (in ms) 2h = Reserved 3h = Reserved                                                                                                                              |
| 1-0   | OTSD_MODE   | R/W    | 1h      | Overtemperature Fault mode 0h = Report on nFault, predriver HiZ, auto recovery with Slow Retry time (in ms) 1h = Report on nFault, predriver HiZ, auto recovery with Fast Retry time (in ms) 2h = Reserved 3h = Reserved                                                                                                                                      |

## 9.8 SYSF\_CTRL Register (Offset = 12h) [Reset = 0515h]

SYSF\_CTRL is shown in SYSF\_CTRL Register and described in SYSF\_CTRL Register Field Descriptions.

Return to the DRV8311 Registers.

System Fault Control Register

Figure 9-8. SYSF\_CTRL Register

| 15         | 14       | 13           | 12       | 11       | 10        | 9        | 8        |
|------------|----------|--------------|----------|----------|-----------|----------|----------|
| Parity_bit | RESERVED | RESERVED     | RESERVED | RESERVED | OTAVDD_EN | OTW_EN   | RESERVED |
| R-0h       | R-0-0h   | R-0-0h       | R-0-0h   | R-0-0h   | R/W-1h    | R/W-0h   | R-0-4h   |
| 7          | 6        | 5            | 4        | 3        | 2         | 1        | 0        |
| RESERVED   | RESERVED | CSAREFUV_E N | RESERVED | RESERVED | RESERVED  | RESERVED | RESERVED |
| R-0-4h     | R-0-4h   | R/W-0h       | R/W-1h   | R-0-0h   | R/W-1h    | R-0-0h   | R/W-1h   |

Table 9-10. SYSF\_CTRL Register Field Descriptions

| Bit   | Field       | Type   | Reset   | Description                                                                                                                                                     |
|-------|-------------|--------|---------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit  | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                                                                          |
| 14-11 | RESERVED    | R-0    | 0h      | Reserved                                                                                                                                                        |
| 10    | OTAVDD_EN   | R/W    | 1h      | AVDD Overtemperature Fault Enable 0h = Overtemperature protection near AVDD is disabled 1h = Overtemperature protection near AVDD is enabled                    |
| 9     | OTW_EN      | R/W    | 0h      | Overtemperature Warning Fault Enable 0h = Over temperature warning reporting on nFAULT is disabled 1h = Over temperature warning reporting on nFAULT is enabled |
| 8-6   | RESERVED    | R-0    | 4h      | Reserved                                                                                                                                                        |
| 5     | CSAREFUV_EN | R/W    | 0h      | CSAREF Undervoltage Fault Enable 0h = CSAREF undervoltage lockout is disabled 1h = CSAREF undervoltage lockout is enabled                                       |
| 4     | RESERVED    | R/W    | 1h      | Reserved                                                                                                                                                        |
| 3     | RESERVED    | R-0    | 0h      | Reserved                                                                                                                                                        |
| 2     | RESERVED    | R/W    | 1h      | Reserved                                                                                                                                                        |
| 1     | RESERVED    | R-0    | 0h      | Reserved                                                                                                                                                        |
| 0     | RESERVED    | R/W    | 1h      | Reserved                                                                                                                                                        |

<!-- image -->

<!-- image -->

## 9.9 DRVF\_CTRL Register (Offset = 13h) [Reset = 0030h]

DRVF\_CTRL is shown in DRVF\_CTRL Register and described in DRVF\_CTRL Register Field Descriptions.

Return to the DRV8311 Registers.

Driver Fault Control Register

| 15         | 14       | 13      | 12      | 11         | 10         | 9        | 8       |
|------------|----------|---------|---------|------------|------------|----------|---------|
| Parity_bit |          |         |         | RESERVED   |            |          |         |
| R-0h       |          |         |         | R-0-0h     |            |          |         |
| 7          | 6        | 5       | 4       | 3          | 2          | 1        | 0       |
| RESERVED   | RESERVED | OCP_DEG | OCP_DEG | OCP_TBLANK | OCP_TBLANK | RESERVED | OCP_LVL |
| R-0-0h     | R-0-0h   | R/W-3h  | R/W-3h  | R/W-0h     | R/W-0h     | R-0-0h   | R/W-0h  |

## Figure 9-9. DRVF\_CTRL Register

## Table 9-11. DRVF\_CTRL Register Field Descriptions

| Bit   | Field      | Type   | Reset   | Description                                                                                                                                         |
|-------|------------|--------|---------|-----------------------------------------------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                                                              |
| 14-6  | RESERVED   | R-0    | 0h      | Reserved                                                                                                                                            |
| 5-4   | OCP_DEG    | R/W    | 3h      | OCP Deglitch time 0h = OCP deglitch time is 0.2 µs 1h = OCP deglitch time is 0.5 µs 2h = OCP deglitch time is 0.8 µs 3h = OCP deglitch time is 1 µs |
| 3-2   | OCP_TBLANK | R/W    | 0h      | OCP Blanking time 0h = OCP blanking time is 0.2 µs 1h = OCP blanking time is 0.5 µs 2h = OCP blanking time is 0.8 µs 3h = OCP blanking time is 1 µs |
| 1     | RESERVED   | R-0    | 0h      | Reserved                                                                                                                                            |
| 0     | OCP_LVL    | R/W    | 0h      | OCP Level Settings 0h = OCP level is 9 A (TYP) 1h = OCP level is 5 A (TYP)                                                                          |

## 9.10 FLT\_TCTRL Register (Offset = 16h) [Reset = 0003h]

FLT\_TCTRL is shown in FLT\_TCTRL Register and described in FLT\_TCTRL Register Field Descriptions.

Return to the DRV8311 Registers.

Fault Timing Control Register

## Figure 9-10. FLT\_TCTRL Register

| 15         | 14       |   13 |   12 | 11          | 10          | 9           | 8           |
|------------|----------|------|------|-------------|-------------|-------------|-------------|
| Parity_bit |          |      |      | RESERVED    |             |             |             |
| R-0h       |          |      |      | R-0-0h      |             |             |             |
| 7          | 6        |    5 |    4 | 3           | 2           | 1           | 0           |
| RESERVED   | RESERVED |      |      | SLOW_TRETRY | SLOW_TRETRY | FAST_TRETRY | FAST_TRETRY |
| R-0-0h     | R-0-0h   |      |      | R/W-0h      |             | R/W-3h      |             |

Table 9-12. FLT\_TCTRL Register Field Descriptions

| Bit   | Field       | Type   | Reset   | Description                                                                         |
|-------|-------------|--------|---------|-------------------------------------------------------------------------------------|
| 15    | Parity_bit  | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                              |
| 14-4  | RESERVED    | R-0    | 0h      | Reserved                                                                            |
| 3-2   | SLOW_TRETRY | R/W    | 0h      | Slow Recovery Retry Time from Fault Condition 0h = 0.5s 1h = 1s 2h = 2s 3h = 5s     |
| 1-0   | FAST_TRETRY | R/W    | 3h      | Fast Recovery Retry Time from Fault Condition 0h = 0.5ms 1h = 1ms 2h = 2ms 3h = 5ms |

<!-- image -->

<!-- image -->

## 9.11 FLT\_CLR Register (Offset = 17h) [Reset = 0000h]

FLT\_CLR is shown in FLT\_CLR Register and described in FLT\_CLR Register Field Descriptions.

Return to the DRV8311 Registers.

Fault Clear Register

## Figure 9-11. FLT\_CLR Register

| 15         |   14 |   13 | 12       | 11       |   10 |   9 | 8       |
|------------|------|------|----------|----------|------|-----|---------|
| Parity_bit |      |      |          | RESERVED |      |     |         |
| R-0h       |      |      |          | R-0-0h   |      |     |         |
| 7          |    6 |    5 | 4        | 3        |    2 |   1 | 0       |
|            |      |      | RESERVED |          |      |     | FLT_CLR |
|            |      |      | R-0-0h   |          |      |     | W-0h    |

## Table 9-13. FLT\_CLR Register Field Descriptions

| Bit   | Field      | Type   | Reset   | Description                                                                                                                                |
|-------|------------|--------|---------|--------------------------------------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                                                     |
| 14-1  | RESERVED   | R-0    | 0h      | Reserved                                                                                                                                   |
| 0     | FLT_CLR    | W      | 0h      | Clear Fault 0h = No clear fault command is issued 1h = To clear the latched fault bits. This bit automatically resets after being written. |

## 9.12 PWMG\_PERIOD Register (Offset = 18h) [Reset = 0000h]

PWMG\_PERIOD  is  shown  in  PWMG\_PERIOD  Register  and  described  in  PWMG\_PERIOD  Register  Field Descriptions.

Return to the DRV8311 Registers.

PWM\_GEN Period Register

## Figure 9-12. PWMG\_PERIOD Register

| 15          | 14          | 13          | 12          | 11          | 10          | 9           | 8           |
|-------------|-------------|-------------|-------------|-------------|-------------|-------------|-------------|
| Parity_bit  | RESERVED    |             |             |             | PWM_PRD_OUT |             |             |
| R-0h        | R-0-0h      |             |             |             | R/W-0h      |             |             |
| 7           | 6           | 5           | 4           | 3           | 2           | 1           | 0           |
| PWM_PRD_OUT | PWM_PRD_OUT | PWM_PRD_OUT | PWM_PRD_OUT | PWM_PRD_OUT | PWM_PRD_OUT | PWM_PRD_OUT | PWM_PRD_OUT |
| R/W-0h      | R/W-0h      | R/W-0h      | R/W-0h      | R/W-0h      | R/W-0h      | R/W-0h      | R/W-0h      |

## Table 9-14. PWMG\_PERIOD Register Field Descriptions

| Bit   | Field       | Type   | Reset   | Description                                                 |
|-------|-------------|--------|---------|-------------------------------------------------------------|
| 15    | Parity_bit  | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved      |
| 14-12 | RESERVED    | R-0    | 0h      | Reserved                                                    |
| 11-0  | PWM_PRD_OUT | R/W    | 0h      | 12-bit Period for output PWM signals in PWM Generation Mode |

<!-- image -->

<!-- image -->

## 9.13 PWMG\_A\_DUTY Register (Offset = 19h) [Reset = 0000h]

PWMG\_A\_DUTY  is  shown  in  PWMG\_A\_DUTY  Register  and  described  in  PWMG\_A\_DUTY  Register  Field Descriptions.

Return to the DRV8311 Registers.

PWM\_GEN A Duty Register

## Figure 9-13. PWMG\_A\_DUTY Register

| 15            | 14            | 13            | 12            | 11            | 10            | 9             | 8             |
|---------------|---------------|---------------|---------------|---------------|---------------|---------------|---------------|
| Parity_bit    | RESERVED      |               |               |               | PWM_DUTY_OUTA |               |               |
| R-0h          | R-0-0h        |               |               |               | R/W-0h        |               |               |
| 7             | 6             | 5             | 4             | 3             | 2             | 1             | 0             |
| PWM_DUTY_OUTA | PWM_DUTY_OUTA | PWM_DUTY_OUTA | PWM_DUTY_OUTA | PWM_DUTY_OUTA | PWM_DUTY_OUTA | PWM_DUTY_OUTA | PWM_DUTY_OUTA |
| R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        |

## Table 9-15. PWMG\_A\_DUTY Register Field Descriptions

| Bit   | Field         | Type   | Reset   | Description                                                 |
|-------|---------------|--------|---------|-------------------------------------------------------------|
| 15    | Parity_bit    | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved      |
| 14-12 | RESERVED      | R-0    | 0h      | Reserved                                                    |
| 11-0  | PWM_DUTY_OUTA | R/W    | 0h      | 12-bit Duty Cycle for Phase A output in PWM Generation Mode |

## 9.14 PWMG\_B\_DUTY Register (Offset = 1Ah) [Reset = 0000h]

PWMG\_B\_DUTY  is  shown  in  PWMG\_B\_DUTY  Register  and  described  in  PWMG\_B\_DUTY  Register  Field Descriptions.

Return to the DRV8311 Registers.

PWM\_GEN B Duty Register

## Figure 9-14. PWMG\_B\_DUTY Register

| 15            | 14            | 13            | 12            | 11            | 10            | 9             | 8             |
|---------------|---------------|---------------|---------------|---------------|---------------|---------------|---------------|
| Parity_bit    | RESERVED      |               |               |               | PWM_DUTY_OUTB |               |               |
| R-0h          | R-0-0h        |               |               |               | R/W-0h        |               |               |
| 7             | 6             | 5             | 4             | 3             | 2             | 1             | 0             |
| PWM_DUTY_OUTB | PWM_DUTY_OUTB | PWM_DUTY_OUTB | PWM_DUTY_OUTB | PWM_DUTY_OUTB | PWM_DUTY_OUTB | PWM_DUTY_OUTB | PWM_DUTY_OUTB |
| R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        |

## Table 9-16. PWMG\_B\_DUTY Register Field Descriptions

| Bit   | Field         | Type   | Reset   | Description                                                 |
|-------|---------------|--------|---------|-------------------------------------------------------------|
| 15    | Parity_bit    | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved      |
| 14-12 | RESERVED      | R-0    | 0h      | Reserved                                                    |
| 11-0  | PWM_DUTY_OUTB | R/W    | 0h      | 12-bit Duty Cycle for Phase B output in PWM Generation Mode |

<!-- image -->

<!-- image -->

## 9.15 PWMG\_C\_DUTY Register (Offset = 1Bh) [Reset = 0000h]

PWMG\_C\_DUTY  is  shown  in  PWMG\_C\_DUTY  Register  and  described  in  PWMG\_C\_DUTY  Register  Field Descriptions.

Return to the DRV8311 Registers.

PWM\_GEN C Duty Register

## Figure 9-15. PWMG\_C\_DUTY Register

| 15            | 14            | 13            | 12            | 11            | 10            | 9             | 8             |
|---------------|---------------|---------------|---------------|---------------|---------------|---------------|---------------|
| Parity_bit    | RESERVED      |               |               |               | PWM_DUTY_OUTC | PWM_DUTY_OUTC |               |
| R-0h          | R-0-0h        |               |               |               | R/W-0h        | R/W-0h        |               |
| 7             | 6             | 5             | 4             | 3             | 2             | 1             | 0             |
| PWM_DUTY_OUTC | PWM_DUTY_OUTC | PWM_DUTY_OUTC | PWM_DUTY_OUTC | PWM_DUTY_OUTC | PWM_DUTY_OUTC | PWM_DUTY_OUTC | PWM_DUTY_OUTC |
| R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        | R/W-0h        |

## Table 9-17. PWMG\_C\_DUTY Register Field Descriptions

| Bit   | Field         | Type   | Reset   | Description                                                 |
|-------|---------------|--------|---------|-------------------------------------------------------------|
| 15    | Parity_bit    | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved      |
| 14-12 | RESERVED      | R-0    | 0h      | Reserved                                                    |
| 11-0  | PWM_DUTY_OUTC | R/W    | 0h      | 12-bit Duty Cycle for Phase C output in PWM Generation Mode |

## 9.16 PWM\_STATE Register (Offset = 1Ch) [Reset = 0777h]

PWM\_STATE is shown in PWM\_STATE Register and described in PWM\_STATE Register Field Descriptions.

Return to the DRV8311 Registers.

PWM State Register

Figure 9-16. PWM\_STATE Register

| 15         | 14         | 13         | 12         | 11       | 10         | 9          | 8          |
|------------|------------|------------|------------|----------|------------|------------|------------|
| Parity_bit | RESERVED   | RESERVED   | RESERVED   | RESERVED | PWMC_STATE | PWMC_STATE | PWMC_STATE |
| R-0h       | R-0-0h     | R-0-0h     | R-0-0h     | R-0-0h   | R/W-7h     | R/W-7h     | R/W-7h     |
| 7          | 6          | 5          | 4          | 3        | 2          | 1          | 0          |
| RESERVED   | PWMB_STATE | PWMB_STATE | PWMB_STATE | RESERVED | PWMA_STATE | PWMA_STATE | PWMA_STATE |
| R-0-0h     | R/W-7h     | R/W-7h     | R/W-7h     | R-0-0h   | R/W-7h     | R/W-7h     | R/W-7h     |

Table 9-18. PWM\_STATE Register Field Descriptions

| Bit   | Field      | Type   | Reset   | Description                                                                                                                                                                                                                                                                                          |
|-------|------------|--------|---------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                                                                                                                                                                                                               |
| 14-11 | RESERVED   | R-0    | 0h      | Reserved                                                                                                                                                                                                                                                                                             |
| 10-8  | PWMC_STATE | R/W    | 7h      | Phase C Driver Output control 0h = High Side is OFF, Low Side is OFF 1h = High Side is OFF, Low Side is forced ON 2h = High Side is forced ON, Low Side is OFF 3h = Reserved 4h = Reserved 5h = High Side is OFF, Low Side PWM 6h = High Side PWM, Low Side is OFF 7h = High Side PWM, Low Side !PWM |
| 7     | RESERVED   | R-0    | 0h      | Reserved                                                                                                                                                                                                                                                                                             |
| 6-4   | PWMB_STATE | R/W    | 7h      | Phase B Driver Output control 0h = High Side is OFF, Low Side is OFF 1h = High Side is OFF, Low Side is forced ON 2h = High Side is forced ON, Low Side is OFF 3h = Reserved 4h = Reserved 5h = High Side is OFF, Low Side PWM 6h = High Side PWM, Low Side is OFF 7h = High Side PWM, Low Side !PWM |
| 3     | RESERVED   | R-0    | 0h      | Reserved                                                                                                                                                                                                                                                                                             |
| 2-0   | PWMA_STATE | R/W    | 7h      | Phase A Driver Output control 0h = High Side is OFF, Low Side is OFF 1h = High Side is OFF, Low Side is forced ON 2h = High Side is forced ON, Low Side is OFF 3h = Reserved 4h = Reserved 5h = High Side is OFF, Low Side PWM 6h = High Side PWM, Low Side is OFF 7h = High Side PWM, Low Side !PWM |

<!-- image -->

<!-- image -->

## 9.17 PWMG\_CTRL Register (Offset = 1Dh) [Reset = 0000h]

PWMG\_CTRL is shown in PWMG\_CTRL Register and described in PWMG\_CTRL Register Field Descriptions.

Return to the DRV8311 Registers.

PWM\_GEN Control Register

Figure 9-17. PWMG\_CTRL Register

| 15           | 14           | 13           | 12               | 11               | 10               | 9 8           |
|--------------|--------------|--------------|------------------|------------------|------------------|---------------|
| Parity_bit   | RESERVED     | RESERVED     | RESERVED         | RESERVED         | PWM_EN           | PWMCNTR_MODE  |
| R-0h         | R-0-0h       | R-0-0h       | R-0-0h           | R-0-0h           | R/W-0h           | R/W-0h        |
| 7            | 6            | 5            | 4                | 3                | 2                | 1 0           |
| PWM_OSC_SYNC | PWM_OSC_SYNC | PWM_OSC_SYNC | SPICLK_FREQ_SYNC | SPICLK_FREQ_SYNC | SPICLK_FREQ_SYNC | SPISYNC_ACRCY |
| R/W-0h       | R/W-0h       | R/W-0h       | R/W-0h           | R/W-0h           | R/W-0h           | R/W-0h        |

## Table 9-19. PWMG\_CTRL Register Field Descriptions

| Bit   | Field            | Type   | Reset   | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
|-------|------------------|--------|---------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit       | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| 14-11 | RESERVED         | R-0    | 0h      | Reserved                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| 10    | PWM_EN           | R/W    | 0h      | Enable 3X Internal mode PWM Generation 0h = PWM_GEN disabled 1h = PWM_GEN enabled                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| 9-8   | PWMCNTR_MODE     | R/W    | 0h      | PWM Gen counter mode 0h = Up and Down 1h = Up 2h = Down 3h = No action                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| 7-5   | PWM_OSC_SYNC     | R/W    | 0h      | Oscillator synchronization and PWM_SYNC control 0h = Oscillator synchronization is disable 1h = PWM_SYNC_PRD indicates period of PWM_SYNC signal and can be used to calibrate PWM period 2h = PWM_SYNC used to set PWM period 3h = Oscillator synchronization is disable 4h = Oscillator synchronization is disable 5h = PWM_SYNC used for oscillator synchronization (only 20 kHz frequency supported) 6h = PWM_SYNC used for oscillator synchronization and setting PWM period (only 20 kHz frequency supported) 7h = SPI Clock pin SCLK used for oscillator synchronization (Configure SPICLK_FREQ_SYNC) |
| 4-2   | SPICLK_FREQ_SYNC | R/W    | 0h      | SPI Clock Frequency for synchronizing the Oscillator 0h = 1 MHz 1h = 1.25 MHz 2h = 2 MHz 3h = 2.5 MHz 4h = 4 MHz 5h = 5 MHz 6h = 8 MHz 7h = 10 MHz                                                                                                                                                                                                                                                                                                                                                                                                                                                          |

<!-- image -->

## Table 9-19. PWMG\_CTRL Register Field Descriptions (continued)

| Bit   | Field         | Type   | Reset   | Description                                                                                                                                                                   |
|-------|---------------|--------|---------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1-0   | SPISYNC_ACRCY | R/W    | 0h      | Number of SPI Clock Cycle require for synchronizing the Oscillator 0h = 512 Clock Cycles (1%) 1h = 256 Clock Cycles (1%) 2h = 128 Clock Cycles (1%) 3h = 64 Clock Cycles (2%) |

<!-- image -->

<!-- image -->

## 9.18 PWM\_CTRL1 Register (Offset = 20h) [Reset = 0007h]

PWM\_CTRL1 is shown in PWM\_CTRL1 Register and described in PWM\_CTRL1 Register Field Descriptions.

Return to the DRV8311 Registers.

PWM Control Register 1

## Figure 9-18. PWM\_CTRL1 Register

| 15         | 14       | 13       | 12       | 11       | 10      | 9        | 8        |
|------------|----------|----------|----------|----------|---------|----------|----------|
| Parity_bit |          |          |          | RESERVED |         |          |          |
| R-0h       |          |          |          | R-0-0h   |         |          |          |
| 7          | 6        | 5        | 4        | 3        | 2       | 1        | 0        |
| RESERVED   | RESERVED | RESERVED | RESERVED | RESERVED | SSC_DIS | PWM_MODE | PWM_MODE |
| R-0-0h     | R-0-0h   | R-0-0h   | R-0-0h   | R-0-0h   | R/W-1h  | R/W-3h   | R/W-3h   |

## Table 9-20. PWM\_CTRL1 Register Field Descriptions

| Bit   | Field      | Type   | Reset   | Description                                                                                                                                       |
|-------|------------|--------|---------|---------------------------------------------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                                                            |
| 14-3  | RESERVED   | R-0    | 0h      | Reserved                                                                                                                                          |
| 2     | SSC_DIS    | R/W    | 1h      | Disable Spread Spectrum Modulation for internal Oscillator 0h = Spread spectrum modulation is enabled 1h = Spread spectrum modulation is disabled |
| 1-0   | PWM_MODE   | R/W    | 3h      | PWM mode selection (The reset setting in DRV8311S is 00b and in DRV8311P is 11b) 0h = 6x mode 1h = 6x mode 2h = 3x mode 3h = PWM Generation mode  |

## 9.19 DRV\_CTRL Register (Offset = 22h) [Reset = 0000h]

DRV\_CTRL is shown in DRV\_CTRL Register and described in DRV\_CTRL Register Field Descriptions.

Return to the DRV8311 Registers.

Predriver control Register

Figure 9-19. DRV\_CTRL Register

| 15         | 14         | 13         | 12         | 11       | 10       | 9         | 8         |
|------------|------------|------------|------------|----------|----------|-----------|-----------|
| Parity_bit | RESERVED   | RESERVED   | RESERVED   | RESERVED | RESERVED | RESERVED  | RESERVED  |
| R-0h       | R-0-0h     | R-0-0h     | R-0-0h     | R/W-0h   | R/W-0h   | R/W-0h    | R/W-0h    |
| 7          | 6          | 5          | 4          | 3        | 2        | 1         | 0         |
| DLYCMP_EN  | TDEAD_CTRL | TDEAD_CTRL | TDEAD_CTRL | RESERVED | RESERVED | SLEW_RATE | SLEW_RATE |
| R/W-0h     | R/W-0h     | R/W-0h     | R/W-0h     | R-0-0h   | R-0-0h   | R/W-0h    | R/W-0h    |

Table 9-21. DRV\_CTRL Register Field Descriptions

| Bit   | Field      | Type   | Reset   | Description                                                                                                                             |
|-------|------------|--------|---------|-----------------------------------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                                                  |
| 14-12 | RESERVED   | R-0    | 0h      | Reserved                                                                                                                                |
| 11-8  | RESERVED   | R/W    | 0h      | Reserved                                                                                                                                |
| 7     | DLYCMP_EN  | R/W    | 0h      | Driver Delay Compensation enable 0h = Driver Delay Compensation is disabled 1h = Driver Delay Compensation is enabled                   |
| 6-4   | TDEAD_CTRL | R/W    | 0h      | Deadtime insertion control 0h = No deadtime (Handshake Only) 1h = 200ns 2h = 400ns 3h = 600ns 4h = 800ns 5h = 1us 6h = 1.2us 7h = 1.4us |
| 3-2   | RESERVED   | R-0    | 0h      | Reserved                                                                                                                                |
| 1-0   | SLEW_RATE  | R/W    | 0h      | Slew rate settings 0h = Slew rate is 35 V/µs 1h = Slew rate is 75 V/µs 2h = Slew rate is 180 V/µs 3h = Slew rate is 230 V/µs            |

<!-- image -->

<!-- image -->

## 9.20 CSA\_CTRL Register (Offset = 23h) [Reset = 0008h]

CSA\_CTRL is shown in CSA\_CTRL Register and described in CSA\_CTRL Register Field Descriptions.

Return to the DRV8311 Registers.

CSA Control Register

| 15         | 14       |   13 |   12 | 11       | 10       | 9        |   8 |
|------------|----------|------|------|----------|----------|----------|-----|
| Parity_bit |          |      |      | RESERVED |          |          |     |
| R-0h       |          |      |      | R-0-0h   |          |          |     |
| 7          | 6        |    5 |    4 | 3        | 2        | 1        |   0 |
| RESERVED   | RESERVED |      |      | CSA_EN   | RESERVED | CSA_GAIN |     |
| R-0-0h     | R-0-0h   |      |      | R/W-1h   | R-0-0h   | R/W-0h   |     |

## Figure 9-20. CSA\_CTRL Register

## Table 9-22. CSA\_CTRL Register Field Descriptions

| Bit   | Field      | Type   | Reset   | Description                                                                                                                            |
|-------|------------|--------|---------|----------------------------------------------------------------------------------------------------------------------------------------|
| 15    | Parity_bit | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                                                                                 |
| 14-4  | RESERVED   | R-0    | 0h      | Reserved                                                                                                                               |
| 3     | CSA_EN     | R/W    | 1h      | Current Sense Amplifier Enable 0h = Current Sense Amplifier is disabled 1h = Current Sense Amplifier is enabled                        |
| 2     | RESERVED   | R-0    | 0h      | Reserved                                                                                                                               |
| 1-0   | CSA_GAIN   | R/W    | 0h      | Current Sense Amplifier Gain settings 0h = CSA gain is 0.25 V/A 1h = CSA gain is 0.5 V/A 2h = CSA gain is 1 V/A 3h = CSA gain is 2 V/A |

## 9.21 SYS\_CTRL Register (Offset = 3Fh) [Reset = 0000h]

SYS\_CTRL is shown in SYS\_CTRL Register and described in SYS\_CTRL Register Field Descriptions.

Return to the DRV8311 Registers.

System Control Register

## Figure 9-21. SYS\_CTRL Register

| 15         | 14        | 13        | 12        | 11       | 10       | 9        | 8        |
|------------|-----------|-----------|-----------|----------|----------|----------|----------|
| Parity_bit | WRITE_KEY | WRITE_KEY | WRITE_KEY | RESERVED | RESERVED | RESERVED | RESERVED |
| R-0h       | W-0h      | W-0h      | W-0h      | R-0-0h   | R-0-0h   | R-0-0h   | R/W-0h   |
| 7          | 6         | 5         | 4         | 3        | 2        | 1        | 0        |
| REG_LOCK   | SPI_PEN   | RESERVED  | RESERVED  | RESERVED | RESERVED | RESERVED | RESERVED |
| R/W-0h     | R/W-0h    | R/W-0h    | R/W-0h    | R/W-0h   |          | R/W-0h   |          |

## Table 9-23. SYS\_CTRL Register Field Descriptions

| Bit   | Field      | Type   | Reset   | Description                                                                  |
|-------|------------|--------|---------|------------------------------------------------------------------------------|
| 15    | Parity_bit | R      | 0h      | Parity Bit if SPI_PEN is set to '1' otherwise reserved                       |
| 14-12 | WRITE_KEY  | W      | 0h      | 0x5 Write Key Specific to this register.                                     |
| 11-9  | RESERVED   | R-0    | 0h      | Reserved                                                                     |
| 8     | RESERVED   | R/W    | 0h      | Reserved                                                                     |
| 7     | REG_LOCK   | R/W    | 0h      | Register Lock Bit 0h = Registers Unlocked 1h = Registers Locked              |
| 6     | SPI_PEN    | R/W    | 0h      | Parity Enable for both SPI and tSPI 0h = Parity Disabled 1h = Parity Enabled |
| 5-4   | RESERVED   | R/W    | 0h      | Reserved                                                                     |
| 3     | RESERVED   | R/W    | 0h      | Reserved                                                                     |
| 2-0   | RESERVED   | R/W    | 0h      | Reserved                                                                     |

<!-- image -->

<!-- image -->

## 10 Application and Implementation

## Note

Information  in  the  following  applications  sections  is  not  part  of  the  TI  component  specification, and  TI does  not  warrant its accuracy  or completeness.  TI's customers  are  responsible  for determining suitability of components for their purposes, as well as validating and testing their design implementation to confirm system functionality.

## 10.1 Application Information

The  DRV8311  can  be  used  to  drive  Brushless-DC  motors.  The  following  design  procedure  can  be  used  to configure the DRV8311.

Figure 10-1. Application Schematics (DRV8311S)

<!-- image -->

## 10.2 Typical Applications

## 10.2.1 Three-Phase Brushless-DC Motor Control

In  this  application,  the  DRV8311  is  used  to  drive  a  Brushless-DC  motor  using  PWMs  from  an  external microcontroller.

## 10.2.1.1 Detailed Design Procedure

Table 10-1 lists the example input parameters for the system design.

Table 10-1. Design Parameters

| DESIGN PARAMETERS          | REFERENCE   | EXAMPLE VALUE   |
|----------------------------|-------------|-----------------|
| Supply voltage             | V VM        | 12 V            |
| Motor RMS current          | I RMS       | 2 A             |
| Motor peak current         | I PEAK      | 3 A             |
| PWM Frequency              | f PWM       | 50 kHz          |
| Slew Rate Setting          | SR          | 230 V/µs        |
| VIN_AVDD supply voltage    | V VIN_AVDD  | 12 V            |
| CSA reference voltage      | V CSA_REF   | 3.0 V           |
| System ambient temperature | T A         | -20°C to +105°C |

## 10.2.1.1.1 Motor Voltage

Brushless-DC motors are typically rated for a certain voltage (for example 5V or 12V). The DRV8311 allows for a range of possible operating voltages from 3V to 20 V.

## 10.2.1.2 Driver Propagation Delay and Dead Time

The propagation delay is defined as the time taken for changing input logic edges INHx and INLx (whichever changes first if MCU dead time is added) to change the half-bridge output voltage (OUTx). Driver propagation delay (tPD) and dead time (tdead) is specified with a typical and maximum value, but not with a minimum value. This  is  because  the  propagation  delay  can  be  smaller  than  typical  depending  on  the  direction  of  current  at the OUTx pin during synchronous switching. Driver propagation delay and dead time can be more than typical values  due  to  slower  internal  turn-ons  of  the  high-side  or  low-side  internal  MOSFETs  to  avoid  internal  dV/dt coupling.

For more information and examples of how propagation delay and dead time differs for input PWM and output configurations, refer Delay and Dead Time in Integrated MOSFET Drivers .

The dead time from the microcontroller's PWM outputs can be used as an extra precaution in addition to the DRV8311 internal shoot-through protection. The DRV8311 uses an internal logic prioritizes the MCU dead time or driver dead time based on their durations.

If the MCU dead time is less than the DRV8311 driver dead time, the driver will compensate and make the true output dead time with the value specified by the DRV8311. If the MCU inserted dead time is larger than the driver dead time, then the DRV8311 will adjust timing as per the MCU dead time.

A  summary  of  the  DRV8311  delay  times  with  respect  to  synchronous  inputs  INHx  and  INLx,  OUTx  current direction, and MCU dead time are listed in Table 10-2.

Table 10-2. Summary of Delay Times in DRV8311 Depending on Logic Inputs and Output Current Direction

| OUTx Current Direction   | INHx    | INLx    | Propagation Delay (t PD )   | Dead Time (t dead )   | Inserted MCU Dead Time (t dead(MCU) )   | Inserted MCU Dead Time (t dead(MCU) )   |
|--------------------------|---------|---------|-----------------------------|-----------------------|-----------------------------------------|-----------------------------------------|
|                          |         |         |                             |                       | t dead(MCU) < t dead                    | t dead(MCU) > t dead                    |
| Out of OUTx              | Rising  | Falling | Typical                     | Typical               | Output dead time = t dead               | Output dead time = t dead(MCU)          |
|                          | Falling | Rising  | Smaller than typical        | Smaller than typical  | Output dead time < t dead               | Output dead time < t dead(MCU)          |

<!-- image -->

<!-- image -->

## Table 10-2. Summary of Delay Times in DRV8311 Depending on Logic Inputs and Output Current Direction (continued)

| OUTx Current Direction   | INHx    | INLx    | Propagation Delay (t )   | Dead Time (t dead )   | Inserted MCU Dead Time (t dead(MCU) )   | Inserted MCU Dead Time (t dead(MCU) )   |
|--------------------------|---------|---------|--------------------------|-----------------------|-----------------------------------------|-----------------------------------------|
|                          |         |         | PD                       |                       | t dead(MCU) < t dead                    | t dead(MCU) > t dead                    |
| Into OUTx                | Rising  | Falling | Smaller than typical     | Smaller than typical  | Output dead time < t dead               | Output dead time < t dead(MCU)          |
|                          | Falling | Rising  | Typical                  | Typical               | Output dead time = t dead               | Output dead time = t dead(MCU)          |

## 10.2.1.3 Delay Compensation

Differences in delays of dead time and propagation delay can cause mismatch in the output timings of PWMs, which can lead to duty cycle distortion. In order to accommodate differences in propagation delay between the conditions mentioned in Table 10-2, DRV8311 integrate a delay compensation feature.

Delay compensation is used to match delay times for currents going into and out of phase (OUTx) by adding a variable delay time (t var ) to match a preset target delay time equal to the propagation delay plus driver dead time (t pd  + tdead). This setting is automatically configured by the DRV8311 when the DLYCMP\_EN bit is set to 1.

## 10.2.1.4 Current Sensing and Output Filtering

The SOx pins are typically sampled by an analog-to-digital converter in the MCU to calculate the phase current. Phase current information is used for closed-loop control such as Field-oriented control.

An example calculation for phase current is shown in Equation 15.

For a system using VREF = 3.0V, GAIN = 0.5 V/A, and a SOx voltage of 1.2V gives IOUTx = 0.6A.

<!-- formula-not-decoded -->

Sometimes  high  frequency  noise  can  appear  at  the  SOx  signals  based  on  voltage  ripple  at  VREF,  added inductance at the SOx traces, or routing of SOx traces near high frequency components. It is recommended to add a low-pass RC filter close to the MCU with cutoff frequency at least 10 times the PWM switching frequency for  trapezoidal  commutation  and  100  times  the  PWM  switching  frequency  for  sinusoidal  commutation  to  filter high frequency noise. A recommended RC filter is 330-ohms, 22-pF to add minimal parallel capacitance to the ADC and current mirroring circuitry without increasing the settling time of the CSA output.

The cutoff frequency for the low-pass RC filter is in Equation 16.

Note

<!-- formula-not-decoded -->

There  is  a  small  dynamic  offset  and  gain  error  that  appears  at  the  CSA  outputs  When  running sensorless sinusoidal or FOC control where two or three current sense are required. Refer Section 8.3.10.2 for details on corrective actions.

D INHA

D OUTAI

D VM

D VM

2 OUTA

2 nFAULT

2 nFAULT

3 SOA

B OUTC

3 NSLEEP

3 NSLEEP

B AVDD

B AVDD

· 5.00 V

· 5.00 V

® 10.0 V ® 10.0 V

· 2.00 V

4 RMSI

## 10.2.1.5 Application Curves

2 2.00 V

2 2.00 V

2 10,0 V

Value

Value

Mean

Mean

Value

Mean

1.43 V

2.55 V

2.55|

1.43

CD RMS

Mean

Value

1.09|

&gt;114mA A Clipping negative

1.09 A

Pinner And Manic Done n with nol Ceo lia

<!-- image -->

Figure 10-2. Device Power up with VM (VM, nFAULT, nSLEEP, AVDD)

<!-- image -->

Figure 10-4. Driver PWM Operation (OUTA, OUTB, OUTC, I\_A)

Figure 10-3. Device Power up with nSLEEP (VM, nFAULT, nSLEEP, AVDD)

<!-- image -->

<!-- image -->

Figure 10-5. Driver PWM Operation with Current Sense Feedback (INHA, OUTA, SOA, I\_A)

<!-- image -->

<!-- image -->

## 10.3 Three Phase Brushless-DC tSPI Motor Control

The DRV8311 can be used to drive Brushless-DC motors using tSPI from a microcontroller. The following design procedure can be used to configure the DRV8311.

Figure 10-6. Application Schematics (DRV8311P) - Three Phase Brushless-DC tSPI Motor Control

<!-- image -->

## 10.3.1 Detailed Design Procedure

## Benefits of tSPI

The  DRV8311P  device  integrates  tSPI  which  allows  for  random  write  and  read  access  to  secondary  motor driver devices for simultaneous motor control over a standard 4-wire SPI interface. This significantly reduce the number of wires in the system to reduce the overall system size and BOM costs. tSPI is especially useful in multi-motor systems by:

- Allowing random access to the DRV8311P devices with a general call address
- Performing read and writes in any order
- No requirement for all tSPI devices to be active at all times

--- -----------------0----------------

===\_=\_\_\_\_\_\_\_"

DPWM-SYNC---------==============£==

2 OUTA· /-

OUTE:

BOUTC.

200m Factor: 2 X

O PWM\_SYNCH

2 OUTAR

BOUTE

E OUTC

· 2.00 V

<!-- image -->

<!-- image -->

- Perform transactions with any active secondary device regardless of the status of the other devices

For more information on using tSPI in multi-motor systems, refer Reduce Wires for Your Next Multi-Motor BLDC Design With tSPI Protocol.

## Application Curves

10.0 V

10.0 M

Value

Mean

Min

Max td Der

· 8100122000uS 500 5/

Figure 10-7. PWM Synchronous Duty Cycle Operation with PWM\_SYNC = 2b (10% - 90%) (PWM\_SYNC, OUTA, OUTB, OUTC)

<!-- image -->

## 10.4 Alternate Applications

The DRV8311 can be used to drive Brushed-DC motors and solenoid loads. The following design procedure can be used to configure the DRV8311.

Figure 10-8. Application Schematics (DRV8311H) - Brushed-DC and Solenoid Load Drive Block Diagram

<!-- image -->

6x PWM mode or 3x PWM mode (with or without current limit) can be used to drive Brushed-DC and/or solenoid loads depending on the application. A Brushed-DC motor can be connected to two OUTx phases to create an integrated full H-bridge configuration to drive the motor in both direction.

Solenoid loads can be connected from OUTx to VM or GND to use the DRV8311 as a push-pull driver in 6x PWM or 3x PWM mode. When the load is connected from OUTx to GND, the HS MOSFET sources current into the solenoid, and the LS MOSFET acts as a recirculation diode to recirculate current from the solenoid. When the load is connected from OUTx to VM, the LS MOSFET sink current from the solenoid to GND, and the HS MOSFET acts as a recirculation diode to recirculate current from the solenoid.

## 11 Power Supply Recommendations

## 11.1 Bulk Capacitance

Having an appropriate local bulk capacitance is an important factor in motor drive system design. It is generally beneficial to have more bulk capacitance, while the disadvantages are increased cost and physical size.

The amount of local capacitance needed depends on a variety of factors, including:

- The highest current required by the motor system
- The capacitance and current capability of the power supply
- The amount of parasitic inductance between the power supply and motor system
- The acceptable voltage ripple
- The type of motor used (brushed dc, brushless DC, stepper)
- The motor braking method

The inductance between the power supply and the motor drive system limits the rate current can change from the power supply. If the local bulk capacitance is too small, the system responds to excessive current demands or dumps from the motor with a change in voltage. When adequate bulk capacitance is used, the motor voltage remains stable and high current can be quickly supplied.

The data sheet generally provides a recommended value, but system-level testing is required to determine the appropriate sized bulk capacitor.

Figure 11-1. Example Setup of Motor Drive System With External Power Supply

<!-- image -->

The voltage rating for bulk capacitors should be higher than the operating voltage, to provide margin for cases when the motor transfers energy to the supply.

<!-- image -->

<!-- image -->

[www.ti.com](https://www.ti.com/)

## 12 Layout 12.1 Layout Guidelines

The bulk capacitor should be placed to minimize the distance of the high-current path through the motor driver device. The connecting metal trace widths should be as wide as possible, and numerous vias should be used when connecting PCB layers. These practices minimize inductance and allow the bulk capacitor to deliver high current.

Small-value capacitors should be ceramic, and placed closely to device pins including, AVDD, charge pump, CSAREF, VINAVDD and VM.

The high-current device outputs should use wide metal traces.

To  reduce  noise  coupling  and  EMI  interference  from  large  transient  currents  into  small-current  signal  paths, grounding should be partitioned between PGND and AGND. TI recommends connecting all non-power stage circuitry  (including  the  thermal  pad)  to  AGND  to  reduce  parasitic  effects  and  improve  power  dissipation  from the device. Ensure grounds are connected through net-ties to reduce voltage offsets and maintain gate driver performance. A common ground plane can also be used for PGND and AGND to minimize inductance in the grounding, but it is recommended to place motor switching outputs as far away from analog and digital signals so motor noise does not couple into the analog and digital circuits.

The device thermal pad should be soldered to the PCB top-layer ground plane. Multiple vias should be used to connect to a large bottom-layer ground plane. The use of large metal planes and multiple vias helps dissipate the heat that is generated in the device.

To improve thermal performance, maximize the ground area that is connected to the thermal pad ground across all  possible  layers  of  the  PCB.  Using  thick  copper  pours  can  lower  the  junction-to-air  thermal  resistance  and improve thermal dissipation from the die surface.

<!-- image -->

## 12.2 Layout Example

<!-- image -->

Figure 12-1. Recommended Layout Example for DRV8311

<!-- image -->

<!-- image -->

## 12.3 Thermal Considerations

The  DRV8311  has  thermal  shutdown  (TSD)  as  previously  described.  A  die  temperature  in  excess  of  150°C (minimally) disables the device until the temperature drops to a safe level.

Any tendency of the device to enter thermal shutdown is an indication of excessive power dissipation, insufficient heatsinking, or too high an ambient temperature.

## 12.3.1 Power Dissipation and Junction Temperature Estimation

## Power Dissipation

The power loss in DRV8311 include standby power losses, LDO power losses, FET conduction and switching losses,  and  diode  losses.  The  FET  conduction  loss  dominates  the  total  power  dissipation  in  DRV8311.  At start-up  and  fault  conditions,  the  output  current  is  much  higher  than  normal  current;  remember  to  take  these peak currents and their duration into consideration. The total device dissipation is the power dissipated in each of the three half bridges added together. The maximum amount of power that the device can dissipate depends on ambient temperature and heatsinking. Note that RDS,ON increases with temperature, so as the device heats, the power dissipation increases. Take this into consideration when designing the PCB and heatsinking.

A summary of equations for calculating each loss is listed in Table 12-1 for trapezoidal control and field-oriented control.

Table 12-1. DRV8311 Power Losses for Trapezoidal and Field-oriented Control

| Loss type         | Trapezoidal control                                    | Field-oriented control                                 |
|-------------------|--------------------------------------------------------|--------------------------------------------------------|
| Standby power     | P standby = V VM x I VM_TA                             | P standby = V VM x I VM_TA                             |
| LDO (from VM)     | P LDO = (V VIN_AVDD - V AVDD ) x I AVDD                | P LDO = (V VIN_AVDD - V AVDD ) x I AVDD                |
| FET conduction    | P CON = 2 x (I PK(trap) ) 2 x R DS,ON(TA)              | P CON = 3 x (I RMS(FOC) ) 2 x R DS,ON(TA)              |
| FET switching     | P SW = I PK(trap) x V PK(trap) x t rise/fall x f PWM   | PSW = 3 x I RMS(FOC) x V PK(FOC) x t rise/fall x f PWM |
| Diode (dead time) | P diode = 2 x I PK(trap) x V F(diode) x t DEAD x f PWM | P diode = 6 x I RMS(FOC) x V F(diode) x t DEAD x f PWM |

## Junction Temperature Estimation

To  calculate  the  junction  temperature  of  the  die  from  power  losses,  use  Equation  17.  Note  that  the  thermal resistance  R θJA   depends  on  PCB  configurations  such  as  the  ambient  temperature,  numbers  of  PCB  layers, copper thickness, and the PCB size.

Refer BLDC integrated MOSFET thermal calculator for estimating the approximate device power dissipation and junction temperature at different use cases.

<!-- formula-not-decoded -->

## 13 Device and Documentation Support 13.1 Support Resources

TI E2E ™  support forums are an engineer's go-to source for fast, verified answers and design help - straight from the experts. Search existing answers or ask your own question to get the quick design help you need.

Linked content is provided "AS IS" by the respective contributors. They do not constitute TI specifications and do not necessarily reflect TI's views; see TI's Terms of Use.

## 13.2 Trademarks

TI E2E ™  is a trademark of Texas Instruments.

All trademarks are the property of their respective owners.

## 13.3 Electrostatic Discharge Caution

<!-- image -->

This integrated circuit can be damaged by ESD. Texas Instruments recommends that all integrated circuits be handled with appropriate precautions. Failure to observe proper handling and installation procedures can cause damage.

ESD damage can range from subtle performance degradation to complete device failure. Precision integrated circuits may be more susceptible to damage because very small parametric changes could cause the device not to meet its published specifications.

## 13.4 Glossary

[TI Glossary](https://www.ti.com/lit/pdf/SLYZ022)

This glossary lists and explains terms, acronyms, and definitions.

## 14 Mechanical, Packaging, and Orderable Information

The following  pages  include  mechanical,  packaging,  and  orderable  information.  This  information  is  the  mostcurrent  data  available  for  the  designated  device.  This  data  is  subject  to  change  without  notice  and  without revision of this document. For browser-based versions of this data sheet, see the left-hand navigation pane.

<!-- image -->

<!-- image -->

www.ti.com

## PACKAGING INFORMATION

| Orderable Device   | Status (1)   | Package Type Package Drawing   |     |   Pins |   Package Qty | Eco Plan (2)   | Lead finish/ Ball material (6)   | MSL Peak Temp (3)   | Op Temp (°C)   | Device Marking (4/5)   | Samples   |
|--------------------|--------------|--------------------------------|-----|--------|---------------|----------------|----------------------------------|---------------------|----------------|------------------------|-----------|
| DRV8311HRRWR       | ACTIVE       | WQFN                           | RRW |     24 |          5000 | RoHS & Green   | NIPDAU                           | Level-1-260C-UNLIM  | -40 to 125     | 8311H                  | Samples   |
| DRV8311PRRWR       | ACTIVE       | WQFN                           | RRW |     24 |          5000 | RoHS & Green   | NIPDAU                           | Level-1-260C-UNLIM  | -40 to 125     | 8311P                  | Samples   |

ACTIVE: Product device recommended for new designs.

LIFEBUY: TI has announced that the device will be discontinued, and a lifetime-buy period is in effect.

NRND: Not recommended for new designs. Device is in production to support existing customers, but TI does not recommend using this part in a new design.

PREVIEW: Device has been announced but is not in production. Samples may or may not be available.

OBSOLETE: TI has discontinued the production of the device.

(2) RoHS: TI defines "RoHS" to mean semiconductor products that are compliant with the current EU RoHS requirements for all 10 RoHS substances, including the requirement that RoHS substance do not exceed 0.1% by weight in homogeneous materials. Where designed to be soldered at high temperatures, "RoHS" products are suitable for use in specified lead-free processes. TI may reference these types of products as "Pb-Free".

RoHS Exempt: TI defines "RoHS Exempt" to mean products that contain lead but are compliant with EU RoHS pursuant to a specific EU RoHS exemption.

Green: TI defines "Green" to mean the content of Chlorine (Cl) and Bromine (Br) based flame retardants meet JS709B low halogen requirements of &lt;=1000ppm threshold. Antimony trioxide based flame retardants must also meet the &lt;=1000ppm threshold requirement.

(3) MSL, Peak Temp. - The Moisture Sensitivity Level rating according to the JEDEC industry standard classifications, and peak solder temperature.

(4) There may be additional marking, which relates to the logo, the lot trace code information, or the environmental category on the device.

(5) Multiple Device Markings will be inside parentheses. Only one Device Marking contained in parentheses and separated by a "~" will appear on a device. If a line is indented then it is a continuation of the previous line and the two combined represent the entire Device Marking for that device.

(6) Lead finish/Ball material - Orderable Devices may have multiple material finish options. Finish options are separated by a vertical ruled line. Lead finish/Ball material values may wrap to two lines if the finish value exceeds the maximum column width.

Important Information and Disclaimer: The information provided on this page represents TI's knowledge and belief as of the date that it is provided. TI bases its knowledge and belief on information provided by third parties, and makes no representation or warranty as to the accuracy of such information. Efforts are underway to better integrate information from third parties. TI has taken and continues to take reasonable steps to provide representative and accurate information but may not have conducted destructive testing or chemical analysis on incoming materials and chemicals. TI and TI suppliers consider certain information to be proprietary, and thus CAS numbers and other limited information may not be available for release.

In no event shall TI's liability arising out of such information exceed the total purchase price of the TI part(s) at issue in this document sold by TI to Customer on an annual basis.

8-Apr-2023

<!-- image -->

www.ti.com

## PACKAGE OPTION ADDENDUM

8-Apr-2023

<!-- image -->

www.ti.com

## TAPE AND REEL INFORMATION

<!-- image -->

## QUADRANT ASSIGNMENTS FOR PIN 1 ORIENTATION IN TAPE

<!-- image -->

| Device       | Package Type   | Package Drawing   |   Pins |   SPQ |   Reel Diameter (mm) |   Reel Width W1 (mm) |   A0 (mm) |   B0 (mm) |   K0 (mm) |   P1 (mm) |   W (mm) | Pin1 Quadrant   |
|--------------|----------------|-------------------|--------|-------|----------------------|----------------------|-----------|-----------|-----------|-----------|----------|-----------------|
| DRV8311HRRWR | WQFN           | RRW               |     24 |  5000 |                330.0 |                 12.4 |       3.3 |       3.3 |       1.1 |       8.0 |     12.0 | Q2              |
| DRV8311PRRWR | WQFN           | RRW               |     24 |  5000 |                330.0 |                 12.4 |       3.3 |       3.3 |       1.1 |       8.0 |     12.0 | Q2              |

## *All dimensions are nominal

## PACKAGE MATERIALS INFORMATION

6-Jun-2022

<!-- image -->

www.ti.com

<!-- image -->

*All dimensions are nominal

| Device       | Package Type   | Package Drawing   |   Pins |   SPQ |   Length (mm) |   Width (mm) |   Height (mm) |
|--------------|----------------|-------------------|--------|-------|---------------|--------------|---------------|
| DRV8311HRRWR | WQFN           | RRW               |     24 |  5000 |         367.0 |        367.0 |          35.0 |
| DRV8311PRRWR | WQFN           | RRW               |     24 |  5000 |         367.0 |        367.0 |          35.0 |

## PACKAGE MATERIALS INFORMATION

6-Jun-2022

<!-- image -->

SCALE  4.0

## WQFN - 0.8 mm max height

PLASTIC QUAD FLATPACK - NO LEAD

<!-- image -->

## NOTES:

1. All linear dimensions are in millimeters. Any dimensions in parenthesis are for reference only. Dimensioning and tolerancing per ASME Y14.5M.
2. This drawing is subject to change without notice.
3. The package thermal pad must be soldered to the printed circuit board for thermal and mechanical performance.

<!-- image -->

PLASTIC QUAD FLATPACK - NO LEAD

<!-- image -->

NOTES: (continued)

4. This package is designed to be soldered to a thermal pad on the board. For more information, see Texas Instruments literature number SLUA271 (www.ti.com/lit/slua271).
5. Vias are optional depending on application, refer to device data sheet. If any vias are implemented, refer to their locations shown on this view. It is recommended that vias under paste be filled, plugged or tented.

<!-- image -->

PLASTIC QUAD FLATPACK - NO LEAD

<!-- image -->

NOTES: (continued)

6. Laser cutting apertures with trapezoidal walls and rounded corners may offer better paste release. IPC-7525 may have alternate design recommendations.

<!-- image -->

## IMPORTANT NOTICE AND DISCLAIMER

TI PROVIDES TECHNICAL AND RELIABILITY DATA (INCLUDING DATA SHEETS), DESIGN RESOURCES (INCLUDING REFERENCE DESIGNS), APPLICATION OR OTHER DESIGN ADVICE, WEB TOOLS, SAFETY INFORMATION, AND OTHER RESOURCES 'AS IS' AND WITH ALL FAULTS, AND DISCLAIMS ALL WARRANTIES, EXPRESS AND IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY RIGHTS.

These resources are intended for skilled developers designing with TI products. You are solely responsible for (1) selecting the appropriate TI products for your application, (2) designing, validating and testing your application, and (3) ensuring your application meets applicable standards, and any other safety, security, regulatory or other requirements.

These resources are subject to change without notice. TI grants you permission to use these resources only for development of an application that uses the TI products described in the resource. Other reproduction and display of these resources is prohibited. No license is granted to any other TI intellectual property right or to any third party intellectual property right. TI disclaims responsibility for, and you will fully indemnify TI and its representatives against, any claims, damages, costs, losses, and liabilities arising out of your use of these resources.

TI's products are provided subject to TI's Terms of Sale or other applicable terms available either on ti.com or provided in conjunction with such TI products. TI's provision of these resources does not expand or otherwise alter TI's applicable warranties or warranty disclaimers for TI products.

TI objects to and rejects any additional or different terms you may have proposed. IMPORTANT NOTICE

Mailing Address: Texas Instruments, Post Office Box 655303, Dallas, Texas 75265 Copyright © 202 3 , Texas Instruments Incorporated