---
description: Treat the schematic files as the wiring source of truth, and read the relevant peripheral datasheet before any driver-level change.
applyTo: "**"
---

# Rule: Schematic is truth; read the datasheet before touching a driver

This BSP is wiring-driven. Two classes of reference documents govern any
hardware-facing change. Consult them **before** writing or modifying code, not
after.

## 1. Wiring / implementation source of truth

When deciding *how something is connected* — pin assignments, which bus a device
is on, shared lines, power rails, M5Bus routing, connector pinouts — the
authoritative sources are, in order:

1. [datasheet/schema.yml](../../datasheet/schema.yml) — the machine-readable,
   net-level wiring schema (core board + add-on board joined by M5Bus). This is
   the **primary** source of truth for nets, pins, buses, and inter-board links.
2. [datasheet/schematic.md](../../datasheet/schematic.md) — the human-readable
   hardware reference derived from the same schematics.
3. [.claude/rules/pinmap.md](pinmap.md) and [.claude/rules/board.md](board.md) —
   the distilled, steering-oriented summaries.

If any of these disagree with code, **the schematic files win**. If the steering
rules (`pinmap.md` / `board.md`) disagree with [schema.yml](../../datasheet/schema.yml)
or [schematic.md](../../datasheet/schematic.md), treat that as a defect: stop,
flag it to the user, and reconcile rather than guessing.

Do not invent pin assignments, bus membership, or reset/power ownership from
generic ESP32 devkit assumptions. Derive them from the schematic sources.

## 2. Read the peripheral datasheet before driver-level changes

Before modifying or adding code that programs a specific chip (register writes,
init sequences, timing, mode configuration, addresses), **first read that
peripheral's datasheet markdown** for context. Driver-level means anything that
touches chip registers, init/wake/reset timing, or the device's electrical
behavior — not application glue.

Peripheral → datasheet map:

| Peripheral | Module | Datasheet to read first |
| --- | --- | --- |
| AXP192 PMU | `lib/power` | [lib/power/datasheet/AXP192.md](../../lib/power/datasheet/AXP192.md) |
| SY7088 boost | `lib/power` | [lib/power/datasheet/SY7088.md](../../lib/power/datasheet/SY7088.md) |
| MPU6886 IMU | `lib/motion` | [lib/motion/datasheet/MPU-6886.md](../../lib/motion/datasheet/MPU-6886.md) |
| BM8563 RTC | `lib/rtc` | [lib/rtc/datasheet/BM8563.md](../../lib/rtc/datasheet/BM8563.md) |
| FT6336 touch | `lib/button`, `lib/display` | [lib/button/datasheet/FT6336.md](../../lib/button/datasheet/FT6336.md) |
| ILI9342 LCD | `lib/display` | [lib/display/datasheet/ILI9342.md](../../lib/display/datasheet/ILI9342.md) |
| NS4168 speaker amp | `lib/audio` | [lib/audio/datasheet/NS4168.md](../../lib/audio/datasheet/NS4168.md) |
| SPM1423 microphone | `lib/audio` | [lib/audio/datasheet/SPM1423.md](../../lib/audio/datasheet/SPM1423.md) |

Notes:

- If a peripheral has **no** datasheet markdown in its module (e.g. the
  ATECC608 secure element, SK6812 LEDs, the CP2104 USB-UART), there is no local
  datasheet to read; rely on the schematic sources above and the existing
  third-party driver/library, and say so rather than fabricating register
  details.
- When a change spans more than one chip (for example the shared-GPIO0 audio
  path), read **all** the involved datasheets first (NS4168 *and* SPM1423).

## 3. Workflow for any hardware-facing change

1. Identify the affected peripheral(s) and bus/pins.
2. Read [schema.yml](../../datasheet/schema.yml) /
   [schematic.md](../../datasheet/schematic.md) to confirm the wiring.
3. Read the relevant peripheral datasheet markdown (table above).
4. Make the code change consistent with both.
5. Per [.claude/rules/design-doc.md](design-doc.md), update
   [.claude/design.md](../design.md) if the change affects anything the design
   doc describes.

## What not to do

- Do not change pin numbers, bus membership, or power/reset ownership without a
  citation from the schematic sources.
- Do not write or edit register-level driver code without first reading that
  chip's datasheet markdown when one exists.
- Do not let [pinmap.md](pinmap.md) / [board.md](board.md) silently diverge from
  [schema.yml](../../datasheet/schema.yml); reconcile and flag instead.
