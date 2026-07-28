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
#define LCD_DRAW_BUF_LINES  40

/* FT6336U touch controller on internal I2C bus */
#define TOUCH_INT_GPIO      GPIO_NUM_39

static const char *_TAG = "CORE2FORAWS_DISPLAY";

/* Handles exposed to consumers */
lv_display_t *core2foraws_display_ptr = NULL;

/* Private handles */
static esp_lcd_panel_io_handle_t _io_handle = NULL;
static esp_lcd_panel_io_handle_t _touch_io_handle = NULL;
static esp_lcd_panel_handle_t    _panel_handle = NULL;
static esp_lcd_touch_handle_t    _touch_handle = NULL;
static lv_indev_t               *_touch_indev = NULL;
static bool                      _lvgl_initialized = false;

static void _display_flush_start( lv_event_t *event )
{
    (void)event;
    xSemaphoreTake( core2foraws_common_spi_semaphore, portMAX_DELAY );
}

static bool _display_flush_done( esp_lcd_panel_io_handle_t panel_io,
                                 esp_lcd_panel_io_event_data_t *event_data,
                                 void *user_ctx )
{
    (void)panel_io;
    (void)event_data;

    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR( core2foraws_common_spi_semaphore, &task_woken );
    lvgl_port_flush_ready( ( lv_display_t * )user_ctx );
    return task_woken == pdTRUE;
}

static esp_err_t _display_touch_read( esp_lcd_touch_point_data_t *points,
                                      uint8_t *point_count,
                                      uint8_t max_points )
{
    if( points == NULL || point_count == NULL || max_points == 0 )
    {
        return ESP_ERR_INVALID_ARG;
    }
    if( _touch_handle == NULL )
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = core2foraws_i2c_lock( COMMON_I2C_INTERNAL );
    if( err != ESP_OK )
    {
        return err;
    }

    err = esp_lcd_touch_read_data( _touch_handle );
    if( err == ESP_OK )
    {
        err = esp_lcd_touch_get_data( _touch_handle, points, point_count,
                                      max_points );
    }

    esp_err_t unlock_err = core2foraws_i2c_unlock( COMMON_I2C_INTERNAL );
    return err != ESP_OK ? err : unlock_err;
}

static void _lvgl_touch_read( lv_indev_t *indev, lv_indev_data_t *data )
{
    (void)indev;
    esp_lcd_touch_point_data_t point;
    uint8_t point_count = 0;

    data->state = LV_INDEV_STATE_RELEASED;
    if( _display_touch_read( &point, &point_count, 1 ) == ESP_OK &&
        point_count > 0 )
    {
        data->point.x = point.x;
        data->point.y = point.y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
}

static void _display_cleanup( void )
{
    bool spi_locked = false;
    if( _lvgl_initialized )
    {
        lvgl_port_stop();
    }

    /* LV_EVENT_FLUSH_START takes this semaphore and the SPI completion ISR
     * gives it. After stopping LVGL refresh, taking it waits for any active
     * callback to finish before its display context is freed. */
    if( core2foraws_common_spi_semaphore != NULL &&
        xSemaphoreTake( core2foraws_common_spi_semaphore,
                        portMAX_DELAY ) == pdTRUE )
    {
        spi_locked = true;
    }

    if( _touch_indev != NULL )
    {
        lvgl_port_lock( 0 );
        lv_indev_delete( _touch_indev );
        lvgl_port_unlock();
        _touch_indev = NULL;
    }

    if( core2foraws_display_ptr != NULL )
    {
        lvgl_port_remove_disp( core2foraws_display_ptr );
        core2foraws_display_ptr = NULL;
    }

    if( _lvgl_initialized )
    {
        lvgl_port_deinit();
        _lvgl_initialized = false;
    }

    if( _touch_handle != NULL )
    {
        esp_lcd_touch_del( _touch_handle );
        _touch_handle = NULL;
    }
    if( _touch_io_handle != NULL )
    {
        esp_lcd_panel_io_del( _touch_io_handle );
        _touch_io_handle = NULL;
    }
    if( _panel_handle != NULL )
    {
        esp_lcd_panel_del( _panel_handle );
        _panel_handle = NULL;
    }
    if( _io_handle != NULL )
    {
        esp_lcd_panel_io_del( _io_handle );
        _io_handle = NULL;
    }

    if( spi_locked )
    {
        xSemaphoreGive( core2foraws_common_spi_semaphore );
    }
}

esp_err_t core2foraws_display_touch_data_get(
    esp_lcd_touch_point_data_t *points, uint8_t *point_count,
    uint8_t max_points )
{
    return _display_touch_read( points, point_count, max_points );
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
        i2c_bus, &io_config, &_touch_io_handle );
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

    return esp_lcd_touch_new_i2c_ft5x06( _touch_io_handle, &tp_cfg,
                                         &_touch_handle );
}

esp_err_t core2foraws_display_get_touch_handle(
    esp_lcd_touch_handle_t *touch_handle )
{
    if( touch_handle == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if( _touch_handle == NULL )
    {
        return ESP_ERR_INVALID_STATE;
    }

    *touch_handle = _touch_handle;
    return ESP_OK;
}

esp_err_t core2foraws_display_init( void )
{
    ESP_LOGI( _TAG, "\tInitializing" );

    if( core2foraws_display_ptr != NULL )
    {
        ESP_LOGW( _TAG, "Display already initialized" );
        return ESP_OK;
    }

    esp_err_t err = core2foraws_common_spi_bus_init();
    if( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "Failed to create shared SPI semaphore: 0x%x", err );
        return err;
    }

    /* ── 1. LCD panel via the shared SPI bus ── */
    err = _init_lcd_panel();
    if( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "LCD panel init failed: 0x%x", err );
        _display_cleanup();
        return err;
    }

    /* ── 2. Touch via esp_lcd_touch ── */
    err = _init_touch();
    if( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "Touch init failed: 0x%x", err );
        _display_cleanup();
        return err;
    }

    /* ── 3. LVGL port ── */
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    /* Raise the LVGL task stack from the default 7168 bytes to avoid a
     * stack overflow during canvas/image rendering. */
    lvgl_cfg.task_stack = 10240;
    /* Pin the LVGL render/flush task to core 1, away from core 0 where the
     * Wi-Fi stack and the IDF event loop run by default. This keeps the
     * DMA-driven display flush off the same core as networking, avoiding
     * scheduling contention that shows up as dropped frames. */
    lvgl_cfg.task_affinity = 1;
    err = lvgl_port_init( &lvgl_cfg );
    if( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "LVGL port init failed: 0x%x", err );
        _display_cleanup();
        return err;
    }
    _lvgl_initialized = true;

    /* ── 4. Add display to LVGL port ── */
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
            /* Draw buffers must live in internal, DMA-capable RAM. PSRAM is
             * not DMA-addressable; a PSRAM draw buffer stalls the SPI flush
             * and causes UI hangs/crashes. Keep buff_spiram = false. See
             * .claude/rules/memory-placement.md. */
            .buff_dma    = true,
            .buff_spiram = false,
            .swap_bytes  = true,
        },
    };

    core2foraws_display_ptr = lvgl_port_add_disp( &disp_cfg );
    if( core2foraws_display_ptr == NULL )
    {
        ESP_LOGE( _TAG, "Failed to add display to LVGL port" );
        _display_cleanup();
        return ESP_FAIL;
    }

    const esp_lcd_panel_io_callbacks_t io_callbacks = {
        .on_color_trans_done = _display_flush_done,
    };
    err = esp_lcd_panel_io_register_event_callbacks(
        _io_handle, &io_callbacks, core2foraws_display_ptr );
    if( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "Failed to register display flush callback: 0x%x",
                  err );
        _display_cleanup();
        return err;
    }

    lv_display_add_event_cb( core2foraws_display_ptr, _display_flush_start,
                             LV_EVENT_FLUSH_START, NULL );

    /* ── 5. Add BSP-owned touch input to LVGL ── */
    lvgl_port_lock( 0 );
    _touch_indev = lv_indev_create();
    if( _touch_indev != NULL )
    {
        lv_indev_set_type( _touch_indev, LV_INDEV_TYPE_POINTER );
        lv_indev_set_display( _touch_indev, core2foraws_display_ptr );
        lv_indev_set_read_cb( _touch_indev, _lvgl_touch_read );
    }
    lvgl_port_unlock();
    if( _touch_indev == NULL )
    {
        ESP_LOGE( _TAG, "Failed to add touch input" );
        _display_cleanup();
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI( _TAG, "\tDisplay initialized" );
    return ESP_OK;
}

esp_err_t core2foraws_display_deinit( void )
{
    _display_cleanup();
    return ESP_OK;
}