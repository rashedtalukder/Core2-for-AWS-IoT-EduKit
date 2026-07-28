
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
 * @file core2foraws_rgb_led.c
 * @brief Standalone SK6812 RGB LED driver for Core2 for AWS IoT Kit.
 *        Uses RMT peripheral directly — no external library dependencies.
 */

#include <string.h>
#include <stdatomic.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_rom_sys.h>

#include "core2foraws_common.h"
#include "core2foraws_rgb_led.h"

static const char *_TAG = "CORE2FORAWS_RGB_LED";

/* ── Hardware constants ─────────────────────────────────────────── */
#define SK6812_GPIO             GPIO_NUM_25
#define SK6812_BYTES_PER_LED    3          /* GRB, no white channel    */
#define SK6812_BUF_SIZE         ( RGB_LED_NUMS * SK6812_BYTES_PER_LED )
#define SK6812_FLUSH_TIMEOUT_MS 1000
#define SK6812_RESET_US         80         /* latch / reset pause (SK6812
                                              requires >= 80 us low) */

/* RMT resolution: 10 MHz, 1 tick = 100 ns */
#define SK6812_RMT_RESOLUTION_HZ 10000000

/* SK6812 bit timings in RMT ticks (1 tick = 100 ns) */
#define SK6812_T0H_TICKS  3   /* 300 ns */
#define SK6812_T0L_TICKS  9   /* 900 ns */
#define SK6812_T1H_TICKS  6   /* 600 ns */
#define SK6812_T1L_TICKS  6   /* 600 ns */

/* ── Static state ───────────────────────────────────────────────── */
static uint8_t  _led_buf[SK6812_BUF_SIZE]; /* GRB pixel buffer      */
static uint8_t  _tx_buf[SK6812_BUF_SIZE];  /* brightness-scaled TX  */
static uint8_t  _brightness = 255;         /* 0-255 driver scale    */
static bool     _initialised = false;

static rmt_channel_handle_t _rmt_channel = NULL;
static rmt_encoder_handle_t _rmt_encoder = NULL;
static StaticSemaphore_t _rgb_mutex_storage;
static SemaphoreHandle_t _rgb_mutex = NULL;
static atomic_uchar _rgb_mutex_state;

static esp_err_t _rgb_lock( void )
{
    if( atomic_load( &_rgb_mutex_state ) != 2 )
    {
        unsigned char expected = 0;
        if( atomic_compare_exchange_strong( &_rgb_mutex_state, &expected, 1 ) )
        {
            _rgb_mutex = xSemaphoreCreateMutexStatic( &_rgb_mutex_storage );
            atomic_store( &_rgb_mutex_state, _rgb_mutex != NULL ? 2 : 0 );
        }
        else
        {
            while( atomic_load( &_rgb_mutex_state ) == 1 )
            {
                taskYIELD();
            }
        }
    }

    if( _rgb_mutex == NULL )
    {
        return ESP_ERR_NO_MEM;
    }
    return xSemaphoreTake( _rgb_mutex,
                           pdMS_TO_TICKS( SK6812_FLUSH_TIMEOUT_MS ) ) == pdTRUE
               ? ESP_OK
               : ESP_ERR_TIMEOUT;
}

static void _rgb_unlock( void )
{
    xSemaphoreGive( _rgb_mutex );
}

/* ── Brightness helper (video-safe scale, never dims to 0) ─────── */
static inline uint8_t _scale8_video( uint8_t val, uint8_t scale )
{
    return (uint8_t)( ( (int)val * (int)scale ) >> 8 )
           + ( ( val && scale ) ? 1 : 0 );
}

/* ── Public API ─────────────────────────────────────────────────── */

esp_err_t core2foraws_rgb_led_init( void )
{
    ESP_LOGI( _TAG, "\tInitializing" );

    esp_err_t err = _rgb_lock();
    if( err != ESP_OK )
    {
        return err;
    }

    if ( _initialised )
    {
        _rgb_unlock();
        return ESP_OK;
    }

    /* Zero the pixel buffer */
    memset( _led_buf, 0, SK6812_BUF_SIZE );

    /* Configure RMT TX channel */
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num          = SK6812_GPIO,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = SK6812_RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    err = rmt_new_tx_channel( &tx_chan_config, &_rmt_channel );
    if ( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "Failed to create RMT TX channel on GPIO%d: 0x%x", SK6812_GPIO, err );
        _rgb_unlock();
        return err;
    }

    /* Create bytes encoder for SK6812 protocol */
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .duration0 = SK6812_T0H_TICKS,
            .level0    = 1,
            .duration1 = SK6812_T0L_TICKS,
            .level1    = 0,
        },
        .bit1 = {
            .duration0 = SK6812_T1H_TICKS,
            .level0    = 1,
            .duration1 = SK6812_T1L_TICKS,
            .level1    = 0,
        },
        .flags.msb_first = 1,
    };

    err = rmt_new_bytes_encoder( &bytes_encoder_config, &_rmt_encoder );
    if ( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "Failed to create RMT bytes encoder: 0x%x", err );
        rmt_del_channel( _rmt_channel );
        _rmt_channel = NULL;
        _rgb_unlock();
        return err;
    }

    err = rmt_enable( _rmt_channel );
    if ( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "Failed to enable RMT channel: 0x%x", err );
        rmt_del_encoder( _rmt_encoder );
        rmt_del_channel( _rmt_channel );
        _rmt_encoder = NULL;
        _rmt_channel = NULL;
        _rgb_unlock();
        return err;
    }

    _initialised = true;
    _rgb_unlock();
    return ESP_OK;
}

esp_err_t core2foraws_rgb_led_single_color_set( uint8_t led_num, uint32_t color )
{
    if ( led_num >= RGB_LED_NUMS )
    {
        ESP_LOGE( _TAG, "LED number %u is out of range (0-%u)",
                  led_num, RGB_LED_NUMS - 1 );
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = _rgb_lock();
    if( err != ESP_OK ) return err;

    size_t idx = led_num * SK6812_BYTES_PER_LED;
    /* SK6812 expects GRB order */
    _led_buf[idx]     = ( color >> 8 )  & 0xFF;   /* G */
    _led_buf[idx + 1] = ( color >> 16 ) & 0xFF;   /* R */
    _led_buf[idx + 2] =   color         & 0xFF;   /* B */

    _rgb_unlock();
    return ESP_OK;
}

esp_err_t core2foraws_rgb_led_side_color_set( rgb_led_side_type_t side,
                                              uint32_t color )
{
    size_t start, count;

    if ( side == RGB_LED_SIDE_RIGHT )
    {
        start = 0;
        count = RGB_LED_NUMS / 2;
    }
    else if ( side == RGB_LED_SIDE_LEFT )
    {
        start = RGB_LED_NUMS / 2;
        count = RGB_LED_NUMS / 2;
    }
    else
    {
        ESP_LOGE( _TAG, "Invalid side value: %d", side );
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = _rgb_lock();
    if( err != ESP_OK ) return err;

    for ( size_t i = start; i < start + count; i++ )
    {
        size_t idx = i * SK6812_BYTES_PER_LED;
        _led_buf[idx]     = ( color >> 8 )  & 0xFF;
        _led_buf[idx + 1] = ( color >> 16 ) & 0xFF;
        _led_buf[idx + 2] =   color         & 0xFF;
    }

    _rgb_unlock();
    return ESP_OK;
}

esp_err_t core2foraws_rgb_led_brightness_set( uint8_t brightness )
{
    if ( brightness > 100 )
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = _rgb_lock();
    if( err != ESP_OK ) return err;
    _brightness = (uint8_t)( ( (uint16_t)brightness * 255 ) / 100 );

    _rgb_unlock();
    return ESP_OK;
}

esp_err_t core2foraws_rgb_led_write( void )
{
    esp_err_t err = _rgb_lock();
    if( err != ESP_OK ) return err;

    /* The RMT channel and encoder are only valid after initialization.
       Guard against use before init to avoid passing a NULL handle to the
       RMT driver. */
    if ( !_initialised )
    {
        _rgb_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    /* The previous asynchronous transfer owns _tx_buf until completion. */
    err = rmt_tx_wait_all_done( _rmt_channel, SK6812_FLUSH_TIMEOUT_MS );
    if( err != ESP_OK )
    {
        _rgb_unlock();
        return err;
    }

    /* Apply brightness scaling to a separate TX buffer */
    if ( _brightness != 255 )
    {
        for ( size_t i = 0; i < SK6812_BUF_SIZE; i++ )
        {
            _tx_buf[i] = _scale8_video( _led_buf[i], _brightness );
        }
    }
    else
    {
        memcpy( _tx_buf, _led_buf, SK6812_BUF_SIZE );
    }

    esp_rom_delay_us( SK6812_RESET_US );

    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    err = rmt_transmit( _rmt_channel, _rmt_encoder, _tx_buf,
                        SK6812_BUF_SIZE, &tx_config );
    _rgb_unlock();
    return err;
}

esp_err_t core2foraws_rgb_led_clear( void )
{
    esp_err_t err = _rgb_lock();
    if( err != ESP_OK ) return err;
    memset( _led_buf, 0, SK6812_BUF_SIZE );
    _rgb_unlock();
    return ESP_OK;
}

esp_err_t core2foraws_rgb_led_deinit( void )
{
    esp_err_t err = _rgb_lock();
    if( err != ESP_OK ) return err;

    if ( !_initialised )
    {
        _rgb_unlock();
        return ESP_OK;
    }

    err = rmt_tx_wait_all_done( _rmt_channel, SK6812_FLUSH_TIMEOUT_MS );
    if( err != ESP_OK )
    {
        _rgb_unlock();
        return err;
    }

    _initialised = false;
    memset( _led_buf, 0, SK6812_BUF_SIZE );

    rmt_disable( _rmt_channel );
    rmt_del_encoder( _rmt_encoder );
    err = rmt_del_channel( _rmt_channel );
    _rmt_encoder = NULL;
    _rmt_channel = NULL;

    _rgb_unlock();
    return err;
}