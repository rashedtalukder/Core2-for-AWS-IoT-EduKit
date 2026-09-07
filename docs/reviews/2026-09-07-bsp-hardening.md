# BSP hardening review, 2026-09-07

This is a prioritized source review and measured hardening pass, not a proof
that the BSP is bug-free or a production qualification. Pin assignments, MCU
supply setpoint, DMA buffer placement, and buffer height were not changed.
Peripheral power state is temporarily exercised and restored during testing.
The supplied board constraints guided the review.

## Corrected

| Area | Defect addressed | Evidence |
| --- | --- | --- |
| Startup | Downstream initialization after PMU failure | Host fault injection |
| I2C | Close/removal racing transfers and registration; lost final reference on failed removal; signed flag shift | Host deterministic interleavings, retry tests, UBSan; board stale-handle check |
| Audio | Milliseconds incorrectly converted to ticks for I2S; short writes reported as success | Installed SDK signatures, 100 Hz host tests, 20 board mode cycles |
| Display | Unsynchronized flush on SPI timeout; missing completion on submission error; teardown after DMA barrier timeout; callback installation race | Full target build and board contention/teardown tests |
| SD | Ignored close errors and abandoned files on SPI timeout; stale read output on early failure | Host fault injection plus real-card SD/display/NVS stress and remount readback |
| RTC | 32-bit ESP32 epoch intermediate overflow and extreme-year addition overflow | Host epoch/year regressions and target build |
| Factory build | Untracked legacy screenshot files shadowed the maintained component | Main CMake excludes only the legacy source and prioritizes the component header; user files preserved |
| Wi-Fi | Initialization and provisioning getter racing teardown | Shared lifecycle lock; 20 concurrent init/deinit cycles and 100 getter calls racing teardown on hardware |
| First use | Higher-priority spin wait can starve initialization | One-tick blocking waits across common/I2C/audio/SD/RGB/expansion/Wi-Fi; host regressions for I2C/audio/SD |
| MCU power | Direct rail disable or arbitrary supply setting could strand the processor | Guard across rail and raw-register APIs; host negative tests produce no writes; normal REG32H shutdown remains supported |

## Current dependencies

Verified against the official registries on 2026-09-07:

- PlatformIO Espressif32 **7.1.2**, bundling ESP-IDF **6.1.0**.
- Updated esp_lvgl_port **2.9.0**, esp_lcd_touch_ft5x06 **1.1.1**, and
  esp_lcd_ili9341 **2.1.0**. Other direct components were already latest:
  esp-cryptoauthlib 3.7.9~2, qrcode 0.2.0, network_provisioning 1.2.4,
  LVGL 9.5.0, and esp_lcd_touch 1.2.1.
- Transitive cJSON is current at 1.7.19~2. cmake_utilities remains at 0.5.3
  because the latest LCD component explicitly requires `0.*`; 1.1.1 is outside
  that constraint. No third-party manifest was patched to force incompatibility.
- The component manager regenerated the local dependency lock and managed cache.
  Both remain ignored by Git per the existing project policy; direct pins are
  retained in the BSP manifest. No SDK/library source patches were required.

## Upgraded hardware run

### Lifecycle follow-up

- Owner reports successful manual physical power/reset-button testing on
  2026-09-07. This is user-observed evidence, not a host-observed software
  `power_off()`/PEK cycle; the software shutdown path still has host coverage.
- Fixed audio/RGB handle loss on teardown and failed-startup cleanup. Each step
  is retryable, with new host fault-injection tests checking ownership, blocked
  reuse, and no premature GPIO resets or in-flight buffer changes.
- Fixed paired I2C/UART release and begin ordering. Host routing models cover
  deletion failures, reopen, and failed UART pin setup followed by retry.
- Fixed motion register/cache-update and scaled-read atomicity, with host tests
  checking ownership until output assignment and cache preservation on failures.
- **3,483 smoke checks passed**, plus checked worker results, on ESP-IDF 6.1.0.
  Ten RGB and paired I2C/UART teardown/reopen cycles, 20 audio cycles, three
  display reinitializations, 100 concurrent motion-range updates, and the Wi-Fi
  lifecycle/getter races passed. No panic/watchdog/brownout/backtrace markers.
- Twenty SD 16 KiB write/read/NVS commit cycles completed in **9,972,693 us**
  alongside **207 redraw requests**. Remount data verification and the injected
  SPI timeout passed; test data was cleaned. Timing varies with SD behavior and
  scheduling; this is not evidence of a speedup or regression against prior runs.
- The extended 1,000-iteration I2C loop included gyro and periodic RTC reads and
  concurrent range updates; it took **10,105,821 us**, including tick delays.
  Internal free remained **85,155 bytes** across the loop. DMA free **71,139**,
  minimum **66,300**, largest block **69,632** bytes; heap integrity passed.
- The earlier RTC NACK was not reproduced in three diagnostic factory boots
  or the extended smoke run. A temporary payload-free transfer trace was then
  removed. No retry/suppression was added and no cause established; instrumentation
  can affect timing, so the intermittent RTC finding remains open.
- Wi-Fi again did not associate within the test window. Secure-element tests
  remained read-only, and the MCU rail setpoint/startup configuration was untouched.
- The final uninstrumented factory image was rebuilt, flashed to the active
  `0xA0000` application slot, and hash-verified. It reached ready at **4.436 s**
  on ESP-IDF 6.1.0, with **zero BSP errors, RTC failures, or crash markers** in
  a 20-second capture. Startup internal free was 162,467 bytes and DMA free
  120,751 bytes (largest block 110,592). This clean sample does not establish
  the cause or permanent resolution of the earlier intermittent RTC NACK.

### Previous upgraded run

- Full ESP-IDF 6.1.0 factory and smoke builds succeeded. Host C11 tests passed
  with AddressSanitizer and UndefinedBehaviorSanitizer.
- **2,281 smoke assertions passed**, plus explicitly checked worker results.
  No panic, assertion, watchdog, brownout, or backtrace markers were observed.
- Backlight 0/25/100%, green LED, and a short vibration pulse passed register
  readback checks. Peripheral state was restored; DCDC1 remained enabled at
  **3350 mV**. This tests API/register behavior, not calibrated optical output.
- **20 SD 16 KiB write/read + NVS commit/reopen/readback cycles** ran alongside
  **67 display redraw requests** in **4,591,525 us**. SD contents matched after
  unmount/remount. The expected SPI lock timeout returned cleanly. The test
  file `/sd_card/bsp-review-test.txt` and `bsp_review` NVS keys were cleaned up.
  This is not a panel-pixel measurement or power-interruption durability test.
- 20 microphone/speaker mode cycles, three display deinit/reinit cycles, and
  forced SPI contention/teardown recovery passed.
- 20 concurrent Wi-Fi init/deinit cycles and 100 provisioning reads racing
  shutdown passed. Wi-Fi did not associate within 15 seconds, so connected-radio
  throughput and memory pressure are still unverified.
- 1,000 shared-I2C iterations took 10,096,425 us, including a tick delay each.
  Heap integrity passed; internal free stayed at **88,287 bytes** across that
  loop. DMA free: **71,619**; minimum: **70,752**; largest block: **69,632** bytes.
- The final restoration of normal `core2foraws_power_off()` followed this run.
  Host tests prove it changes only REG32H bit 7, not MCU rail configuration;
  target compilation passed. An actual shutdown followed by physical power-key
  restart has not been performed. Datasheet/schematic rationale and the safety
  boundary are documented in [design document](../design.md), section 6.1.
- No secure-element slots, keys, zone locks, counters, or startup defaults were
  modified. Only read operations were used for secure-element tests.
- The final factory image, with normal software shutdown restored, was flashed
  to `0xA0000` and hash-verified. It reported ESP-IDF 6.1.0 and reached ready at
  4.426 seconds. A 15-second capture contained no crash markers, but one RTC
  read returned `ESP_ERR_INVALID_RESPONSE` shortly after startup; a second boot
  reproduced that single NACK. This remains a factory-workload investigation,
  not a clean-runtime signoff. The stress-image assertions above still passed.
- C/C++ editor diagnostics retained a `mach-o/getsect.h` include warning for
  the power source despite a successful Xtensa build. PlatformIO metadata and
  compilation commands were regenerated; local IntelliSense was pointed at the
  target compilation database. Editor reindex/reload may still be required.

## Initial 6.0.1 verification

- Native C11 host suite passed with AddressSanitizer and UndefinedBehaviorSanitizer.
- Factory firmware and dedicated board-test image compiled with PlatformIO
  espressif32 7.0.1 / ESP-IDF 6.0.1. ESP-IDF 5.3 was not rebuilt in this pass.
- ESP32-D0WDQ6-V3 board at `/dev/cu.usbserial-02036BEC`: **2,193 checks passed**.
  Repeated startup, 20 audio mode cycles, three display reinitializations,
  deliberate SPI contention and teardown-timeout recovery all completed.
- 1,000 motion/PMU read iterations, interleaved with secure-element reads and
  redraws, took **10,096,442 us**, including one RTOS tick delay per iteration.
  This is a contention smoke workload, not peak bus throughput or a before/after
  performance benchmark.
- Heap integrity passed. Final internal free: **90,603 bytes**; DMA free:
  **72,615 bytes**; DMA minimum: **71,767 bytes**; largest DMA block:
  **69,632 bytes**. Internal free changed by **-8 bytes** over the measured loop.
  These are smoke-image figures, not the factory UI's full memory budget.
- No panic, assertion, watchdog, brownout, or backtrace markers were observed.
- Wi-Fi start returned success, but **association was not established within
  15 seconds**. These figures do not validate the connected-radio memory budget.
- Final normal factory image was rebuilt after the RTC and API edge-case fixes,
  flashed only to the verified active app slot at `0xA0000`, and hash-verified.
  The 20-second boot capture reached `Factory firmware ready` at 4.421 seconds
  with no BSP/display errors or crash markers. Before the full UI started,
  internal free was 163,623 bytes, DMA free 120,815 bytes, and largest DMA block
  110,592 bytes. These are startup figures, not connected-Wi-Fi low-water marks.
- The 2,193-check smoke run preceded the final RTC arithmetic, uninitialized-I2C
  return-code, and empty-SD-read-output changes. Those final changes passed the
  complete host suite, final target build, and factory boot; the smoke image was
  not reflashed a second time.

## Remaining findings and limits

1. **Intermittent RTC startup NACK:** earlier factory boots reported one
  `ESP_ERR_INVALID_RESPONSE`; the trace boots and current smoke run did not.
  Root cause is unproven, and error handling has deliberately not been hidden.
2. **Connected-radio validation:** Wi-Fi association, reconnect, and provisioning
  flows require an available network and remain unqualified by these runs.
3. **Coverage is not exhaustive:** analog expansion cleanup, raw/direct hardware
  access, and other modules' partial-init failures are not covered by the new
  audio/RGB/I2C/UART lifecycle tests. Tests use explicit SDK mock contracts, not
  an exhaustive hardware-fault model.

Physical touch accuracy, audible playback, RGB optical output, interrupted-power
SD durability, external accessories, Wi-Fi/BLE reconnect/provisioning, power-loss
recovery, calibration, and a long-duration soak were not qualified. No secure-
element provisioning or key change was performed. Tests and rerun
instructions are in [tests/README.md](../../tests/README.md).