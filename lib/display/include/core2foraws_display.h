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
   * @brief Handle to the esp_lcd_touch driver for the FT6336U.
   *
   * Can be used by other subsystems (e.g. virtual buttons) to read raw
   * touch coordinates from the capacitive touch panel.
   */
  /* @[declare_core2foraws_display_touch_handle] */
  esp_lcd_touch_handle_t core2foraws_display_get_touch_handle( void );
  /* @[declare_core2foraws_display_touch_handle] */

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
  /* @[declare_xore2foraws_display_init] */
  esp_err_t core2foraws_display_init( void );
  /* @[declare_xore2foraws_display_init] */

#ifdef __cplusplus
}
#endif
#endif