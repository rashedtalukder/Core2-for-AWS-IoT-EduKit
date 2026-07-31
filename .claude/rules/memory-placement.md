---
description: Where BSP buffers must live (internal DRAM vs. external PSRAM). The LVGL display draw buffers and all DMA buffers stay in internal RAM; never move them to PSRAM.
applyTo: "**"
---

# Rule: Internal DRAM vs. external PSRAM buffer placement

This board has 8 MB of external PSRAM (ESPSRAM64H) and a much smaller pool of
internal DRAM. PSRAM is plentiful but **cannot be reached by the ESP32 DMA
engines** and is slower (cached) for the CPU. Internal DRAM is scarce but DMA-
capable and fast. Placing the wrong buffer in the wrong pool causes either
hard failures (DMA cannot touch PSRAM) or framerate/latency regressions that
manifest as UI hangs and crashes.

## 1. Hard requirement — LVGL display draw buffers stay in internal DRAM

The LVGL draw buffers for the ILI9342C display **must** be allocated in
internal, DMA-capable RAM. In [lib/display/core2foraws_display.c](../../lib/display/core2foraws_display.c)
the `lvgl_port_display_cfg_t` flags must remain:

```c
.flags = {
    .buff_dma    = true,    /* DMA-capable internal RAM */
    .buff_spiram = false,   /* never PSRAM */
    .swap_bytes  = true,
},
```

Do **not** set `buff_spiram = true` for the display. Rationale, confirmed
empirically on this board:

- The 40 MHz SPI panel transfer is DMA-driven; PSRAM is not DMA-addressable,
  so a PSRAM draw buffer forces slow byte copies and stalls the LVGL flush.
- Display writes must be fast. A slow flush path blocks the LVGL task long
  enough to cause downstream hangs and crashes.
- Moving the draw buffers from PSRAM into internal DRAM **increased** the
  framerate even after `LCD_DRAW_BUF_LINES` was reduced to free DRAM
  (commits `87cb344`, `aeaeb19`).

`LCD_DRAW_BUF_LINES` is set from `CONFIG_CORE2FORAWS_LCD_DRAW_BUF_LINES`
(default 20, range 10–60). Two buffers of `lines * 320 * 2` bytes are
allocated, each needing a single **contiguous** DMA-capable block:

| Lines | Per buffer | Total |
| --- | --- | --- |
| 10 | 6,400 B | 12,800 B |
| 20 (default) | 12,800 B | 25,600 B |
| 40 | 25,600 B | 51,200 B |

This is a **budget, not a fixed value**. The correct setting depends on what
the consuming application does with internal DRAM, which the BSP cannot know,
so it is an application-level Kconfig choice rather than a BSP constant. Do not
change the default without following the verification procedure below, and
never "solve" a DRAM shortage by relocating these buffers to PSRAM.

### Verification procedure for any change to the draw buffer height

1. Build and boot with the new value, with Wi-Fi associated and the display
   active — the DRAM low-water mark is reached after the network is up, not at
   `core2foraws_init()`.
2. Call `core2foraws_common_heap_report()` at both points and record
  `dma_largest_block`, `dma_free`, and `dma_minimum_free`. Contiguity is what
  fails first; current total free size alone is not sufficient evidence.
3. Confirm `dma_largest_block` still clears one buffer with margin.
4. Record the measured framerate alongside the heap numbers in the commit
   message. Commit `aeaeb19` reduced this value on measured evidence; any
   increase needs measurements of equal weight to overturn it.

`core2foraws_display_init()` performs a pre-flight
`heap_caps_get_largest_free_block( MALLOC_CAP_DMA )` check and fails with
`ESP_ERR_NO_MEM` and an actionable log rather than allocating blindly. Keep
that check in place.

Prefer reclaiming DRAM elsewhere over shrinking these buffers — see the
"Internal DRAM budget" section of [README.md](../../README.md) for the
application-level levers (`CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP`,
`CONFIG_CORE2FORAWS_WIFI_RELEASE_BLE_WHEN_PROVISIONED`, Wi-Fi buffer tuning).

## 2. Other buffers that must also stay in internal DRAM (not optional)

These are DMA- or timing-critical and **cannot** move to PSRAM:

- **Audio I2S DMA buffers** — `lib/audio` drives the NS4168 speaker and
  SPM1423 microphone through I2S DMA. I2S DMA cannot address PSRAM.
- **SD-card / shared-SPI DMA buffers** — `lib/sd` and the shared SPI bus use
  SPI-master DMA, which requires internal DMA-capable RAM.
- **SK6812 RGB LED TX buffer** — `lib/rgb_led` feeds the RMT peripheral with
  tight SK6812 timing; keep its buffer internal.

## 3. What *may* safely live in PSRAM

PSRAM is the right home for large, CPU-only, latency-tolerant, non-DMA data:

- **Application scratch / payload buffers** the BSP hands back to the user —
  e.g. microphone capture copies, UART payloads, crypto serial/public-key
  strings. The public headers already demonstrate
  `heap_caps_malloc( ..., MALLOC_CAP_SPIRAM )` for these and that guidance
  should stay.
- **Bulk, infrequently-touched application state** that never participates in
  a DMA transfer.

When a buffer will be the *source or destination of a DMA transfer*, it must be
internal (`MALLOC_CAP_DMA` / `MALLOC_CAP_INTERNAL`), regardless of size.

## 3a. Do NOT move the LVGL object heap (`LV_MEM`) to PSRAM

The LVGL object/widget heap — the `LV_MEM` pool holding widgets, styles,
animations, and event descriptors (distinct from the draw buffers) — must stay
in internal DRAM. Relocating it to PSRAM was considered and **rejected**.
Rationale:

- LVGL walks its object tree on every refresh; the random pointer-chasing into
  cached PSRAM causes cache thrashing and raises per-frame CPU cost, even with
  the draw buffers kept internal. This reintroduces the latency/hang failure
  mode the draw-buffer fix eliminated, via a subtler path.
- LVGL may allocate image/canvas descriptors from the same pool; if any such
  allocation becomes a DMA source/destination it cannot be reached by the
  SPI/I2S DMA engines — a hard failure.
- A split heap plus DMA-driven internal draw buffers widens the surface for
  cache write-back ordering bugs and unpredictable PSRAM bus contention.

Do not set an `LV_MEM`/`mem_custom` pool in PSRAM for this board.

## 4. Decision checklist before choosing a pool

1. Does any DMA engine (SPI, I2S, RMT) read/write this buffer? → internal DRAM,
   no exceptions.
2. Is it on a per-frame / per-sample latency-critical path? → internal DRAM.
3. Is it large, CPU-only, and latency-tolerant? → PSRAM is fine and preferred
   to conserve scarce DRAM.

Keep this rule, [.claude/design.md](../design.md) §5, and the display driver in
sync if any of the above changes.
