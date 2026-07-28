/*
 * Core2 for AWS IoT Kit BSP v2.1.0
 * Copyright (C) 2026 Rashed Talukder.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file core2foraws_common.h
 * @brief Core2 for AWS IoT Kit helper library used across BSP drivers
 */

#ifndef _CORE2FORAWS_COMMON_H_
#define _CORE2FORAWS_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_err.h>

#include "core2foraws_i2c.h"

/**
 * @brief The port used by the internal I2C peripherals.
 */
/* @[declare_core2foraws_common_i2c_internal] */
#define COMMON_I2C_INTERNAL CORE2FORAWS_I2C_INTERNAL
/* @[declare_core2foraws_common_i2c_internal] */

/**
 * @brief The port used by the external I2C peripherals.
 */
/* @[declare_core2foraws_common_i2c_external] */
#define COMMON_I2C_EXTERNAL CORE2FORAWS_I2C_EXTERNAL
/* @[declare_core2foraws_common_i2c_external] */

/**
 * @brief The port used by the I2S peripherals (speaker & microphone).
 */
/* @[declare_core2foraws_common_i2s_internal] */
#define COMMON_I2S_INTERNAL 0
/* @[declare_core2foraws_common_i2s_internal] */

/**
 * @brief FreeRTOS binary semaphore used to serialize display and SD card SPI
 * transfers.
 * 
 * The display and SD card share a SPI bus.
 *
 * @note The BSP display and SD modules take this semaphore automatically.
 * Application code should use the LVGL port lock for LVGL object access and
 * only take this semaphore directly when adding another device or raw
 * transaction to the shared SPI bus.
 */
/* @[declare_core2foraws_common_spi_semaphore] */
extern SemaphoreHandle_t core2foraws_common_spi_semaphore;
/* @[declare_core2foraws_common_spi_semaphore] */

/**
 * @brief Creates the shared display/SD SPI semaphore if needed.
 *
 * This function is idempotent. The semaphore is binary rather than a mutex so
 * an asynchronous LCD transfer can take it from the LVGL task and release it
 * from the SPI completion callback.
 *
 * @return
 *  - ESP_OK         : Success or already initialized
 *  - ESP_ERR_NO_MEM : Semaphore allocation failed
 */
/* @[declare_core2foraws_common_spi_semaphore_init] */
esp_err_t core2foraws_common_spi_semaphore_init( void );
/* @[declare_core2foraws_common_spi_semaphore_init] */

/**
 * @brief Initializes the board's shared SPI2 bus for the LCD and SD card.
 *
 * The bus has fixed board wiring and is initialized once for the BSP lifetime.
 * The function is thread-safe and idempotent, so display and SD can initialize
 * in either order.
 *
 * @return ESP_OK on success, otherwise an ESP-IDF SPI initialization error.
 */
/* @[declare_core2foraws_common_spi_bus_init] */
esp_err_t core2foraws_common_spi_bus_init( void );
/* @[declare_core2foraws_common_spi_bus_init] */

/**
 * @brief Function used to standardize error returns.
 * 
 * This is a helper function used to return any non-zero error codes as
 * ESP_FAIL, and zero value as ESP_OK.
 * 
 * @param[in] error_code The error code for a library.
 * @return [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros). 0 or `ESP_OK` if successful.
 */
/* @[declare_core2foraws_common_error] */
esp_err_t core2foraws_common_error( int32_t error_code );
/* @[declare_core2foraws_common_error] */

/**
 * @brief Report the minimum free stack ("high-water mark") of a task.
 *
 * Gets, and logs at info level, the smallest amount of unused stack the task
 * has ever had since it started, expressed in bytes. Use it to size
 * FreeRTOS task stacks: run your real workload, read the watermark, and keep
 * a safety margin above the peak usage (allocation minus reported free).
 *
 * The BSP creates two long-lived tasks whose stacks you may want to measure:
 * the virtual-button poll task (named `"buttonPress"`) and the LVGL
 * render/flush task (named `"LVGL task"`). Look either up with
 * [`xTaskGetHandle()`](https://www.freertos.org/a00021.html#xTaskGetHandle)
 * or pass `NULL` to measure the calling task.
 *
 * **Example:**
 *
 * Log how much stack headroom the LVGL task still has.
 * @code{c}
 *  #include "core2foraws.h"
 *
 *  core2foraws_init();
 *  TaskHandle_t lvgl = xTaskGetHandle( "LVGL task" );
 *  size_t free_stack_bytes;
 *  esp_err_t err = core2foraws_common_task_stack_watermark(
 *      "APP", lvgl, &free_stack_bytes );
 * @endcode
 *
 * @param[in] tag  Log tag to print under. If `NULL`, a default tag is used.
 * @param[in] task Handle of the task to inspect, or `NULL` for the caller.
 * @param[out] watermark_bytes Minimum free stack in bytes since the task
 * started.
 * @return
 *  - ESP_OK              : Success
 *  - ESP_ERR_INVALID_ARG : @p watermark_bytes is `NULL`
 */
/* @[declare_core2foraws_common_task_stack_watermark] */
esp_err_t core2foraws_common_task_stack_watermark( const char *tag,
                                                   TaskHandle_t task,
                                                   size_t *watermark_bytes );
/* @[declare_core2foraws_common_task_stack_watermark] */

#ifdef __cplusplus
}
#endif
#endif
