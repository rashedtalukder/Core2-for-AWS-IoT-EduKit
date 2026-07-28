/*
 * Core2 for AWS IoT Kit BSP v2.1.0
 * Copyright (C) 2026 Rashed Talukder.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file core2foraws_display.h
 * @brief Core2 for AWS IoT Kit display hardware driver APIs
 */

#ifndef _CORE2FORAWS_DISPLAY_H_
#define _CORE2FORAWS_DISPLAY_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <esp_err.h>
#include <esp_lcd_touch.h>
#include <esp_lvgl_port.h>
#include <stdint.h>

#include "lvgl.h"

  /**
   * @brief Pointer to the registered LVGL display struct.
   */
  /* @[declare_core2foraws_display_ptr] */
  extern lv_display_t *core2foraws_display_ptr;
  /* @[declare_core2foraws_display_ptr] */

  /**
     * @brief Gets the esp_lcd_touch driver handle for the FT6336U.
   *
   * Can be used by other subsystems (e.g. virtual buttons) to read raw
   * touch coordinates from the capacitive touch panel.
  *
  * @deprecated Use core2foraws_display_touch_data_get() so access is
  * serialized with all other internal-I2C peripherals.
     *
     * @param[out] touch_handle Receives the touch driver handle.
     * @return
     *  - ESP_OK                : Success
     *  - ESP_ERR_INVALID_ARG   : @p touch_handle is `NULL`
     *  - ESP_ERR_INVALID_STATE : The touch driver is not initialized
   */
  /* @[declare_core2foraws_display_touch_handle] */
    esp_err_t core2foraws_display_get_touch_handle(
      esp_lcd_touch_handle_t *touch_handle );
  /* @[declare_core2foraws_display_touch_handle] */

  /**
   * @brief Reads one or more FT6336 touch points under the shared internal-I2C
   * lock.
   *
   * Use this function instead of calling `esp_lcd_touch_read_data()` directly
   * so touch, PMU, RTC, IMU, and secure-element transactions remain serialized.
   *
   * @param[out] points Touch-point output array.
   * @param[out] point_count Number of reported points.
   * @param[in] max_points Capacity of @p points.
   * @return ESP_OK on success or an argument, state, lock, or I2C error.
   */
  /* @[declare_core2foraws_display_touch_data_get] */
  esp_err_t core2foraws_display_touch_data_get(
      esp_lcd_touch_point_data_t *points, uint8_t *point_count,
      uint8_t max_points );
  /* @[declare_core2foraws_display_touch_data_get] */

  /**
   * @brief Initializes the display controller and touch driver.
   *
   * Uses esp_lcd to drive the ILI9342C display via SPI, esp_lcd_touch for
   * the FT6336U capacitive touch controller over I2C, and esp_lvgl_port to
   * integrate both with the LVGL graphics library.
   *
   * @note The core2foraws_init() calls this function
   * when the hardware feature is enabled.
   *
   * @return
   * [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros).
   *  - ESP_OK                : Success
   *  - ESP_ERR_INVALID_STATE : Library is unable to initialize
   *  - ESP_ERR_NO_MEM        : Out of memory
   */
  /* @[declare_core2foraws_display_init] */
  esp_err_t core2foraws_display_init( void );
  /* @[declare_core2foraws_display_init] */

  /**
   * @brief Stops LVGL and releases the display, touch, and panel-I/O resources.
   *
   * The shared SPI bus remains initialized because the SD card may still use
   * it. This function is idempotent.
   *
   * @return ESP_OK.
   */
  /* @[declare_core2foraws_display_deinit] */
  esp_err_t core2foraws_display_deinit( void );
  /* @[declare_core2foraws_display_deinit] */

#ifdef __cplusplus
}
#endif
#endif