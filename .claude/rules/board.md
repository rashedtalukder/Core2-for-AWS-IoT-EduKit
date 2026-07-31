# BSP Steering Notes for M5Stack Core2 for AWS + M5GO Bottom for AWS

## Scope and modeling rule

* Treat this platform as **two coupled boards**:

  1. **Core2 main board**
  2. **M5GO Bottom for AWS add-on board**
* The add-on board is **not independent**. It inherits power, buses, and GPIO through the **M5Bus connector**.
* BSP generation should model:

  * one ESP32 SoC
  * one main-board PMU/power hierarchy
  * one attached expansion board with its own peripherals and connectors

---

# Core architectural corrections

## 1. This is not a generic ESP32 board

* Do not assume standard ESP32 devkit defaults for:

  * SPI display wiring
  * I2C device placement
  * SD-card mode
  * audio pins
  * backlight control
  * vibrator control
  * reset ownership
* The BSP must follow actual board wiring, not reference-driver defaults.

## 2. The add-on board consumes M5Bus lines that might otherwise look “free”

* Several GPIOs exposed on M5Bus are actually used by the add-on board.
* BSP must not expose these as generally available application GPIOs if the add-on board is part of the target configuration.

---

# Main board notes retained and reinforced

## 3. LCD and SD card share one SPI bus

* The Core2 LCD and TF card socket share:

  * `MOSI = GPIO23`
  * `MISO = GPIO38`
  * `SCK = GPIO18`
* Separate chip selects:

  * `LCD CS = GPIO5`
  * `SD CS = GPIO4`
* BSP must create:

  * one shared SPI controller
  * separate SPI devices for LCD and SD
  * bus arbitration and per-device config
* Do not generate separate hardware SPI buses for LCD and SD.

## 4. Internal I2C and external I2C are different buses

* Internal I2C on Core2:

  * `GPIO21 = SDA`
  * `GPIO22 = SCL`
* External I2C through Port A:

  * `GPIO32 = SDA`
  * `GPIO33 = SCL`
* BSP must keep them distinct.
* All on-board peripherals — including add-on board devices (MPU6886, ATECC608) — are on the internal bus (GPIO21/22). The external bus (GPIO32/33) is reserved for Port A "unit" accessories only.

## 5. LCD reset and touch reset remain a shared reset domain

* `LCD_RST` is shared between:

  * display reset
  * touch reset
  * PMU-controlled reset path
* BSP must not generate separate independent reset controls for LCD and touch.

## 6. AXP192 remains central to board bring-up

* PMU is still mandatory for correct board behavior.
* BSP should initialize PMU before higher-level peripherals that depend on PMU-controlled rails or control signals.

---

# New add-on board wiring notes

## 7. The add-on board devices use the internal I2C bus on GPIO21/GPIO22

From the add-on schematic:

* `I2C_SDA` is carried over M5Bus pin 17 and maps to:

  * `GPIO21`
* `I2C_SCL` is carried over M5Bus pin 18 and maps to:

  * `GPIO22`

Add-on board devices on this bus include:

* **MPU6886**
* **ATECC608**
* the J3 **I2C socket**

BSP must:

* place add-on I2C peripherals on the Core2 **internal I2C bus** (GPIO21/GPIO22)
* not fabricate a separate add-on I2C bus on GPIO32/GPIO33
* recognize the external I2C bus (GPIO32/GPIO33) is reserved for Port A units only

## 8. The add-on board provides additional pull-ups on the internal I2C bus

The add-on schematic shows:

* `R2 = 4.7k` pull-up on `I2C_SDA`
* `R3 = 4.7k` pull-up on `I2C_SCL`

The Core2 main board already has pull-ups on the internal I2C bus.

Implication for BSP:

* Software does not normally need to care, but documentation should note that:

  * internal I2C pull-up strength is the combination of main board and add-on board pull-ups
* Do not assume absence of pull-ups when bringing up internal I2C peripherals.

## 9. MPU6886 is on the internal I2C bus

The add-on schematic explicitly shows **MPU-6886** connected to:

* `I2C_SCL`
* `I2C_SDA`

which reach the MCU as:

* `GPIO22 / GPIO21` through M5Bus pins 18/17

BSP must:

* place `MPU6886` on the **internal I2C bus** (GPIO21/GPIO22)
* not place it on the external I2C bus (GPIO32/GPIO33)

## 10. ATECC608 is also on the internal I2C bus

The **ATECC608** secure element uses address `0x35` and is connected to:

* `I2C_SCL`
* `I2C_SDA`

Thus it is also on:

* `GPIO22 / GPIO21`

BSP must:

* place secure element on the **internal I2C bus**
* not fabricate an external-I2C connection for it
* use the board-fixed 7-bit address `0x35` at 100 kHz rather than inheriting a
  consuming project's generic cryptoauthlib address
* use `ATECC608` as the canonical component name in BSP code and documentation

## 11. The add-on board also exposes that same internal I2C bus to a socket

The add-on board J3 “Socket Power 4P” connector carries:

* `5V`
* `GND`
* `SDA`
* `SCL`

These are the same internal I2C signals:

* `GPIO21`
* `GPIO22`

Implication:

* external connector peripherals may share the same bus as onboard add-on devices
* BSP should model the internal I2C bus as a **multi-drop bus**
* do not assume MPU6886 and ATECC608 are the only devices present

---

# New UART notes from the add-on board

## 12. UART2 is exported to both the add-on board and an external socket

The add-on board schematic shows:

* `U2_RXD`
* `U2_TXD`

These are routed to:

* external UART socket
* M5Bus pins corresponding to:

  * `GPIO13`
  * `GPIO14`

From the Core2 M5Bus mapping:

* `GPIO13 = RXD2`
* `GPIO14 = TXD2` 

BSP must:

* map UART2 to:

  * `RX = GPIO13`
  * `TX = GPIO14`
* understand that these lines are externally exposed and potentially used by connected modules

## 13. UART2 should not be treated as unused general GPIO if the add-on board is present

Because the add-on board routes UART2 to its own connector:

* do not casually repurpose `GPIO13` / `GPIO14`
* BSP should reserve them for:

  * add-on board UART header support
  * optional module communication

---

# New microphone notes

## 14. The microphone is on the add-on board and is not I2C

The add-on schematic shows microphone:

* **SPM1423HM4H-B**

Signals:

* `DAT`
* `CLK`

Power:

* `3.3V`

This is not wired as an analog microphone and not on I2C.

BSP must:

* model microphone as a **digital mic interface**
* not invent an ADC-based microphone driver
* not invent I2C control for the mic

## 15. The add-on board maps microphone clock/data to M5Bus GPIOs

The add-on board M5Bus block labels:

* `CLK` on M5Bus side mapped to **GPIO0**
* `DAT` on M5Bus side mapped to **GPIO34**

So microphone wiring is:

* `MIC_CLK = GPIO0`
* `MIC_DATA = GPIO34`

BSP must use these exact pins.

## 16. Microphone clock shares GPIO0 with the speaker LRCK on the main board

This is a critical nuance.

Main board speaker amp uses:

* `LRCK = GPIO0` 

Add-on microphone uses:

* `CLK = GPIO0`

Implication:

* the microphone and speaker path share a key audio timing pin
* autogenerated BSP must **not assume independent unrelated audio pin sets**
* simultaneous full-duplex audio may require a deliberate shared-audio configuration, not separate unrelated drivers
* careless generation of separate mic and speaker drivers may produce conflicting pin ownership

Recommended BSP steering:

* treat audio as a **shared subsystem**
* centralize ownership of:

  * `GPIO0`
  * any shared digital audio clocking assumptions
* avoid initializing mic and speaker independently without checking pin conflict

## 17. GPIO0 is now even more constrained than on the main board alone

GPIO0 already had:

* boot/strap sensitivity
* auto-programming implications
* speaker LRCK use

With the add-on board it also carries:

* microphone clock

BSP should:

* strongly reserve `GPIO0`
* avoid exposing it as generic GPIO
* perform audio initialization only after stable boot
* document that audio pin multiplexing around `GPIO0` is board-specific and conflict-prone

## 18. Microphone data uses GPIO34, which is input-only on ESP32-class parts

The add-on mic data line is on:

* `GPIO34`

That is appropriate for a microphone data input.

BSP should:

* model `GPIO34` as input-only
* not generate code that tries to drive it
* treat it as microphone receive data only for this board role

---

# New IMU notes

## 19. MPU6886 address selection is hard-wired

The add-on board shows MPU6886 with:

* `AD0/SDO` pulled down via `R5 4.7k` to GND

Implication:

* the address/select state is fixed by hardware
* BSP should not invent a runtime address-select GPIO
* driver should assume the address corresponding to `AD0 = 0`

## 20. MPU6886 interrupt is available but not clearly routed through M5Bus in the shown view

The add-on schematic shows `INT` on the MPU6886 symbol, but the visible snippet does not clearly show a routed host GPIO connection.

BSP should:

* not assume interrupt support unless the full pin route is proven
* support polling mode by default
* only enable IMU interrupt-driven mode if a confirmed host GPIO mapping is available from a fuller source

## 21. MPU6886 CS/SDO pins indicate the device is configured for I2C-style use

The add-on board wiring shows `I2C_SCL` and `I2C_SDA`, and the chip is not being presented as SPI-wired for host use.

BSP must:

* generate I2C IMU support
* not create an SPI MPU6886 driver for this board

---

# New secure element notes

## 22. ATECC608 is powered from 3.3V and attached directly to the internal I2C bus

The add-on schematic shows:

* `VCC = +3.3V`
* `SCL = I2C_SCL`
* `SDA = I2C_SDA`

BSP should:

* place secure element on the internal I2C bus (GPIO21/GPIO22)
* not generate a separate enable/reset GPIO unless another sheet proves one exists

## 23. ATECC608 should be considered a shared-bus device with strict transaction behavior

Because it is on the same internal I2C bus as:

* MPU6886
* AXP192, BM8563, touch panel
* the J3 I2C socket devices

BSP should:

* serialize accesses correctly
* avoid assumptions that bus latency and wake behavior are isolated from other devices

---

# New RGB LED notes

## 24. The add-on board includes a daisy-chained RGB LED string

The add-on schematic shows:

* `RGB LED * 10`
* parts labeled `SK6812`
* daisy-chained from one LED to the next

This is a one-wire addressable LED chain, not separate PWM LEDs.

BSP must:

* create a single addressable RGB LED device representing **10 chained pixels**
* not create ten independent GPIO-controlled LEDs
* not generate standard three-channel PWM RGB LED drivers

## 25. The LED chain is driven from M5Bus GPIO25

In the add-on M5Bus mapping, the net labeled `RGB` is connected to:

* `GPIO25` (M5Bus pin 8)

So:

* `RGB_DATA = GPIO25`

BSP must use GPIO25 as the one-wire LED output. Note that GPIO25 is also the
DAC2 pin, so the two uses are mutually exclusive. GPIO26 (M5Bus pin 10) is
*not* the LED line — it passes through to the add-on GPIO socket J2.

## 26. The SK6812 chain is powered from 5V, not 3.3V

The LED schematic shows:

* LED supply at `+5V`

Implications:

* LED power domain is not the same as 3.3V logic domain
* BSP documentation should note that LED activity depends on 5V availability through the add-on power path
* do not model RGB power as if sourced from a 3.3V MCU rail

## 27. The RGB data line has a pull-up / bias arrangement and transistor stage nearby

The add-on schematic shows:

* `R7 4.7k`
* `R8 1k`
* `Q1 SS8550`

This suggests the RGB data/power drive path is board-specific and not a bare direct-MCU-to-LED chain.

BSP should:

* still treat it logically as a single GPIO-driven SK6812 output on `GPIO25`
* avoid over-assuming direct electrical topology if generating timing-sensitive notes
* not remap this to another pin

---

# New power notes from the add-on board

## 28. The add-on board has its own local battery/charging circuit

The add-on schematic includes:

* `TP4057`
* `BAT+`
* battery indicator LEDs

This means the add-on board has a local battery-related power section in addition to Core2 power domains.

BSP implications:

* software should not assume all battery behavior is owned solely by the Core2 AXP192
* however, no obvious host-control interface for the TP4057 is shown, so this is mostly a hardware/power note rather than a software driver requirement

## 29. The add-on board consumes both 3.3V and 5V from M5Bus

From the add-on board:

* IMU / secure element / mic use `+3.3V`
* RGB LEDs use `+5V`
* sockets also expose `+5V`

BSP should recognize that attached add-on peripherals are split across power domains:

* logic/peripherals on 3.3V
* LEDs / sockets on 5V

---

# Expansion connector notes with add-on present

## 30. M5Bus is no longer just an external expansion port; it is part of the board implementation

Once this add-on is attached, M5Bus lines are consumed by onboard functionality of the combined system.

BSP should reserve at least these M5Bus-exposed lines for add-on use:

* `GPIO21` / `GPIO22` for the internal I2C bus shared with add-on devices
* `GPIO13` / `GPIO14` for UART2 socket
* `GPIO25` for RGB LED chain
* `GPIO34` for microphone data
* `GPIO0` for microphone clock and speaker LRCK shared role

Note: `GPIO32` / `GPIO33` (external I2C) pass through to Port A only and are not
used by any add-on onboard device.

## 31. Some M5Bus lines remain externally exposed through sockets on the add-on board

The add-on board itself further exposes:

* UART  — **Port C (blue)**, GPIO13/GPIO14
* I2C   — internal I2C pogo/socket, GPIO21/GPIO22
* GPIO36 / GPIO26 — **Port B (black)**, ADC/DAC
* power

These Grove ports (Port B, Port C) exist only on the M5GO-Bottom2 for AWS
expansion base, not on the standalone Core2 main board. The main board exposes
only **Port A (red)** external I2C (GPIO32/GPIO33).

So BSP docs should distinguish:

* signals used internally by add-on features
* signals further exported to user sockets

---

# Updated audio steering guidance

## 32. Audio must be treated as a shared board subsystem, not separate independent device drivers

With both schematics combined:

Main board:

* speaker amp:

  * `GPIO0`
  * `GPIO12`
  * `GPIO2`

Add-on board:

* microphone:

  * `GPIO0`
  * `GPIO34`

The shared pin is:

* `GPIO0`

BSP should:

* centralize audio pin ownership in one board-audio layer
* avoid separate autonomous initialization of speaker and microphone subsystems
* check whether the intended audio mode is:

  * playback only
  * capture only
  * coordinated shared-clock operation

## 33. Do not auto-generate full-duplex assumptions unless explicitly designed

Even though microphone and speaker both appear audio-related, the schematic evidence only proves:

* shared clock-related use on `GPIO0`
* separate data paths

BSP should not automatically assume a standard ESP32 full-duplex I2S template will be correct.
Instead:

* create a board-specific audio configuration
* treat clock routing and pin ownership as board-defined constraints

---

# Updated reserved-pin guidance

## 34. Add these pins to “board-claimed” status when add-on board is present

In addition to flash/PSRAM/console/power-management reservations, the combined board should treat these as committed:

* `GPIO0` — speaker LRCK + microphone clock + boot-sensitive
* `GPIO2` — speaker data
* `GPIO12` — speaker BCLK
* `GPIO13` — UART2 RX to add-on socket
* `GPIO14` — UART2 TX to add-on socket
* `GPIO25` — SK6812 RGB chain
* `GPIO21` — internal I2C SDA
* `GPIO22` — internal I2C SCL
* `GPIO32` — external (Port A) I2C SDA
* `GPIO33` — external (Port A) I2C SCL
* `GPIO34` — microphone data
* `GPIO39` — touch interrupt
* `GPIO5` — LCD CS
* `GPIO4` — SD CS
* `GPIO15` — LCD D/C
* `GPIO18` / `GPIO23` / `GPIO38` — shared SPI bus

These should not be presented as casually free GPIOs in a generated BSP.

---

# Updated probe and init ordering

## 35. Recommended bring-up order for the combined board

1. MCU core and boot stabilization
2. PMU / AXP192 initialization
3. required rail enable state
4. shared reset-domain handling
5. internal I2C bus on GPIO21/22, including core devices (AXP192, BM8563,
  touch) and add-on devices (MPU6886, ATECC608)
6. external (Port A) I2C bus on GPIO32/33
7. shared SPI devices:

   * LCD
   * SD card
8. board audio subsystem
9. RGB LED chain
10. exported sockets / optional external modules

This ordering helps avoid powering or probing devices before their bus/power dependencies are stable.

---

# Final anti-hallucination summary for BSP generation

## 36. Combined-board truths the BSP must honor

* LCD and SD share one SPI bus.
* Core internal I2C and external (Port A) I2C are different buses.
* MPU6886 is on the internal I2C bus, not the external bus.
* ATECC608 is on the internal I2C bus, not the external bus.
* Touch reset and LCD reset are shared.
* Speaker enable is PMU-controlled, not directly MCU-controlled.
* LCD backlight is not a simple direct ESP32 GPIO backlight.
* Vibration motor power is PMU rail-based.
* Microphone uses `GPIO34` data and `GPIO0` clock.
* Speaker and microphone share `GPIO0`, so audio pin ownership must be unified.
* RGB LEDs are a 10-pixel SK6812 chain on `GPIO25`, powered from 5V.
* UART2 on `GPIO13/14` is consumed by add-on socket wiring.
* Flash and PSRAM pins remain reserved and unavailable.

---

# BSP Notes: Espressif Library Gaps and Display/Touch/Microphone Tweaks

Compared to the Espressif community BSP for the plain Core2 (`espressif/esp-bsp`, `m5stack_core_2`), the following are relevant for the Core2 for AWS IoT Kit BSP.

## 37. `espressif/esp_codec_dev` is not used; it could offload speaker and microphone maintenance

The standard Core2 BSP declares `espressif/esp_codec_dev ~1.5` as a dependency and wraps both speaker and microphone through a unified `esp_codec_dev_handle_t` API. The Core2 for AWS BSP instead drives the NS4168 speaker amp and SPM1423HM4H-B microphone with raw `driver/i2s_std.h` and `driver/i2s_pdm.h` calls directly.

What `esp_codec_dev` provides over raw I2S:

* A consistent `esp_codec_dev_open` / `esp_codec_dev_write` / `esp_codec_dev_read` / `esp_codec_dev_close` interface
* Volume control via `esp_codec_dev_set_out_vol()` without manual sample scaling
* Works with `codec_if = NULL` for amps (like NS4168) that have no I2C control plane
* Decouples application code from the raw I2S channel handle lifecycle

To adopt it, add to `idf_component.yml`:

```yaml
espressif/esp_codec_dev:
  version: "~1.5"
```

The audio constraint that **speaker and microphone cannot be active simultaneously** (shared `GPIO0`) still applies regardless of whether `esp_codec_dev` is used — that is a hardware constraint, not a library one.

## 38. LVGL draw buffer size is an application budget, not a fixed constant

The standard Espressif Core2 BSP hardcodes a 50-line draw buffer. Do **not**
copy that number here. On this board the draw buffer height is
`CONFIG_CORE2FORAWS_LCD_DRAW_BUF_LINES` (default 20), because the right value
depends on how much internal DRAM the consuming application leaves free — and
these buffers must be contiguous DMA-capable internal DRAM.

Larger values reduce partial transfers per frame, but internal DRAM is the
scarce resource on this board and PSRAM is not a fallback. Treat any change as
a measured decision, not a default to raise. The authoritative policy and the
required verification procedure are in
[.claude/rules/memory-placement.md](memory-placement.md); do not restate or
contradict them here.

## 39. LVGL is pinned to core 1

[lib/display/core2foraws_display.c](../../lib/display/core2foraws_display.c)
sets `lvgl_cfg.task_affinity = 1`, matching the standard BSP. This keeps the
DMA-driven display flush off core 0, where the Wi-Fi stack and the IDF event
loop run by default, avoiding scheduling contention that shows up as dropped
frames. Keep this pinning if camera or other CPU-intensive work is added.

## 40. `swap_bytes = true` is already correct for LVGL 9 + SPI ILI9341

The Core2 for AWS display driver already sets `.flags.swap_bytes = true` in `lvgl_port_display_cfg_t`. The standard BSP conditionally sets `swap_bytes = (BSP_LCD_BIGENDIAN ? true : false)`. Both result in `true` for the ILI9341 over SPI on this platform. No change needed — this is confirmation the current setting is correct.