/*
 * Core2 for AWS IoT Kit BSP
 * Copyright (C) 2026 Rashed Talukder.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
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

typedef struct core2foraws_i2c_device_node
{
    i2c_master_dev_handle_t handle;
    uint16_t address;
    uint32_t speed_hz;
    size_t references;
    struct core2foraws_i2c_device_node *next;
} core2foraws_i2c_device_node_t;

typedef struct
{
    i2c_master_bus_handle_t handle;
    StaticSemaphore_t mutex_storage;
    SemaphoreHandle_t mutex;
    atomic_uchar mutex_state;
    core2foraws_i2c_device_node_t *devices;
} core2foraws_i2c_bus_state_t;

enum
{
    I2C_MUTEX_UNINITIALIZED = 0,
    I2C_MUTEX_INITIALIZING,
    I2C_MUTEX_READY,
};

/* The internal bus lives for the BSP lifetime. The external bus can be
 * opened, populated with multiple application devices, closed, and reopened. */
static core2foraws_i2c_bus_state_t _bus_state[ CORE2FORAWS_I2C_PORT_MAX ];

static const gpio_num_t _sda_pin[ CORE2FORAWS_I2C_PORT_MAX ] = { INTERNAL_I2C_SDA, EXTERNAL_I2C_SDA };
static const gpio_num_t _scl_pin[ CORE2FORAWS_I2C_PORT_MAX ] = { INTERNAL_I2C_SCL, EXTERNAL_I2C_SCL };
static const i2c_port_num_t _i2c_port[ CORE2FORAWS_I2C_PORT_MAX ] = { I2C_NUM_0, I2C_NUM_1 };

static esp_err_t _i2c_mutex_ensure( core2foraws_i2c_port_t port )
{
    core2foraws_i2c_bus_state_t *state = &_bus_state[ port ];
    unsigned char current = atomic_load( &state->mutex_state );

    if( current == I2C_MUTEX_READY )
    {
        return ESP_OK;
    }

    unsigned char expected = I2C_MUTEX_UNINITIALIZED;
    if( atomic_compare_exchange_strong( &state->mutex_state, &expected,
                                        I2C_MUTEX_INITIALIZING ) )
    {
        state->mutex =
            xSemaphoreCreateRecursiveMutexStatic( &state->mutex_storage );
        if( state->mutex == NULL )
        {
            atomic_store( &state->mutex_state, I2C_MUTEX_UNINITIALIZED );
            return ESP_ERR_NO_MEM;
        }

        atomic_store( &state->mutex_state, I2C_MUTEX_READY );
        return ESP_OK;
    }

    while( atomic_load( &state->mutex_state ) == I2C_MUTEX_INITIALIZING )
    {
        vTaskDelay( 1 );
    }

    return atomic_load( &state->mutex_state ) == I2C_MUTEX_READY
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}

static core2foraws_i2c_device_node_t *_i2c_device_find_by_handle(
    core2foraws_i2c_bus_state_t *state, i2c_master_dev_handle_t handle,
    core2foraws_i2c_device_node_t **previous )
{
    core2foraws_i2c_device_node_t *prev = NULL;
    core2foraws_i2c_device_node_t *node = state->devices;

    while( node != NULL )
    {
        if( node->handle == handle )
        {
            if( previous != NULL )
            {
                *previous = prev;
            }
            return node;
        }
        prev = node;
        node = node->next;
    }

    return NULL;
}

esp_err_t core2foraws_i2c_init( core2foraws_i2c_port_t port )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX )
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = _i2c_mutex_ensure( port );
    if( err != ESP_OK )
    {
        return err;
    }

    err = core2foraws_i2c_lock( port );
    if( err != ESP_OK )
    {
        return err;
    }

    core2foraws_i2c_bus_state_t *state = &_bus_state[ port ];
    if( state->handle != NULL )
    {
        core2foraws_i2c_unlock( port );
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = _i2c_port[ port ],
        .sda_io_num = _sda_pin[ port ],
        .scl_io_num = _scl_pin[ port ],
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    err = i2c_new_master_bus( &bus_cfg, &state->handle );
    if( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "Failed to create I2C master bus on port %d: 0x%x",
                  port, err );
        state->handle = NULL;
    }
    else
    {
        ESP_LOGI( _TAG, "I2C master bus initialized on port %d (SDA=%d, SCL=%d)",
                  port, _sda_pin[ port ], _scl_pin[ port ] );
    }

    core2foraws_i2c_unlock( port );
    return err;
}

esp_err_t core2foraws_i2c_deinit( core2foraws_i2c_port_t port )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if( port == CORE2FORAWS_I2C_INTERNAL )
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_err_t err = _i2c_mutex_ensure( port );
    if( err != ESP_OK )
    {
        return err;
    }

    err = core2foraws_i2c_lock( port );
    if( err != ESP_OK )
    {
        return err;
    }

    core2foraws_i2c_bus_state_t *state = &_bus_state[ port ];
    while( state->devices != NULL )
    {
        core2foraws_i2c_device_node_t *node = state->devices;
        esp_err_t remove_err = i2c_master_bus_rm_device( node->handle );
        if( remove_err != ESP_OK )
        {
            core2foraws_i2c_unlock( port );
            return remove_err;
        }
        state->devices = node->next;
        free( node );
    }

    if( state->handle != NULL )
    {
        esp_err_t delete_err = i2c_del_master_bus( state->handle );
        if( err == ESP_OK )
        {
            err = delete_err;
        }
        if( delete_err == ESP_OK )
        {
            state->handle = NULL;
        }
    }

    core2foraws_i2c_unlock( port );

    return err;
}

esp_err_t core2foraws_i2c_get_bus_handle( core2foraws_i2c_port_t port,
                                          i2c_master_bus_handle_t *handle )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX || handle == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if( atomic_load( &_bus_state[ port ].mutex_state ) != I2C_MUTEX_READY )
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = core2foraws_i2c_lock( port );
    if( err != ESP_OK )
    {
        return err;
    }

    *handle = _bus_state[ port ].handle;
    err = *handle != NULL ? ESP_OK : ESP_ERR_INVALID_STATE;
    core2foraws_i2c_unlock( port );
    return err;
}

esp_err_t core2foraws_i2c_device_add( core2foraws_i2c_port_t port,
                                      uint16_t dev_addr,
                                      uint32_t scl_speed_hz,
                                      i2c_master_dev_handle_t *dev_handle )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX || dev_addr > 0x7f ||
        scl_speed_hz == 0 || dev_handle == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if( atomic_load( &_bus_state[ port ].mutex_state ) != I2C_MUTEX_READY )
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = core2foraws_i2c_lock( port );
    if( err != ESP_OK )
    {
        return err;
    }

    core2foraws_i2c_bus_state_t *state = &_bus_state[ port ];
    if( state->handle == NULL )
    {
        core2foraws_i2c_unlock( port );
        return ESP_ERR_INVALID_STATE;
    }

    for( core2foraws_i2c_device_node_t *node = state->devices;
         node != NULL; node = node->next )
    {
        if( node->address == dev_addr && node->speed_hz == scl_speed_hz )
        {
            node->references++;
            *dev_handle = node->handle;
            core2foraws_i2c_unlock( port );
            return ESP_OK;
        }
    }

    core2foraws_i2c_device_node_t *node = calloc( 1, sizeof( *node ) );
    if( node == NULL )
    {
        core2foraws_i2c_unlock( port );
        return ESP_ERR_NO_MEM;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = scl_speed_hz,
    };

    err = i2c_master_bus_add_device( state->handle, &dev_cfg, dev_handle );
    if( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "Failed to add device 0x%02x on port %d: 0x%x",
                  dev_addr, port, err );
    }
    else
    {
        node->handle = *dev_handle;
        node->address = dev_addr;
        node->speed_hz = scl_speed_hz;
        node->references = 1;
        node->next = state->devices;
        state->devices = node;
        /* One line per device (registration is rare), so this is safe to
           leave on without flooding the console. */
        ESP_LOGV( _TAG, "Added device 0x%02x on port %d at %lu Hz",
                  dev_addr, port, ( unsigned long ) scl_speed_hz );
    }

    if( err != ESP_OK )
    {
        free( node );
    }

    core2foraws_i2c_unlock( port );

    return err;
}

esp_err_t core2foraws_i2c_device_remove( i2c_master_dev_handle_t dev_handle )
{
    if( dev_handle == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    for( core2foraws_i2c_port_t port = CORE2FORAWS_I2C_INTERNAL;
         port < CORE2FORAWS_I2C_PORT_MAX; port++ )
    {
        if( atomic_load( &_bus_state[ port ].mutex_state ) != I2C_MUTEX_READY )
        {
            continue;
        }

        esp_err_t err = core2foraws_i2c_lock( port );
        if( err != ESP_OK )
        {
            return err;
        }

        core2foraws_i2c_bus_state_t *state = &_bus_state[ port ];
        core2foraws_i2c_device_node_t *previous = NULL;
        core2foraws_i2c_device_node_t *node =
            _i2c_device_find_by_handle( state, dev_handle, &previous );
        if( node != NULL )
        {
            if( node->references > 1 )
            {
                node->references--;
                core2foraws_i2c_unlock( port );
                return ESP_OK;
            }

            err = i2c_master_bus_rm_device( node->handle );
            if( err == ESP_OK )
            {
                if( previous == NULL )
                {
                    state->devices = node->next;
                }
                else
                {
                    previous->next = node->next;
                }
                free( node );
            }
            core2foraws_i2c_unlock( port );
            return err;
        }

        core2foraws_i2c_unlock( port );
    }

    return ESP_ERR_NOT_FOUND;
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

    if( atomic_load( &_bus_state[ port ].mutex_state ) != I2C_MUTEX_READY )
    {
        return ESP_ERR_INVALID_STATE;
    }

    if( core2foraws_i2c_lock( port ) != ESP_OK )
    {
        ESP_LOGE( _TAG, "I2C read: mutex timeout on port %d", port );
        return ESP_ERR_TIMEOUT;
    }

    if( _bus_state[ port ].handle == NULL ||
        _i2c_device_find_by_handle( &_bus_state[ port ], dev_handle, NULL ) == NULL )
    {
        core2foraws_i2c_unlock( port );
        return ESP_ERR_INVALID_STATE;
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

    core2foraws_i2c_unlock( port );
    return err;
}

esp_err_t core2foraws_i2c_write( core2foraws_i2c_port_t port,
                                 i2c_master_dev_handle_t dev_handle,
                                 uint32_t reg,
                                 const uint8_t *buffer,
                                 uint16_t size )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX || dev_handle == NULL ||
        ( size > 0 && buffer == NULL ) ||
        ( ( reg & CORE2FORAWS_I2C_NO_REG ) && size == 0 ) )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if( atomic_load( &_bus_state[ port ].mutex_state ) != I2C_MUTEX_READY )
    {
        return ESP_ERR_INVALID_STATE;
    }

    if( core2foraws_i2c_lock( port ) != ESP_OK )
    {
        ESP_LOGE( _TAG, "I2C write: mutex timeout on port %d", port );
        return ESP_ERR_TIMEOUT;
    }

    if( _bus_state[ port ].handle == NULL ||
        _i2c_device_find_by_handle( &_bus_state[ port ], dev_handle, NULL ) == NULL )
    {
        core2foraws_i2c_unlock( port );
        return ESP_ERR_INVALID_STATE;
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
                core2foraws_i2c_unlock( port );
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

        if( size > 0 )
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

    core2foraws_i2c_unlock( port );
    return err;
}

esp_err_t core2foraws_i2c_lock( core2foraws_i2c_port_t port )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX ||
        atomic_load( &_bus_state[ port ].mutex_state ) != I2C_MUTEX_READY )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if( xSemaphoreTakeRecursive( _bus_state[ port ].mutex,
                                 pdMS_TO_TICKS( I2C_LOCK_TIMEOUT_MS ) ) != pdTRUE )
    {
        TaskHandle_t holder = xSemaphoreGetMutexHolder( _bus_state[ port ].mutex );
        ESP_LOGE( _TAG,
              "I2C lock timeout on port %d (requester=%s, holder=%s, state=%d, priority=%u)",
                  port, pcTaskGetName( NULL ),
                  holder != NULL ? pcTaskGetName( holder ) : "none",
                  holder != NULL ? ( int )eTaskGetState( holder ) : -1,
                  holder != NULL ? ( unsigned )uxTaskPriorityGet( holder ) : 0 );
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t core2foraws_i2c_unlock( core2foraws_i2c_port_t port )
{
    if( port >= CORE2FORAWS_I2C_PORT_MAX ||
        atomic_load( &_bus_state[ port ].mutex_state ) != I2C_MUTEX_READY )
    {
        return ESP_ERR_INVALID_ARG;
    }

    return xSemaphoreGiveRecursive( _bus_state[ port ].mutex ) == pdTRUE
               ? ESP_OK
               : ESP_ERR_INVALID_STATE;
}
