---
description: Keep the BSP design document in sync as the BSP code evolves.
applyTo: "**"
---

# Rule: Keep the design doc current as the BSP iterates

The canonical design document for this BSP lives at
[docs/design.md](../../docs/design.md). It must stay accurate as the code changes.

## When this rule applies

Whenever you make a change that affects any of the following, you MUST review
[docs/design.md](../../docs/design.md) in the same task and update it if it is now
stale:

- **Init flow or ordering** — `core2foraws_init()` in [core2foraws.c](../../core2foraws.c)
  (adding/removing a subsystem, changing bring-up order, changing what is
  fatal vs. non-fatal, changing what is init-on-demand).
- **Public API** — any function signature, enum, or macro in a
  `lib/*/include/*.h` header, or in [include/core2foraws.h](../../include/core2foraws.h).
- **Modules** — adding, removing, renaming, or merging a module under `lib/`
  (update the module catalog table in §7).
- **Shared-resource handling** — the I2C bus/mutex model, the shared SPI
  semaphore, the audio (GPIO0/I2S) mutual-exclusion, or any new lock.
  Keep §5 and the thread-safety table in §8.3 correct.
- **Power/AXP192** — rail assignments, PMU-controlled signals, or power
  bring-up order (§6).
- **Build configuration** — ESP-IDF version target, Kconfig flags, CMake
  `REQUIRES`/`PRIV_REQUIRES`, or managed dependencies in
  [idf_component.yml](../../idf_component.yml) (§9).
- **Hardware constraints / pin usage** — keep §10 consistent with
  [.claude/rules/pinmap.md](pinmap.md) and [.claude/rules/board.md](board.md).

## How to update

1. Make the code change first.
2. Re-read the affected section(s) of [docs/design.md](../../docs/design.md).
3. Edit only the parts that are now inaccurate. Preserve the document's
   newcomer-friendly tone and its two stated goals: **stability** and
   **ease of understanding**.
4. Keep diagrams (Mermaid), tables, and file links accurate. Only link to
   files that exist; use workspace-relative paths.
5. Do not duplicate the authoritative pin/board facts — reference
   [.claude/rules/pinmap.md](pinmap.md) and [.claude/rules/board.md](board.md)
   instead of restating them.

## What not to do

- Do not let the design doc and the code diverge silently.
- Do not document features, modules, or APIs that do not exist in the code.
- Do not expand the doc into per-function API reference; it is a design
  overview, not generated API docs.
