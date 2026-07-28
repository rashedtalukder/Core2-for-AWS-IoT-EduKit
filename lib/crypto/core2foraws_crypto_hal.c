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
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "cryptoauthlib.h"

#include "core2foraws_common.h"
#include "core2foraws_i2c.h"

static const char *TAG = "ATECC608_HAL";

/* SDA pin for the internal I2C bus — must match core2foraws_i2c.c */
#define ATECC_SDA_PIN   GPIO_NUM_21

static i2c_master_dev_handle_t _atecc_dev = NULL;

/**
 * @brief Drive the ATECC608 wake pulse on SDA under the shared-bus lock.
 *
 * The wake pulse manipulates SDA directly through GPIO rather than the
 * I2C driver, so it must hold the internal-bus lock for the duration of
 * the pulse. The internal I2C bus is shared with the AXP192, BM8563,
 * touch panel, and MPU6886; without the lock the 80 µs SDA-low pulse
 * would corrupt a concurrent transaction to one of those devices.
 */
static esp_err_t _atecc_wake_pulse( void )
{
    esp_err_t err = core2foraws_i2c_lock( CORE2FORAWS_I2C_INTERNAL );
    if( err != ESP_OK )
    {
        return err;
    }
    gpio_set_direction( ATECC_SDA_PIN, GPIO_MODE_OUTPUT_OD );
    gpio_set_level( ATECC_SDA_PIN, 0 );
    esp_rom_delay_us( 80 );
    gpio_set_level( ATECC_SDA_PIN, 1 );
    /* Restore SDA to I2C peripheral control */
    gpio_set_direction( ATECC_SDA_PIN, GPIO_MODE_INPUT_OUTPUT_OD );
    return core2foraws_i2c_unlock( CORE2FORAWS_I2C_INTERNAL );
}

/**
 * @brief Send the ATECC608 I2C wake pulse and verify the response.
 *
 * The ATECC608 wakes when SDA is held low for ≥60 µs (tWLO).  Rather
 * than issuing a general-call write to address 0x00 through the I2C
 * driver (which logs a spurious NACK error), we briefly switch SDA to
 * GPIO output mode, hold it low, then restore it to open-drain mode
 * for the I2C peripheral.
 */
static ATCA_STATUS _atecc_wake( ATCAIface iface )
{
    if( _atecc_dev == NULL )
    {
        return ATCA_NOT_INITIALIZED;
    }

    ATCAIfaceCfg *cfg = atgetifacecfg( iface );
    if( cfg == NULL )
    {
        return ATCA_BAD_PARAM;
    }

    /* Drive SDA low for ≥60 µs to wake the ATECC608, serialized against
     * other devices sharing the internal I2C bus */
    if( _atecc_wake_pulse() != ESP_OK )
    {
        return ATCA_COMM_FAIL;
    }

    /* Wait tWHI + tWLO */
    atca_delay_us( cfg->wake_delay );

    /* Read the 4-byte wake response from the real device address */
    uint8_t response[4] = { 0 };
    int retries = cfg->rx_retries;
    esp_err_t rx_err = ESP_FAIL;
    while( retries-- > 0 && rx_err != ESP_OK )
    {
        rx_err = core2foraws_i2c_read( CORE2FORAWS_I2C_INTERNAL, _atecc_dev,
            CORE2FORAWS_I2C_NO_REG, response, sizeof( response ) );
    }

    if( rx_err != ESP_OK )
    {
        return ATCA_WAKE_FAILED;
    }

    return hal_check_wake( response, 4 );
}

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
    /*
     * During the wake sequence, CryptoAuthLib temporarily sets the iface
     * config address to 0x00 (general call) before calling atsend().
     * Detect this and delegate to the wake helper so the write actually
     * goes to address 0x00 instead of the static _atecc_dev handle.
     */
    if( iface && iface->mIfaceCFG )
    {
        uint16_t cur_addr = ATCA_IFACECFG_I2C_ADDRESS( iface->mIfaceCFG );
        if( cur_addr == 0x00 )
        {
            /* Wake pulse — drive SDA low via GPIO instead of I2C driver,
             * serialized against other devices on the shared internal bus */
            return _atecc_wake_pulse() == ESP_OK ? ATCA_SUCCESS
                                                  : ATCA_COMM_FAIL;
        }
    }

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
    (void) paramlen;

    if( iface && iface->mIfaceCFG )
    {
        switch( option )
        {
            case ATCA_HAL_CONTROL_WAKE:
                return _atecc_wake( iface );
            case ATCA_HAL_CHANGE_BAUD:
                return __wrap_hal_i2c_change_baud( iface,
                    ( param != NULL ) ? *( uint32_t * )param : 0 );
            default:
                return ATCA_UNIMPLEMENTED;
        }
    }
    return ATCA_BAD_PARAM;
}
