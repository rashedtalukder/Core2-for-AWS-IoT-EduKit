# M5Stack Core2 for AWS – Hardware Reference (LLM Driver / BSP Specification)

## Board Overview

The **M5Stack Core2 for AWS** is an ESP32-based embedded system with integrated display, touch panel, audio amplifier, RTC, power management, and expansion bus.

Primary components:

| Component      | Function                    |
| -------------- | --------------------------- |
| ESP32-D0WDQ6   | Main MCU                    |
| AXP192         | Power Management Unit       |
| ILI9342C       | LCD controller              |
| FT6336U        | Capacitive touch controller |
| MPU6886        | IMU sensor                  |
| BM8563         | RTC                         |
| NS4168         | Speaker amplifier           |
| SY7088         | 5V boost converter          |
| ATECC608       | Secure element              |
| SPM1423        | Microphone                  |
| CP2104         | USB-UART bridge             |
| XM25QH128      | SPI Flash                   |
| ESPSRAM64H     | PSRAM                       |
| TF card socket | microSD storage             |
| 1027 DC motor  | vibration motor             |

---

# MCU

## ESP32-D0WDQ6

### Power rails

| Rail       | Voltage       | Usage   |
| ---------- | ------------- | ------- |
| MCU_VDD    | 3.3V          | core IO |
| VDDA       | analog supply |         |
| VDD3P3_CPU | CPU           |         |
| VDD3P3_RTC | RTC domain    |         |
| VDD_SDIO   | SPI flash     |         |

---

## ESP32 Pin Map

| GPIO   | Function               |
| ------ | ---------------------- |
| GPIO0  | Boot / speaker control |
| GPIO1  | UART TX0               |
| GPIO2  | speaker I2S            |
| GPIO3  | UART RX0               |
| GPIO4  | SD card                |
| GPIO5  | LCD CS                 |
| GPIO12 | speaker BCLK           |
| GPIO13 | UART2 RX               |
| GPIO14 | UART2 TX               |
| GPIO15 | LCD DC                 |
| GPIO16 | PSRAM                  |
| GPIO17 | PSRAM                  |
| GPIO18 | SPI SCK                |
| GPIO19 | M5Bus GPIO             |
| GPIO21 | I2C SDA                |
| GPIO22 | I2C SCL                |
| GPIO23 | SPI MOSI               |
| GPIO25 | DAC                    |
| GPIO26 | DAC                    |
| GPIO27 | general IO             |
| GPIO32 | external I2C SDA       |
| GPIO33 | external I2C SCL       |
| GPIO34 | ADC                    |
| GPIO35 | ADC                    |
| GPIO36 | ADC                    |
| GPIO38 | SPI MISO               |
| GPIO39 | touch interrupt        |

---

# System Buses

## Internal I2C Bus

Pins:

```
SDA = GPIO21
SCL = GPIO22
```

Pull-ups present on board.

**All on-board I2C peripherals share this bus**, including devices physically located on the add-on board (MPU6886, ATECC608). These reach GPIO21/GPIO22 through M5Bus pins 17/18.

Devices on this bus:

| Device   | Board     | Function         |
| -------- | --------- | ---------------- |
| AXP192   | Core      | PMU              |
| FT6336U  | Core      | Touch controller |
| BM8563   | Core      | RTC              |
| MPU6886  | Add-on    | IMU              |
| ATECC608 | Add-on    | Secure element   |

---

## External I2C Bus (Port A)

```
SDA = GPIO32
SCL = GPIO33
```

Available through the external **Port A** (red) Grove connector. **Used only for plug-in "unit" accessories**, not by any on-board peripheral.

---

## SPI Bus (Primary)

```
MOSI = GPIO23
MISO = GPIO38
SCK  = GPIO18
```

Note: GPIO19 appears on M5Bus pin 22 but is **not connected** to the primary SPI bus.

Devices:

| Device  | CS    |
| ------- | ----- |
| LCD     | GPIO5 |
| microSD | GPIO4 |

---

## SPI Flash

Chip: **XM25QH128**

Pins:

| Signal | ESP32  |
| ------ | ------ |
| CS     | GPIO11 |
| SCLK   | GPIO6  |
| MOSI   | GPIO7  |
| MISO   | GPIO8  |
| WP     | GPIO9  |
| HOLD   | GPIO10 |

---

## PSRAM

Chip: **ESPSRAM64H**

Pins:

| Signal | ESP32    |
| ------ | -------- |
| CS     | GPIO17   |
| CLK    | GPIO16   |
| SIO0-3 | GPIO7-10 |

---

# Display

Controller: **ILI9342C**

Interface: **SPI**

Signals:

| Signal | ESP32           |
| ------ | --------------- |
| MOSI   | GPIO23          |
| MISO   | GPIO38          |
| SCK    | GPIO18          |
| CS     | GPIO5           |
| DC     | GPIO15          |
| RST    | AXP192 GPIO4    |
| BL     | AXP192 DCDC3    |

Power:

```
PERI_VDD (3.3V)
```

Note: Display reset is controlled via the AXP192 PMU's GPIO4 (NMOS open-drain), not an ESP32 GPIO. Backlight brightness is controlled by the AXP192 DCDC3 rail voltage (2.2V–3.3V).

---

# Touch Panel

Controller: **FT6336U**

Interface: **I2C**

Pins:

| Signal | ESP32           |
| ------ | --------------- |
| SDA    | GPIO21          |
| SCL    | GPIO22          |
| INT    | GPIO39          |
| RST    | AXP192 GPIO4    |

Note: Touch reset is shared with the display reset line, both controlled via AXP192 GPIO4.

---

# IMU

Chip: **MPU6886** (located on add-on board)

Interface: **I2C** (internal bus via M5Bus pins 17/18)

Pins:

| Signal | ESP32  |
| ------ | ------ |
| SDA    | GPIO21 |
| SCL    | GPIO22 |

---

# RTC

Chip: **BM8563**

Interface: **I2C**

Pins:

| Signal | ESP32  |
| ------ | ------ |
| SDA    | GPIO21 |
| SCL    | GPIO22 |
| INT    | GPIO21 |

Clock crystal:

```
32.768 kHz
```

Backup battery present.

---

# Secure Element

Chip: **ATECC608** (located on add-on board)

Interface: **I2C** (internal bus via M5Bus pins 17/18)

Pins:

| Signal | ESP32  |
| ------ | ------ |
| SDA    | GPIO21 |
| SCL    | GPIO22 |

---

# Audio System

## Microphone

Device: **SPM1423**

Interface:

```
I2S
```

Signals:

| Signal | ESP32  |
| ------ | ------ |
| DATA   | GPIO34 |
| CLK    | GPIO0  |

---

## Speaker Amplifier

Chip: **NS4168**

Interface: **I2S**

Pins:

| Signal | ESP32  |
| ------ | ------ |
| LRCK   | GPIO0  |
| BCLK   | GPIO12 |
| DATA   | GPIO2  |
| CTRL   | GPIO0  |

Speaker output to onboard speaker.

---

# Vibration Motor

Device: **1027 DC Motor**

Control:

```
VIB_MOTOR
```

Driven through transistor.

ESP32 control pin:

```
GPIO21
```

---

# Storage

## microSD Card

Interface: **SPI**

Pins:

| Signal | ESP32  |
| ------ | ------ |
| CS     | GPIO4  |
| MOSI   | GPIO23 |
| MISO   | GPIO38 |
| SCK    | GPIO18 |

Power:

```
PERI_VDD
```

---

# USB Interface

Bridge chip:

```
CP2104
```

Connections:

| Signal | ESP32 |
| ------ | ----- |
| TX     | GPIO3 |
| RX     | GPIO1 |

USB-C connector provides:

```
USB_5V
```

---

# Power System

## PMU

Chip: **AXP192**

Functions:

* battery charging
* power rails
* power monitoring
* voltage regulators

Interface:

```
I2C
```

Connected to:

```
GPIO21 / GPIO22
```

Main rails generated:

| Rail     | Usage        |
| -------- | ------------ |
| IPS_BUS  | system power |
| MCU_VDD  | MCU 3.3V     |
| PERI_VDD | peripherals  |
| RTC_VDD  | RTC          |
| SYS_VBAT | battery      |

---

# Boost Converter

Chip:

```
SY7088
```

Function:

```
3.3V → 5V boost
```

Enable pin:

```
BST_EN
```

Used for:

```
BUS_5V
```

---

# Expansion Bus (M5Bus)

Connector exposes ESP32 signals.

## Pinout

| Pin | Signal |
| --- | ------ |
| 1   | GND    |
| 2   | GPIO35 |
| 3   | GND    |
| 4   | GPIO36 |
| 5   | GND    |
| 6   | EN     |
| 7   | GPIO23 |
| 8   | GPIO25 |
| 9   | GPIO38 |
| 10  | GPIO26 |
| 11  | GPIO18 |
| 12  | 3.3V   |
| 13  | GPIO3  |
| 14  | GPIO1  |
| 15  | GPIO13 |
| 16  | GPIO14 |
| 17  | GPIO21 |
| 18  | GPIO22 |
| 19  | GPIO32 |
| 20  | GPIO33 |
| 21  | GPIO27 |
| 22  | GPIO19 |
| 23  | GPIO2  |
| 24  | GPIO0  |
| 25  | HPWR   |
| 26  | GPIO34 |
| 27  | HPWR   |
| 28  | 5V     |
| 29  | HPWR   |
| 30  | BAT    |

---

# External Ports

Three Grove-compatible ports are exposed on the M5Stack Core2 for external "unit" accessories.

## Port A (Red) — I2C

Connector J4.

| Pin | Signal          | GPIO   |
| --- | --------------- | ------ |
| 1   | EXT_I2C_SDA     | GPIO32 |
| 2   | EXT_I2C_SCL     | GPIO33 |
| 3   | 5V              | —      |
| 4   | GND             | —      |

Used only for external I2C "unit" accessories. **Not used by any on-board peripheral** — all on-board I2C devices use the internal bus (GPIO21/GPIO22).

## Port B (Black) — DAC / ADC

| Pin | Signal | GPIO   |
| --- | ------ | ------ |
| 1   | DAC    | GPIO26 |
| 2   | ADC    | GPIO36 |
| 3   | 5V     | —      |
| 4   | GND    | —      |

Used for analog input/output "unit" devices.

## Port C (Blue) — UART

| Pin | Signal   | GPIO   |
| --- | -------- | ------ |
| 1   | UART2_RX | GPIO13 |
| 2   | UART2_TX | GPIO14 |
| 3   | 5V       | —      |
| 4   | GND      | —      |

Used for serial "unit" devices (UART2).

---

# External Battery

Connector J5.

Pins:

| Pin | Signal   |
| --- | -------- |
| 1   | SYS_VBAT |
| 2   | GND      |

---

# Buttons

## Reset button

Signal:

```
MCU_RST
```

Connected to ESP32 reset.

---

## Power button

Signal:

```
PWR_KEY
```

Connected to PMU.

---

# LEDs

## System LED

Signal:

```
SYS_LED
```

Connected to ESP32.

---

# Important Power Rails

| Rail     | Voltage      |
| -------- | ------------ |
| USB_5V   | USB input    |
| BUS_5V   | boosted 5V   |
| IPS_BUS  | system power |
| MCU_VDD  | 3.3V         |
| PERI_VDD | peripherals  |
| RTC_VDD  | RTC          |
| SYS_VBAT | battery      |

---

# Driver Implementation Notes

### I2C devices (all on internal bus)

Drivers required:

```
AXP192    (core board)
BM8563    (core board)
FT6336U   (core board)
MPU6886   (add-on board)
ATECC608  (add-on board)
```

Bus (internal I2C — shared by all on-board peripherals):

```
GPIO21 SDA
GPIO22 SCL
```

---

### SPI devices

Drivers required:

```
ILI9342C LCD
microSD
```

Bus:

```
MOSI GPIO23
MISO GPIO38
SCK GPIO18
```

---

### Audio drivers

```
I2S microphone
I2S speaker amplifier
```

---

### Power management

PMU driver required for:

```
battery level
voltage rails
power control
```

---

### Expansion bus

External "unit" modules connect through the Grove ports:

- **Port A** (Red) — I2C (GPIO32/GPIO33)
- **Port B** (Black) — DAC/ADC (GPIO26/GPIO36)
- **Port C** (Blue) — UART2 (GPIO13/GPIO14)

The **M5Bus** 30-pin connector joins the core board to the add-on board, carrying power and GPIO signals including the internal I2C bus.
