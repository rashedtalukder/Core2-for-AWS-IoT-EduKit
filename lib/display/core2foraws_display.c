/*
 * Core2 for AWS IoT Kit BSP v2.0.0
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
 * @file core2foraws_display.c
 * @brief Core2 for AWS IoT Kit display driver using esp_lcd + esp_lcd_touch + esp_lvgl_port
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_check.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_ili9341.h>
#include <esp_lcd_touch_ft5x06.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_lvgl_port.h>

#include "core2foraws_common.h"
#include "core2foraws_display.h"

/* ── Hardware constants (from schema.yml) ── */

/* SPI bus shared with SD card (HSPI / SPI2) */
#define LCD_SPI_HOST        SPI2_HOST

/* ILI9342C display (ILI9341-compatible) */
#define LCD_H_RES           320
#define LCD_V_RES           240
#define LCD_SPI_MOSI        GPIO_NUM_23
#define LCD_SPI_MISO        GPIO_NUM_38
#define LCD_SPI_SCLK        GPIO_NUM_18
#define LCD_SPI_CS          GPIO_NUM_5
#define LCD_DC              GPIO_NUM_15
#define LCD_PIXEL_CLK_HZ    ( 40 * 1000 * 1000 )
#define LCD_CMD_BITS        8
#define LCD_PARAM_BITS      8
#define LCD_DRAW_BUF_LINES  25

/* FT6336U touch controller on internal I2C bus */
#define TOUCH_INT_GPIO      GPIO_NUM_39

static const char *_TAG = "CORE2FORAWS_DISPLAY";

/* Handles exposed to consumers */
lv_display_t *core2foraws_display_ptr = NULL;

/* Private handles */
static esp_lcd_panel_io_handle_t _io_handle = NULL;
static esp_lcd_panel_handle_t    _panel_handle = NULL;
static esp_lcd_touch_handle_t    _touch_handle = NULL;
static lv_indev_t               *_touch_indev = NULL;

/**
 * @brief Initialize the SPI bus shared between LCD and SD card.
 *
 * The bus is configured once; both esp_lcd and the SD card driver
 * attach their own devices to it afterwards.
 */
static esp_err_t _init_spi_bus( void )
{
    const spi_bus_config_t bus_cfg = {
        .mosi_io_num   = LCD_SPI_MOSI,
        .miso_io_num   = LCD_SPI_MISO,
        .sclk_io_num   = LCD_SPI_SCLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_H_RES * LCD_DRAW_BUF_LINES * sizeof( uint16_t ),
    };

    return spi_bus_initialize( LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO );
}

/**
 * @brief Create the esp_lcd panel IO and panel driver for the ILI9341.
 */
static esp_err_t _init_lcd_panel( void )
{
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num       = LCD_DC,
        .cs_gpio_num       = LCD_SPI_CS,
        .pclk_hz           = LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits      = LCD_CMD_BITS,
        .lcd_param_bits    = LCD_PARAM_BITS,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };

    esp_err_t err = esp_lcd_new_panel_io_spi(
        ( esp_lcd_spi_bus_handle_t ) LCD_SPI_HOST, &io_config, &_io_handle );
    if( err != ESP_OK )
    {
        return err;
    }

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num  = GPIO_NUM_NC,
        .rgb_ele_order   = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel  = 16,
    };

    err = esp_lcd_new_panel_ili9341( _io_handle, &panel_config, &_panel_handle );
    if( err != ESP_OK )
    {
        return err;
    }

    ESP_RETURN_ON_ERROR( esp_lcd_panel_reset( _panel_handle ), _TAG, "panel reset" );
    ESP_RETURN_ON_ERROR( esp_lcd_panel_init( _panel_handle ), _TAG, "panel init" );
    ESP_RETURN_ON_ERROR( esp_lcd_panel_invert_color( _panel_handle, true ), _TAG, "invert color" );
    ESP_RETURN_ON_ERROR( esp_lcd_panel_disp_on_off( _panel_handle, true ), _TAG, "disp on" );

    return ESP_OK;
}

/**
 * @brief Create the esp_lcd_touch driver for the FT6336U (FT5x06-compatible).
 */
static esp_err_t _init_touch( void )
{
    esp_lcd_panel_io_handle_t touch_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config =
        ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    io_config.scl_speed_hz = 400000;

    i2c_master_bus_handle_t i2c_bus = NULL;
    esp_err_t err = core2foraws_i2c_get_bus_handle( COMMON_I2C_INTERNAL, &i2c_bus );
    if( err != ESP_OK )
    {
        return err;
    }

    err = esp_lcd_new_panel_io_i2c_v2(
        i2c_bus, &io_config, &touch_io_handle );
    if( err != ESP_OK )
    {
        return err;
    }

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = TOUCH_INT_GPIO,
        .levels = {
            .reset     = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy  = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    return esp_lcd_touch_new_i2c_ft5x06( touch_io_handle, &tp_cfg,
                                         &_touch_handle );
}

esp_lcd_touch_handle_t core2foraws_display_get_touch_handle( void )
{
    return _touch_handle;
}

esp_err_t core2foraws_display_init( void )
{
    ESP_LOGI( _TAG, "\tInitializing" );

    if( core2foraws_display_ptr != NULL )
    {
        ESP_LOGW( _TAG, "Display already initialized" );
        return ESP_OK;
    }

    if( core2foraws_common_spi_semaphore == NULL )
    {
        core2foraws_common_spi_semaphore = xSemaphoreCreateMutex();
        if( core2foraws_common_spi_semaphore == NULL )
        {
            ESP_LOGE( _TAG, "Failed to create shared SPI semaphore" );
            return ESP_ERR_NO_MEM;
        }
    }

    /* ── 1. SPI bus (shared with SD card) ── */
    esp_err_t err = _init_spi_bus();
    if( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "SPI bus init failed: 0x%x", err );
        return err;
    }

    /* ── 2. LCD panel via esp_lcd ── */
    err = _init_lcd_panel();
    if( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "LCD panel init failed: 0x%x", err );
        return err;
    }

    /* ── 3. Touch via esp_lcd_touch ── */
    err = _init_touch();
    if( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "Touch init failed: 0x%x", err );
        return err;
    }

    /* ── 4. LVGL port ── */
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    /* Raise the LVGL task stack from the default 7168 bytes to avoid a
     * stack overflow during canvas/image rendering. */
    lvgl_cfg.task_stack = 10240;
    err = lvgl_port_init( &lvgl_cfg );
    if( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "LVGL port init failed: 0x%x", err );
        return err;
    }

    /* ── 5. Add display to LVGL port ── */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle     = _io_handle,
        .panel_handle  = _panel_handle,
        .buffer_size   = LCD_H_RES * LCD_DRAW_BUF_LINES,
        .double_buffer = true,
        .hres          = LCD_H_RES,
        .vres          = LCD_V_RES,
        .monochrome    = false,
        .rotation = {
            .swap_xy  = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma    = true,
            .buff_spiram = false,
            .swap_bytes  = true,
        },
    };

    core2foraws_display_ptr = lvgl_port_add_disp( &disp_cfg );
    if( core2foraws_display_ptr == NULL )
    {
        ESP_LOGE( _TAG, "Failed to add display to LVGL port" );
        return ESP_FAIL;
    }

    /* ── 6. Add touch input to LVGL port ── */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp   = core2foraws_display_ptr,
        .handle = _touch_handle,
    };

    _touch_indev = lvgl_port_add_touch( &touch_cfg );
    if( _touch_indev == NULL )
    {
        ESP_LOGW( _TAG, "Failed to add touch input" );
    }

    ESP_LOGI( _TAG, "\tDisplay initialized" );
    return ESP_OK;
}