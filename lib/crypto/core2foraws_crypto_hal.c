/*
 * Core2 for AWS IoT Kit BSP — ATECC608 I2C HAL shim
 * Copyright (C) 2026 Rashed Talukder.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * This file provides __wrap_hal_i2c_* functions that override the default
 * esp-cryptoauthlib I2C HAL at link time (via -Wl,--wrap).  Instead of
 * creating a separate I2C master bus, the ATECC608 is registered as a
 * device on the BSP's shared internal I2C bus (GPIO21/GPIO22, I2C_NUM_0)
 * which is also used by the AXP192, BM8563, FT6X36, and MPU6886.
 */

#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "cryptoauthlib.h"

#include "core2foraws_common.h"
#include "core2foraws_i2c.h"

static const char *TAG = "ATECC608_HAL";

static i2c_master_dev_handle_t _atecc_dev = NULL;

/* ------------------------------------------------------------------ */
/*  Wrapped HAL functions                                              */
/* ------------------------------------------------------------------ */

ATCA_STATUS __wrap_hal_i2c_change_baud( ATCAIface iface, uint32_t speed )
{
    (void) iface;
    (void) speed;
    return ATCA_SUCCESS;
}

ATCA_STATUS __wrap_hal_i2c_init( ATCAIface iface, ATCAIfaceCfg *cfg )
{
    if( _atecc_dev != NULL )
    {
        return ATCA_SUCCESS;
    }

    uint16_t addr;
#ifdef ATCA_ENABLE_DEPRECATED
    addr = cfg->atcai2c.slave_address >> 1;
#else
    addr = cfg->atcai2c.address >> 1;
#endif

    esp_err_t err = core2foraws_i2c_device_add( CORE2FORAWS_I2C_INTERNAL, addr,
        cfg->atcai2c.baud, &_atecc_dev );
    if( err != ESP_OK )
    {
        ESP_LOGE( TAG, "Failed to add ATECC608 I2C device: %s",
                  esp_err_to_name( err ) );
        return ATCA_COMM_FAIL;
    }

    return ATCA_SUCCESS;
}

ATCA_STATUS __wrap_hal_i2c_post_init( ATCAIface iface )
{
    (void) iface;
    return ATCA_SUCCESS;
}

ATCA_STATUS __wrap_hal_i2c_send( ATCAIface iface, uint8_t word_address,
                                 uint8_t *txdata, int txlength )
{
    (void) iface;

    size_t write_size = 1;
    if( NULL != txdata && 0 < txlength )
    {
        write_size += txlength;
    }

    uint8_t *write_buffer = malloc( write_size );
    if( !write_buffer )
    {
        return ATCA_COMM_FAIL;
    }

    write_buffer[0] = word_address;
    if( NULL != txdata && 0 < txlength )
    {
        memcpy( write_buffer + 1, txdata, txlength );
    }

    esp_err_t err = core2foraws_i2c_write( CORE2FORAWS_I2C_INTERNAL, _atecc_dev,
        CORE2FORAWS_I2C_NO_REG, write_buffer, write_size );

    ESP_LOGD( TAG, "txdata: %p , txlength: %d error: %s", txdata, txlength,
        esp_err_to_name( err ) );

    free( write_buffer );

    if( err == ESP_OK )
    {
        return ATCA_SUCCESS;
    }

    return ATCA_COMM_FAIL;
}

ATCA_STATUS __wrap_hal_i2c_receive( ATCAIface iface, uint8_t word_address,
                                    uint8_t *rxdata, uint16_t *rxlength )
{
    (void) iface;
    (void) word_address;

    if( ( NULL == rxlength ) || ( NULL == rxdata ) )
    {
        return ATCA_TRACE( ATCA_BAD_PARAM, "NULL pointer encountered" );
    }

    esp_err_t err = core2foraws_i2c_read( CORE2FORAWS_I2C_INTERNAL, _atecc_dev,
        CORE2FORAWS_I2C_NO_REG, rxdata, *rxlength );

    if( err == ESP_OK )
    {
        return ATCA_SUCCESS;
    }

    return ATCA_COMM_FAIL;
}

ATCA_STATUS __wrap_hal_i2c_release( void *hal_data )
{
    (void) hal_data;
    return ATCA_SUCCESS;
}

ATCA_STATUS __wrap_hal_i2c_control( ATCAIface iface, uint8_t option,
                                    void *param, size_t paramlen )
{
    (void) param;
    (void) paramlen;

    if( iface && iface->mIfaceCFG )
    {
        if( ATCA_HAL_CHANGE_BAUD == option )
        {
            return __wrap_hal_i2c_change_baud( iface, *( uint32_t * )param );
        }
        else
        {
            return ATCA_UNIMPLEMENTED;
        }
    }
    return ATCA_BAD_PARAM;
}
