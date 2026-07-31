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
 * @file core2foraws_crypto.c
 * @brief Core2 for AWS IoT Kit cryptographic hardware driver APIs
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <mbedtls/atca_mbedtls_wrap.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/pk.h>
#include <esp_log.h>

#include "cryptoauthlib.h"
#include "core2foraws_common.h"
#include "core2foraws_crypto.h"

#define ATECC608_I2C_ADDRESS_8BIT 0x6A
#define ATECC608_I2C_BAUD_HZ      100000

static mbedtls_entropy_context _entropy;
static mbedtls_ctr_drbg_context _ctr_drbg;
static ATCAIfaceCfg _crypto_iface_cfg;

static const char *_TAG = "CORE2FORAWS_CRYPTO";
static bool _crypto_initialized = false;

static int _configure_mbedtls_rng( void );
static void _close_mbedtls_rng( void );

static esp_err_t _configure_mbedtls_rng( void )
{
    int err;
    const char * seed = "\tAWS IoT Kit random seed string";
    mbedtls_ctr_drbg_init( &_ctr_drbg );

    ESP_LOGD(_TAG, "\tSeeding mbedTLS random number generator");

    mbedtls_entropy_init( &_entropy );
    err = mbedtls_ctr_drbg_seed( &_ctr_drbg, mbedtls_entropy_func, &_entropy,
        ( const unsigned char * )seed, strlen( seed ) );
    
    if ( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "\tFailed to seed mbedTLS ring. 'mbedtls_ctr_drbg_seed' returned %d", err );
    } 
    else 
    {
        ESP_LOGD( _TAG, "\tSuccessfully seeded mbedTLS ring" );
    }

    return err;
}

static void _close_mbedtls_rng( void )
{
    fflush( stdout );
    mbedtls_ctr_drbg_free( &_ctr_drbg );
    mbedtls_entropy_free( &_entropy );
    ESP_LOGD( _TAG, "\tClosed mbedTLS ring" );
}

esp_err_t core2foraws_crypto_init( void )
{
    ESP_LOGI( _TAG, "\tInitializing" );

    if ( _crypto_initialized )
    {
        return ESP_OK;
    }

    if ( _configure_mbedtls_rng() != ESP_OK )
    {
        return ESP_FAIL;
    }

    _crypto_iface_cfg = cfg_ateccx08a_i2c_default;
#ifdef ATCA_ENABLE_DEPRECATED
    _crypto_iface_cfg.atcai2c.slave_address = ATECC608_I2C_ADDRESS_8BIT;
#else
    _crypto_iface_cfg.atcai2c.address = ATECC608_I2C_ADDRESS_8BIT;
#endif
    _crypto_iface_cfg.atcai2c.baud = ATECC608_I2C_BAUD_HZ;

    ATCA_STATUS err = atcab_init( &_crypto_iface_cfg );
    
    if ( err != ATCA_SUCCESS ) 
    {
        ESP_LOGE( _TAG, "\tFailed to initialize ATECC608. atcab_init returned %x", err );
        _close_mbedtls_rng();
        return core2foraws_common_error( err );
    }

    _crypto_initialized = true;
    ESP_LOGD( _TAG, "\tSuccessfully initialized ATECC608" );
    
    return core2foraws_common_error( err ); 
}

esp_err_t core2foraws_crypto_serial_get( char *serial_number )
{
    if ( serial_number == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t serial[ ATCA_SERIAL_NUM_SIZE ];

    ATCA_STATUS err = atcab_read_serial_number( serial );
    
    if ( err != ATCA_SUCCESS )
    {
        ESP_LOGE( _TAG, "\tFailed to read ATECC608 serial number. atcab_read_serial_number returned %x", err );
    }
    else
    {
        for ( size_t i = 0; i < ATCA_SERIAL_NUM_SIZE; i++ )
        {
            snprintf( serial_number + i * 2, 3, "%02X", serial[ i ] );
        }

        serial_number[ CRYPTO_SERIAL_STR_SIZE - 1 ] = '\0';
    }

    return core2foraws_common_error( err );

}

esp_err_t core2foraws_crypto_pubkey_base64_get( char *public_key )
{
    if ( public_key == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t buf_len = CRYPTO_PUB_KEY_SIZE;
    uint8_t buf[ buf_len ];
    uint8_t * tmp;

    static const uint8_t public_key_x509_header[] = 
    {
        0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A,
        0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04
    };

    size_t public_key_x509_header_len = sizeof( public_key_x509_header );
    uint8_t pubkey[ ATCA_PUB_KEY_SIZE ];

    ATCA_STATUS err = atcab_get_pubkey( 0, pubkey );
    if ( err != ATCA_SUCCESS )
    {
        ESP_LOGE( _TAG, "\tFailed to get public key from ATECC608. atcab_get_pubkey returned %x", err );
        return core2foraws_common_error( err );
    }

    /* Calculate where the raw data will fit into the buffer */
    tmp = buf + sizeof( buf ) - ATCA_PUB_KEY_SIZE - public_key_x509_header_len;
    /* Copy the header */
    memcpy( tmp, public_key_x509_header, public_key_x509_header_len);

    /* Copy the key bytes */
    memcpy( tmp + public_key_x509_header_len, pubkey, ATCA_PUB_KEY_SIZE );

    /* Convert to base 64 */
    err = atcab_base64encode( tmp, ATCA_PUB_KEY_SIZE + public_key_x509_header_len, ( char * )buf, &buf_len );
    if ( err != ATCA_SUCCESS )
    {
        ESP_LOGE( _TAG, "\tFailed to base64 encode public key. atcab_base64encode returned %x", err );
        return core2foraws_common_error( err );
    }

    if ( buf_len >= CRYPTO_PUB_KEY_SIZE )
    {
        return ESP_FAIL;
    }
 
    memcpy( public_key, buf, buf_len );
    
    /* Add a null terminator */
    public_key[ buf_len ] = '\0';

    return core2foraws_common_error( err );
}

esp_err_t core2foraws_crypto_sha256_sign( const unsigned char *message, uint8_t *signature, size_t *signature_length )
{
    if ( message == NULL || signature == NULL || signature_length == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    int mbed_err = 0;
    mbedtls_pk_context pkey;
    mbedtls_pk_init( &pkey );

    mbed_err = atca_mbedtls_pk_init( &pkey, 0 );
    if ( mbed_err != 0 )
    {
        ESP_LOGE( _TAG, "\tFailed to initialize private key access from ATECC608. atca_mbedtls_pk_init returned -0x%x", -mbed_err );
        return core2foraws_common_error( mbed_err );
    }

    mbed_err = mbedtls_pk_sign( &pkey, MBEDTLS_MD_SHA256, message, 0, signature, MBEDTLS_PK_SIGNATURE_MAX_SIZE, signature_length, mbedtls_ctr_drbg_random, &_ctr_drbg );
    mbedtls_pk_free( &pkey );

    if (mbed_err != 0) {
        ESP_LOGE( _TAG, "\tFailed to sign message with ATECC608 private key. mbedtls_pk_sign returned -0x%x", -mbed_err );
    }

    return core2foraws_common_error( mbed_err );

}

esp_err_t core2foraws_crypto_sha256_verify( const unsigned char *message, const uint8_t *signature, const size_t signature_length, bool *verified )
{
    if ( message == NULL || signature == NULL || verified == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    int mbed_err = 0;
    mbedtls_pk_context pkey;
    *verified = false;
    mbedtls_pk_init( &pkey );

    mbed_err = atca_mbedtls_pk_init( &pkey, 0 );
    if ( mbed_err != 0 )
    {
        ESP_LOGE( _TAG, "\tFailed to initialize private key access from ATECC608. atca_mbedtls_pk_init returned -0x%x", -mbed_err );
        return core2foraws_common_error( mbed_err );
    }

    mbed_err = mbedtls_pk_verify( &pkey, MBEDTLS_MD_SHA256, message, 0, signature, signature_length );
    mbedtls_pk_free( &pkey );
    
    if ( mbed_err != 0 )
    {
        ESP_LOGE( _TAG, "\tFailed to verify message with ATECC608 private key. mbedtls_pk_verify returned -0x%x", -mbed_err );
    }
    else
    {
        *verified = true;
    }

    return core2foraws_common_error( mbed_err );
}