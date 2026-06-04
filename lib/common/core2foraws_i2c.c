/*
 * Core2 for AWS IoT Kit BSP
 * Copyright (C) 2026 Rashed Talukder.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/i2c_master.h>

#include "core2foraws_i2c.h"

static const char *_TAG = "CORE2FORAWS_I2C";

/* Bus pin configurations */
#define INTERNAL_I2C_SDA    GPIO_NUM_21
#define INTERNAL_I2C_SCL    GPIO_NUM_22
#define INTERNAL_I2C_FREQ   100000

#define EXTERNAL_I2C_SDA    GPIO_NUM_32
#define EXTERNAL_I2C_SCL    GPIO_NUM_33
#define EXTERNAL_I2C_FREQ   100000

#define I2C_LOCK_TIMEOUT_MS 1000

/* Per-transfer timeout (milliseconds) passed to the i2c_master driver. A
 * bounded value ensures a stuck or clock-stretching device cannot block the
 * caller forever while holding the per-bus mutex. */
#define I2C_XFER_TIMEOUT_MS 1000

/* Stack buffer size for register writes (register address + payload). Writes
 * that fit are served without a heap allocation; larger writes fall back to
 * malloc. Sized to cover the common 1-2 byte register writes plus a small
 * multi-byte payload. */
#define I2C_WRITE_STACK_BUF_SIZE 32

/* Per-bus state */
static i2c_master_bus_handle_t _bus_handle[ CORE2FORAWS_I2C_PORT_MAX ] = { NULL, NULL };
static SemaphoreHandle_t _bus_mutex[ CORE2FORAWS_I2C_PORT_MAX ] = { NULL, NULL };

static const gpio_num_t _sda_pin[ CORE2FORAWS_I2C_PORT_MAX ] = { INTERNAL_I2C_SDA, EXTERNAL_I2C_SDA };
static const gpio_num_t _scl_pin[ CORE2FORAWS_I2C_PORT_MAX ] = { INTERNAL_I2C_SCL, EXTERNAL_I2C_SCL };
static const i2c_port_num_t _i2c_port[ CORE2FORAWS_I2C_PORT_MAX ] = { I2C_NUM_0, I2C_NUM_1 };

esp_err_t core2foraws_i2c_init( core2foraws_i2c_port_t port )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX )
    {
        return ESP_ERR_INVALID_ARG;
    }

    /* Already initialized */
    if( _bus_handle[ port ] != NULL )
    {
        return ESP_OK;
    }

    /* Create mutex */
    if( _bus_mutex[ port ] == NULL )
    {
        _bus_mutex[ port ] = xSemaphoreCreateMutex();
        if( _bus_mutex[ port ] == NULL )
        {
            ESP_LOGE( _TAG, "Failed to create mutex for port %d", port );
            return ESP_ERR_NO_MEM;
        }
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = _i2c_port[ port ],
        .sda_io_num = _sda_pin[ port ],
        .scl_io_num = _scl_pin[ port ],
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus( &bus_cfg, &_bus_handle[ port ] );
    if( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "Failed to create I2C master bus on port %d: 0x%x",
                  port, err );
        _bus_handle[ port ] = NULL;
    }
    else
    {
        ESP_LOGI( _TAG, "I2C master bus initialized on port %d (SDA=%d, SCL=%d)",
                  port, _sda_pin[ port ], _scl_pin[ port ] );
    }

    return err;
}

esp_err_t core2foraws_i2c_deinit( core2foraws_i2c_port_t port )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if( _bus_handle[ port ] == NULL )
    {
        return ESP_OK;
    }

    esp_err_t err = i2c_del_master_bus( _bus_handle[ port ] );
    if( err == ESP_OK )
    {
        _bus_handle[ port ] = NULL;
    }

    return err;
}

esp_err_t core2foraws_i2c_get_bus_handle( core2foraws_i2c_port_t port,
                                          i2c_master_bus_handle_t *handle )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX || handle == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if( _bus_handle[ port ] == NULL )
    {
        return ESP_ERR_INVALID_STATE;
    }

    *handle = _bus_handle[ port ];
    return ESP_OK;
}

esp_err_t core2foraws_i2c_device_add( core2foraws_i2c_port_t port,
                                      uint16_t dev_addr,
                                      uint32_t scl_speed_hz,
                                      i2c_master_dev_handle_t *dev_handle )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX || dev_handle == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if( _bus_handle[ port ] == NULL )
    {
        ESP_LOGE( _TAG, "Bus port %d not initialized", port );
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = scl_speed_hz,
    };

    esp_err_t err = i2c_master_bus_add_device( _bus_handle[ port ], &dev_cfg, dev_handle );
    if( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "Failed to add device 0x%02x on port %d: 0x%x",
                  dev_addr, port, err );
    }
    else
    {
        /* One line per device (registration is rare), so this is safe to
           leave on without flooding the console. */
        ESP_LOGV( _TAG, "Added device 0x%02x on port %d at %lu Hz",
                  dev_addr, port, ( unsigned long ) scl_speed_hz );
    }

    return err;
}

esp_err_t core2foraws_i2c_device_remove( i2c_master_dev_handle_t dev_handle )
{
    if( dev_handle == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_bus_rm_device( dev_handle );
}

esp_err_t core2foraws_i2c_read( core2foraws_i2c_port_t port,
                                i2c_master_dev_handle_t dev_handle,
                                uint32_t reg,
                                uint8_t *buffer,
                                uint16_t size )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX || dev_handle == NULL ||
        buffer == NULL || size == 0 )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if( _bus_mutex[ port ] == NULL )
    {
        return ESP_ERR_INVALID_STATE;
    }

    if( xSemaphoreTake( _bus_mutex[ port ],
                        pdMS_TO_TICKS( I2C_LOCK_TIMEOUT_MS ) ) != pdTRUE )
    {
        ESP_LOGE( _TAG, "I2C read: mutex timeout on port %d", port );
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err;

    if( reg & CORE2FORAWS_I2C_NO_REG )
    {
        /* No register address — direct read */
        err = i2c_master_receive( dev_handle, buffer, size,
                                  I2C_XFER_TIMEOUT_MS );
    }
    else
    {
        /* Write register address, then read (atomic with repeated start) */
        uint8_t reg_buf[ 2 ];
        uint8_t reg_len;

        if( reg & CORE2FORAWS_I2C_REG_16 )
        {
            reg_buf[ 0 ] = ( reg >> 8 ) & 0xFF;
            reg_buf[ 1 ] = reg & 0xFF;
            reg_len = 2;
        }
        else
        {
            reg_buf[ 0 ] = reg & 0xFF;
            reg_len = 1;
        }

        err = i2c_master_transmit_receive( dev_handle, reg_buf, reg_len,
                                           buffer, size,
                                           I2C_XFER_TIMEOUT_MS );
    }

    xSemaphoreGive( _bus_mutex[ port ] );
    return err;
}

esp_err_t core2foraws_i2c_write( core2foraws_i2c_port_t port,
                                 i2c_master_dev_handle_t dev_handle,
                                 uint32_t reg,
                                 const uint8_t *buffer,
                                 uint16_t size )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX || dev_handle == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if( _bus_mutex[ port ] == NULL )
    {
        return ESP_ERR_INVALID_STATE;
    }

    if( xSemaphoreTake( _bus_mutex[ port ],
                        pdMS_TO_TICKS( I2C_LOCK_TIMEOUT_MS ) ) != pdTRUE )
    {
        ESP_LOGE( _TAG, "I2C write: mutex timeout on port %d", port );
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err;

    if( reg & CORE2FORAWS_I2C_NO_REG )
    {
        /* No register address — direct write */
        err = i2c_master_transmit( dev_handle, buffer, size,
                                   I2C_XFER_TIMEOUT_MS );
    }
    else
    {
        /* Prepend register address to data */
        uint8_t reg_len;
        if( reg & CORE2FORAWS_I2C_REG_16 )
        {
            reg_len = 2;
        }
        else
        {
            reg_len = 1;
        }

        /* Most register writes are a few bytes, so serve them from a stack
           buffer and avoid heap churn on this hot path. Fall back to a
           heap allocation only for unusually large payloads. */
        uint8_t stack_buf[ I2C_WRITE_STACK_BUF_SIZE ];
        uint8_t *tx_buf;
        bool tx_buf_heap = false;

        if( (size_t)reg_len + size <= sizeof( stack_buf ) )
        {
            tx_buf = stack_buf;
        }
        else
        {
            tx_buf = malloc( reg_len + size );
            if( tx_buf == NULL )
            {
                xSemaphoreGive( _bus_mutex[ port ] );
                return ESP_ERR_NO_MEM;
            }
            tx_buf_heap = true;
        }

        if( reg_len == 2 )
        {
            tx_buf[ 0 ] = ( reg >> 8 ) & 0xFF;
            tx_buf[ 1 ] = reg & 0xFF;
        }
        else
        {
            tx_buf[ 0 ] = reg & 0xFF;
        }

        if( size > 0 && buffer != NULL )
        {
            memcpy( tx_buf + reg_len, buffer, size );
        }

        err = i2c_master_transmit( dev_handle, tx_buf, reg_len + size,
                                   I2C_XFER_TIMEOUT_MS );
        if( tx_buf_heap )
        {
            free( tx_buf );
        }
    }

    xSemaphoreGive( _bus_mutex[ port ] );
    return err;
}

esp_err_t core2foraws_i2c_lock( core2foraws_i2c_port_t port )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX || _bus_mutex[ port ] == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if( xSemaphoreTake( _bus_mutex[ port ],
                        pdMS_TO_TICKS( I2C_LOCK_TIMEOUT_MS ) ) != pdTRUE )
    {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t core2foraws_i2c_unlock( core2foraws_i2c_port_t port )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX || _bus_mutex[ port ] == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreGive( _bus_mutex[ port ] );
    return ESP_OK;
}
