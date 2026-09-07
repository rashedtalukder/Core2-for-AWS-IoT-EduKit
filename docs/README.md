# BSP engineering documentation

- [Design and hardware safety](design.md): architecture, ownership, power policy,
  memory placement, and supported build configuration.
- [Hardening review, 2026-09-07](reviews/2026-09-07-bsp-hardening.md): recorded
  fixes, test evidence, and remaining qualification limits.
- [Tests](../tests/README.md): native regressions and the opt-in hardware app.
- [Hardware schema](../datasheet/schema.yml) and
  [schematic reference](../datasheet/schematic.md): board wiring evidence.

Keep reusable design guidance here; dated measurements belong under `reviews/`.
Chip-specific source datasheets stay with their owning module under `lib/`.