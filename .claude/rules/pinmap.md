# M5Stack Core2 for AWS Hardware Specification
## Board Architecture

```
System = Core2 Main Board + M5Bus Add-on Board
MCU = ESP32-D0WDQ6-V3
PMU = AXP192
```

Two hardware layers exist:

| Layer        | Description                                        |
| ------------ | -------------------------------------------------- |
| Core Board   | MCU, LCD, touch, PMU, SD, RTC, speaker, USB        |
| M5Bus Add-on | RGB LEDs, microphone, IMU, secure element, sockets |

All add-on peripherals connect through **M5Bus expansion connector**.

---

# MCU

## Processor

```
ESP32-D0WDQ6-V3
```

Dual-core Xtensa LX6 @ 240 MHz.

### On-package / on-board memory

```
Flash = 16 MB (XM25QH128)
PSRAM = 8 MB (ESPSRAM64H)
```

### Reserved pins (cannot be repurposed)

Flash:

```
GPIO6
GPIO7
GPIO8
GPIO9
GPIO10
GPIO11
```

PSRAM:

```
GPIO16
GPIO17
```

UART programming:

```
GPIO1
GPIO3
```

---

# Power System

## Power Management IC

```
AXP192
```

Interface:

```
I2C (GPIO21 / GPIO22)
```

Functions:

* battery charging
* power rails
* system power control
* peripheral power switching

### Generated rails

| Rail      | Source        |
| --------- | ------------- |
| MCU_VDD   | AXP192 DCDC1  |
| PERI_VDD  | AXP192 LDO2   |
| RTC_VDD   | AXP192 LDO1   |
| VIB_MOTOR | AXP192 LDO3   |
| IPS_BUS   | AXP192 IPSOUT |
| BUS_5V    | SY7088 boost  |
| USB_5V    | USB input     |

---

## Boost Converter

```
SY7088
```

Purpose:

```
IPS_BUS → BUS_5V
```

Enable pin:

```
BST_EN
```

Controlled by:

```
AXP192 EXTEN
```

---

# System Buses

## Internal I2C Bus

```
SDA = GPIO21
SCL = GPIO22
```

Pull-ups present.

### Devices

| Device   | Board  | Function         | I2C Addr |
| -------- | ------ | ---------------- | -------- |
| AXP192   | core   | PMU              | 0x34     |
| BM8563   | core   | RTC              | 0x51     |
| FT6336U  | core   | touch controller | 0x38     |
| MPU6886  | add-on | IMU              | 0x68     |
| ATECC608 | add-on | secure element   | 0x35     |

Note: The add-on board IMU and secure element reach this internal bus through
M5Bus pins 17/18 (GPIO21/GPIO22). The add-on J3 I2C socket is also on this bus.

---

## External I2C Bus (Port A)

```
SDA = GPIO32
SCL = GPIO33
```

Exposed through the external **Port A** (red) Grove connector only.

Used by:

| Device                       | Board |
| ---------------------------- | ----- |
| External I2C "unit" accessory | core  |

Not used by any on-board peripheral.

---

## SPI Bus

Shared bus.

```
MOSI = GPIO23
MISO = GPIO38
SCK  = GPIO18
```

Devices:

| Device       | CS    |
| ------------ | ----- |
| ILI9342C LCD | GPIO5 |
| SD card      | GPIO4 |

Important:

```
LCD and SD share the same SPI controller
```

Drivers must coordinate bus access.

---

## UART

### UART0

```
TX = GPIO1
RX = GPIO3
```

Connected to:

```
CP2104 USB-UART
```

Used for:

```
programming / console
```

---

### UART2

```
RX = GPIO13
TX = GPIO14
```

Exposed through add-on board socket.

---

# Core Board Devices

---

# Display

Controller:

```
ILI9342C
```

Interface:

```
SPI
```

Pins:

| Signal | GPIO    |
| ------ | ------- |
| MOSI   | GPIO23  |
| MISO   | GPIO38  |
| SCK    | GPIO18  |
| CS     | GPIO5   |
| DC     | GPIO15  |
| RST    | LCD_RST |

Backlight:

```
LCD_BL (PMU-controlled rail)
```

---

# Touch Panel

Controller:

```
FT6336U
```

I2C address:

```
0x38
```

Interface:

```
I2C (internal)
```

Pins:

| Signal | GPIO    |
| ------ | ------- |
| SDA    | GPIO21  |
| SCL    | GPIO22  |
| INT    | GPIO39  |
| RST    | LCD_RST |

Important:

```
Touch and LCD share the same reset signal
```

---

# Real Time Clock

```
BM8563
```

I2C address:

```
0x51
```

Interface:

```
I2C (internal)
```

Pins:

| Signal | GPIO   |
| ------ | ------ |
| SDA    | GPIO21 |
| SCL    | GPIO22 |

Backup battery present on early revisions only (RTC coin cell removed in the
2023.27 hardware revision; timekeeping while powered is unaffected).

---

# Speaker

Amplifier:

```
NS4168
```

Interface:

```
digital audio
```

Pins:

| Signal | GPIO   |
| ------ | ------ |
| LRCK   | GPIO0  |
| BCLK   | GPIO12 |
| DATA   | GPIO2  |

Enable signal:

```
SPK_EN
```

Controlled by:

```
AXP192 GPIO2
```

---

# Vibration Motor

Device:

```
1027 DC motor
```

Power rail:

```
VIB_MOTOR
```

Source:

```
AXP192 LDO3
```

Not directly driven by MCU GPIO.

---

# SD Card

Interface:

```
SPI
```

Pins:

| Signal | GPIO   |
| ------ | ------ |
| CS     | GPIO4  |
| MOSI   | GPIO23 |
| MISO   | GPIO38 |
| SCK    | GPIO18 |

Mode:

```
SPI mode (not SDMMC)
```

---

# USB Interface

Chip:

```
CP2104
```

Connections:

| Signal | ESP32 |
| ------ | ----- |
| TXD    | GPIO3 |
| RXD    | GPIO1 |

USB connector:

```
USB-C
```

---

# Storage

## Flash

```
XM25QH128
```

Connected to ESP32 QSPI.

## PSRAM

```
ESPSRAM64H
```

Connected to ESP32 QSPI.

---

# Add-On Board Devices

Connected through **M5Bus**.

---

# RGB LED Chain

LED type:

```
SK6812
```

Quantity:

```
10 LEDs
```

Interface:

```
single-wire addressable
```

Control pin:

```
GPIO25
```

GPIO25 (M5Bus pin 8, net "RGB") drives the chain through transistor Q1.
GPIO25 is also the DAC2 pin, so those two uses are mutually exclusive.

Power:

```
5V
```

Driver should treat this as:

```
1 device containing 10 pixels
```

---

# Microphone

Device:

```
SPM1423HM4H-B
```

Digital microphone.

Pins:

| Signal | GPIO   |
| ------ | ------ |
| CLK    | GPIO0  |
| DATA   | GPIO34 |

Important:

```
GPIO0 is shared with speaker LRCK
```

Audio subsystem must coordinate.

---

# IMU

Device:

```
MPU6886
```

I2C address:

```
0x68
```

Interface:

```
I2C (internal)
```

Pins:

| Signal | GPIO   |
| ------ | ------ |
| SDA    | GPIO21 |
| SCL    | GPIO22 |

Located on the add-on board; reaches the internal I2C bus via M5Bus pins 17/18.

Address select:

```
AD0 pulled LOW
```

---

# Secure Element

Device:

```
ATECC608
```

I2C address:

```
0x35
```

Interface:

```
I2C (internal)
```

Pins:

| Signal | GPIO   |
| ------ | ------ |
| SDA    | GPIO21 |
| SCL    | GPIO22 |

Located on the add-on board; shares the internal I2C bus with the IMU,
AXP192, BM8563, and touch panel via M5Bus pins 17/18.

---

# Add-On Sockets

The M5GO-Bottom2 for AWS expansion board adds two Grove (HY2.0-4P) ports plus
an internal-I2C pogo/socket. These ports do **not** exist on the standalone
Core2 main board; they are provided by the AWS add-on/expansion base.

## UART socket (Port C, blue)

Signals:

| Pin | Signal |
| --- | ------ |
| 1   | RXD    |
| 2   | TXD    |
| 3   | VCC    |
| 4   | GND    |

Mapped to:

```
GPIO13 (RXD2) / GPIO14 (TXD2)
```

Labeled **Port C (blue)** in the official M5Stack documentation.

---

## GPIO socket (Port B, black)

Signals:

```
GPIO36 (ADC)
GPIO26 (DAC)
```

Labeled **Port B (black)** in the official M5Stack documentation.

---

## I2C socket

Signals:

```
SDA
SCL
5V
GND
```

Mapped to the internal I2C bus (GPIO21/GPIO22), with 4.7k pull-ups to +3.3V.
Exposed as a pogo-pin / socket on the add-on base, not a colored Grove port.

---

# M5Bus Connector Mapping

| Pin | Signal         | ESP32   |
| --- | -------------- | ------- |
| 1   | GND            | -       |
| 2   | GPIO35         | GPIO35  |
| 3   | GND            | -       |
| 4   | GPIO36         | GPIO36  |
| 5   | GND            | -       |
| 6   | RESET          | EN      |
| 7   | MOSI           | GPIO23  |
| 8   | DAC            | GPIO25  |
| 9   | MISO           | GPIO38  |
| 10  | DAC            | GPIO26  |
| 11  | SCK            | GPIO18  |
| 12  | 3.3V           | -       |
| 13  | RXD0           | GPIO3   |
| 14  | TXD0           | GPIO1   |
| 15  | RXD2           | GPIO13  |
| 16  | TXD2           | GPIO14  |
| 17  | SDA (internal) | GPIO21  |
| 18  | SCL (internal) | GPIO22  |
| 19  | SDA (external) | GPIO32  |
| 20  | SCL (external) | GPIO33  |
| 21  | GPIO27         | GPIO27  |
| 22  | GPIO19         | GPIO19  |
| 23  | GPIO2          | GPIO2   |
| 24  | GPIO0          | GPIO0   |
| 25  | HPWR           | -       |
| 26  | GPIO34         | GPIO34  |
| 27  | HPWR           | -       |
| 28  | 5V             | -       |
| 29  | HPWR           | -       |
| 30  | BAT            | battery |

---

# Shared Pin Constraints

## Shared SPI bus

```
LCD + SD share SPI
```

Drivers must coordinate.

---

## Shared reset line

```
LCD_RST used by LCD and touch
```

Driven by AXP192 GPIO4 (NMOS open-drain), not an ESP32 GPIO.

---

## Shared audio clock

```
GPIO0 used by speaker LRCK and microphone CLK
```

Audio drivers must coordinate initialization.

---

## External I2C shared bus

Devices:

```
external Port A "unit" accessories only
```

No on-board peripheral uses this bus.

---

# Driver Initialization Order

Recommended BSP startup sequence:

```
1 Initialize ESP32 core
2 Initialize AXP192 PMU
3 Enable required power rails
4 Initialize internal I2C bus
5 Initialize external I2C bus
6 Initialize touch + RTC
7 Initialize IMU + secure element
8 Initialize SPI bus
9 Initialize LCD
10 Initialize SD card
11 Initialize audio subsystem
12 Initialize RGB LED chain
13 Initialize external sockets
```

---

# Audio Subsystem Constraints

Speaker:

```
GPIO0
GPIO12
GPIO2
```

Microphone:

```
GPIO0
GPIO34
```

Shared clock pin:

```
GPIO0
```

Audio drivers must avoid conflicting configurations.

---

# Reserved Pins

The BSP must not repurpose these:

```
GPIO0  audio + boot strap
GPIO1  UART0 TX
GPIO3  UART0 RX
GPIO6-11 flash
GPIO16-17 PSRAM
GPIO23 SPI MOSI
GPIO18 SPI SCK
GPIO38 SPI MISO
GPIO5 LCD CS
GPIO4 SD CS
GPIO21 internal SDA
GPIO22 internal SCL
GPIO32 external SDA
GPIO33 external SCL
GPIO25 RGB LED
GPIO34 microphone
GPIO39 touch interrupt
```

---

# Summary

This board should be modeled as:

```
ESP32 system with:
- PMU-managed power rails
- shared SPI display/storage bus
- two independent I2C buses
- shared audio clock line
- external expansion board consuming multiple GPIO
```

Drivers must account for:

```
shared buses
shared reset lines
shared audio clock
PMU-controlled power rails
```
