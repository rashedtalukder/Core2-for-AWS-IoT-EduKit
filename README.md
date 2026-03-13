# M5Stack Core2 for AWS IoT Kit Board Support Package (BSP)
This repository contains the drivers specific to the [M5Stack Core2 for AWS IoT Kit](https://m5stack.com/products/m5stack-core2-esp32-iot-development-kit-for-aws-iot-edukit) reference Hardware. This BSP is used in the microcontroller tutorials presented in the [AWS IoT Kit](https://aws.amazon.com/iot/edukit) program.

## Cloning
To clone using HTTPS:
```
git clone -b BSP-dev https://github.com/m5stack/Core2-for-AWS-IoT-Kit.git
```
Using SSH:
```
git clone -b BSP-dev git@github.com:m5stack/Core2-for-AWS-IoT-Kit.git
```

> **Note:** This repository no longer uses Git submodules. All third-party driver code (AXP192 PMU, MPU6886 IMU) is included directly in the BSP source tree.

> **Windows users:** If the repository or any project consuming it contains symbolic links, set `core.symlinks true` (`git config --global core.symlinks true`) and either enable [Developer Mode](https://docs.microsoft.com/en-us/windows/apps/get-started/enable-your-device-for-development) or run git commands from an elevated console.

## Usage
It is recommended to use the [project template](https://github.com/m5stack/Project_Template-Core2_for_AWS) instead of the BSP directly. The GitHub project template repository contains all the necessary external dependencies and configuration to work properly with the Core2 for AWS IoT Kit. The BSP is tested for compatibility with [ESP-IDF v5.3](https://www.espressif.com/en/products/sdks/esp-idf) or [PlatformIO](https://github.com/platformio/platform-espressif32) espressif32 v6.9+. Please ensure that your installation of PlatformIO is updated to the latest version of PlatformIO Core using the command `pio upgrade` from the PlatformIO terminal window. Follow the [AWS IoT Kit — Getting Started](https://aws-iot-kit-docs.m5stack.com/en/getting-started/kit) tutorial for instructions on how to setup your environment.

We also have code examples, drivers, or content available in other frameworks:
- [Arduino](https://github.com/m5stack/aws-iot-kit-examples/tree/main/Basic_Arduino)
- [UIFlow](https://docs.m5stack.com/en/quick_start/core2_for_aws/uiflow)
- [MicroPython](https://github.com/m5stack/Core2forAWS-MicroPython)

## Support
To get support with AWS IoT Kit, post your question in the [content repo's discussions](https://github.com/m5stack/aws-iot-kit-tutorials/discussions).
For issues with the AWS IoT Kit this repo, please [submit an issue](https://github.com/m5stack/Core2-for-AWS-IoT-Kit/issues) to this repository.