# M5Stack Core2 for AWS IoT Kit Board Support Package (BSP)

This repository contains the drivers specific to the [M5Stack Core2 for AWS IoT Kit](https://m5stack.com/products/m5stack-core2-esp32-iot-development-kit-for-aws-iot-edukit) reference Hardware. This BSP is used in the microcontroller tutorials presented in the [AWS IoT Kit](https://aws.amazon.com/iot/edukit) program.

## Cloning

To clone using HTTPS:

```shell
git clone -b BSP-dev https://github.com/m5stack/Core2-for-AWS-IoT-Kit.git
```

Using SSH:

```shell
git clone -b BSP-dev git@github.com:m5stack/Core2-for-AWS-IoT-Kit.git
```

**Note:** This repository no longer uses Git submodules. All third-party driver code (AXP192 PMU, MPU6886 IMU) is included directly in the BSP source tree.

**Windows users:** If the repository or any project consuming it contains symbolic links, set `core.symlinks true` (`git config --global core.symlinks true`) and either enable [Developer Mode](https://docs.microsoft.com/en-us/windows/apps/get-started/enable-your-device-for-development) or run git commands from an elevated console.

## Usage

It is recommended to use the [project template](https://github.com/m5stack/Project_Template-Core2_for_AWS) instead of the BSP directly. The project template contains the application configuration and managed dependencies required by the Core2 for AWS IoT Kit.

This repository is an ESP-IDF component, not a standalone application. The component is built and tested with [ESP-IDF v5.3](https://www.espressif.com/en/products/sdks/esp-idf). [PlatformIO](https://github.com/platformio/platform-espressif32) `espressif32` v6.9+ is supported by consuming applications; this component does not contain its own `platformio.ini`. Follow the [AWS IoT Kit — Getting Started](https://aws-iot-kit-docs.m5stack.com/en/getting-started/kit) tutorial for environment setup.

`core2foraws_init()` attempts every enabled automatic module after the internal I2C foundation is available and returns `ESP_FAIL` if one or more modules fail. Applications that need module-specific recovery can initialize those modules independently. Wi-Fi initialization returns NVS and network errors without erasing the default NVS partition; `core2foraws_wifi_reset()` clears only persistent Wi-Fi configuration.

The master `SOFTWARE_BSP_SUPPORT` Kconfig option controls the common layer and all hardware modules. When it is disabled, common APIs are not compiled or exposed and `core2foraws_init()` is a successful no-op.

The internal I2C bus is a permanent, recursively locked board resource. External Port A I2C can host multiple managed devices and can be closed/reopened independently. Display and SD share one common-owned SPI2 bus; SD transfers are chunked so large file operations do not monopolize display refresh.

API safety notes for this revision:

- Use `core2foraws_display_touch_data_get()` instead of reading the raw touch handle; it participates in internal-I2C serialization.
- `core2foraws_expports_uart_read()` requires the destination buffer capacity before the output byte count.
- Audio I/O is bounded by `AUDIO_IO_TIMEOUT_MS` and serialized against speaker/microphone disable.
- `core2foraws_display_deinit()` releases display, touch, and LVGL resources while leaving shared SPI2 available to SD.

We also have code examples, drivers, or content available in other frameworks:

- [Arduino](https://github.com/m5stack/aws-iot-kit-examples/tree/main/Basic_Arduino)
- [UIFlow](https://docs.m5stack.com/en/quick_start/core2_for_aws/uiflow)
- [MicroPython](https://github.com/m5stack/Core2forAWS-MicroPython)

## Support

To get support with AWS IoT Kit, post your question in the [content repo's discussions](https://github.com/m5stack/aws-iot-kit-tutorials/discussions).
For issues with the AWS IoT Kit this repo, please [submit an issue](https://github.com/m5stack/Core2-for-AWS-IoT-Kit/issues) to this repository.
