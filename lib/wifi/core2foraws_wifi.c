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

/* This library referenced the Espressif Systems (Shanghai) PTE LTD's, Public Domain
 * provisioning example:
 * https://github.com/espressif/esp-idf/tree/release/v4.3/examples/provisioning/wifi_prov_mgr
*/

/**
 * @file core2foraws_wifi.c
 * @brief Core2 for AWS IoT Kit Wi-Fi helper APIs
 */

#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>

#include "qrcode.h"

#include "core2foraws_wifi.h"
#include "core2foraws_common.h"

#define PROV_QR_VERSION "v1"
#define PROV_TRANSPORT  "ble"
#define PROV_POP_STR_SIZE    9
#define QRCODE_BASE_URL "https://espressif.github.io/esp-jumpstart/qrcode.html"

static const char *_TAG = "CORE2FORAWS_WIFI";

EventGroupHandle_t wifi_event_group = NULL;

static esp_netif_t *_wifi_netif = NULL;
static SemaphoreHandle_t _service_name_mutex = NULL;
static bool _wifi_initialized = false;
static atomic_bool _wifi_started;
static atomic_bool _provisioning_active;
static StaticSemaphore_t _wifi_lifecycle_mutex_storage;
static SemaphoreHandle_t _wifi_lifecycle_mutex = NULL;
static atomic_uchar _wifi_lifecycle_mutex_state;
static char service_name[ 19 ];

static esp_err_t _get_pop( char *pop, size_t pop_size );
static void _on_prov_event_handler( void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data );
static void _on_got_ip( void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data );
static void _on_wifi_start( void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data );
static void _on_wifi_connect( void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data );
static void _on_wifi_disconnect( void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data );
static esp_err_t _device_service_name_set( void );
static esp_err_t _wifi_prov_qr_print( void );
static esp_err_t _core2foraws_wifi_start_locked( void );
static esp_err_t _core2foraws_wifi_deinit_locked( void );

static esp_err_t _wifi_lifecycle_lock( void )
{
    if( atomic_load( &_wifi_lifecycle_mutex_state ) != 2 )
    {
        unsigned char expected = 0;
        if( atomic_compare_exchange_strong( &_wifi_lifecycle_mutex_state,
                                            &expected, 1 ) )
        {
            _wifi_lifecycle_mutex = xSemaphoreCreateMutexStatic(
                &_wifi_lifecycle_mutex_storage );
            atomic_store( &_wifi_lifecycle_mutex_state,
                          _wifi_lifecycle_mutex != NULL ? 2 : 0 );
        }
        else
        {
            while( atomic_load( &_wifi_lifecycle_mutex_state ) == 1 )
                taskYIELD();
        }
    }

    if( _wifi_lifecycle_mutex == NULL ) return ESP_ERR_NO_MEM;
    return xSemaphoreTake( _wifi_lifecycle_mutex, pdMS_TO_TICKS( 1000 ) ) ==
                   pdTRUE
               ? ESP_OK
               : ESP_ERR_TIMEOUT;
}

static void _wifi_lifecycle_unlock( void )
{
    xSemaphoreGive( _wifi_lifecycle_mutex );
}

static esp_err_t _get_pop( char *pop, size_t pop_size )
{
    uint8_t eth_mac[ 6 ];
    esp_err_t err = esp_wifi_get_mac( WIFI_IF_STA, eth_mac );
    if ( err == ESP_OK )
    {
        snprintf( pop, pop_size, "%02x%02x%02x%02x", eth_mac[ 2 ], eth_mac[ 3 ], eth_mac[ 4 ], eth_mac[ 5 ] );
        return ESP_OK;
    }

    ESP_LOGE( _TAG, "Failed to get MAC address to generate PoP: 0x%x", err );
    return err;
}

static void _on_prov_event_handler( void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data )
{
    static uint8_t _retries;
    
    if ( event_id == WIFI_PROV_START )
        ESP_LOGI( _TAG, "\tProvisioning started" );
    else if ( event_id == WIFI_PROV_CRED_RECV )
    {
        wifi_sta_config_t *wifi_sta_cfg = ( wifi_sta_config_t * )event_data;
        ESP_LOGI( _TAG, "\tReceived Wi-Fi credentials"
                    "\n\tSSID     : %s",
                    ( const char * ) wifi_sta_cfg->ssid );
        /* Never log the plaintext password. Log its length only so the
           credential delivery can still be debugged without leaking the
           secret over the UART console. */
        ESP_LOGD( _TAG, "\tPassword : (%u characters received)",
                    ( unsigned int ) strlen( ( const char * ) wifi_sta_cfg->password ) );
    }
    else if ( event_id == WIFI_PROV_CRED_FAIL )
    {
        wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
        ESP_LOGE( _TAG, "\tProvisioning failed!\n\tReason : %s"
                    "\n\tWi-Fi will erase the credentials and restart provisioning in %d retries",
                    ( *reason == WIFI_PROV_STA_AUTH_ERROR ) ?
                    "Wi-Fi station authentication failed" : "Wi-Fi access-point not found",
                    WIFI_RETRIES_MAX_FAILS - _retries );
        _retries++;
        if ( _retries >= WIFI_RETRIES_MAX_FAILS )
        {
            ESP_LOGI( _TAG, "\tFailed to connect with provisioned AP, reseting provisioned credentials" );
            wifi_prov_mgr_reset_sm_state_on_failure();
            _retries = 0;
        }
    }
    else if ( event_id == WIFI_PROV_CRED_SUCCESS )
    {
        ESP_LOGI( _TAG, "\tProvisioning successful");
        _retries = 0;
    }
    else if ( event_id == WIFI_PROV_END )
    {
        if( atomic_exchange( &_provisioning_active, false ) )
        {
            wifi_prov_mgr_deinit();
        }
    }
}

static void _on_got_ip( void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data )
{
    if ( event_id == IP_EVENT_STA_GOT_IP )
    {
        ip_event_got_ip_t *event = ( ip_event_got_ip_t * )event_data;

        ESP_LOGI( _TAG, "\tGot IPv4 address: " IPSTR, IP2STR( &event->ip_info.ip ) );

        xEventGroupClearBits( wifi_event_group, WIFI_DISCONNECTED_BIT );
        xEventGroupClearBits( wifi_event_group, WIFI_CONNECTING_BIT );
        xEventGroupSetBits( wifi_event_group, WIFI_CONNECTED_BIT );
    }
}

static void _on_wifi_start( void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data )
{
    ESP_LOGI( _TAG, "\tStarting Wi-Fi... %ld", (long)event_id );
    xEventGroupSetBits( wifi_event_group, WIFI_DISCONNECTED_BIT );
    esp_wifi_connect();
}

static void _on_wifi_connect( void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data )
{
    ESP_LOGI( _TAG, "\tConnected" );
}

static void _on_wifi_disconnect( void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data )
{
    ESP_LOGI( _TAG, "\tDisconnected, attempting to reconnect..." );
    xEventGroupClearBits( wifi_event_group, WIFI_CONNECTED_BIT );
    xEventGroupSetBits( wifi_event_group, WIFI_DISCONNECTED_BIT );

    esp_err_t err = esp_wifi_connect();
    if ( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "\tReconnect failed: 0x%x", err );
        return;
    }

    xEventGroupSetBits( wifi_event_group, WIFI_CONNECTING_BIT );
}

static esp_err_t _device_service_name_set( void )
{
    uint8_t eth_mac[ 6 ];
    const char *ssid_prefix = "CORE2FORAWS_";
    esp_err_t err = esp_wifi_get_mac( WIFI_IF_STA, eth_mac );
    if ( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "Failed to get MAC address to set service name: 0x%x", err );
        return err;
    }

    if ( xSemaphoreTake( _service_name_mutex, pdMS_TO_TICKS( 40 ) ) == pdTRUE )
    {
        snprintf( service_name, sizeof( service_name ), "%s%02X%02X%02X",
                ssid_prefix, eth_mac[ 3 ], eth_mac[ 4 ], eth_mac[ 5 ] );
        xSemaphoreGive( _service_name_mutex );
        return ESP_OK;
    }

    ESP_LOGE( _TAG, "Failed to set service name." );
    return ESP_ERR_TIMEOUT;
}

static esp_err_t _wifi_prov_qr_print( void )
{
    char provisioning_payload[ WIFI_PROV_STR_LEN ] = { 0 };
    esp_err_t err = core2foraws_wifi_prov_str_get( provisioning_payload );

    if ( err == ESP_OK )
    {
        ESP_LOGI( _TAG, "\tScan this QR code from the provisioning application for Wi-Fi provisioning." );
        esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
        err = esp_qrcode_generate( &cfg, provisioning_payload );
        ESP_LOGI( _TAG, "\tIf QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s", QRCODE_BASE_URL, provisioning_payload);
    }

    return err;
}

esp_err_t core2foraws_wifi_init( void )
{
    if ( _wifi_initialized )
    {
        return ESP_OK;
    }

    bool ip_handler_registered = false;
    bool wifi_start_handler_registered = false;
    bool wifi_connected_handler_registered = false;
    bool wifi_disconnected_handler_registered = false;
    bool provisioning_handler_registered = false;

    esp_err_t err = nvs_flash_init();
    if ( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "NVS initialization failed without erasing application data: 0x%x", err );
        return err;
    }

    ESP_LOGI( _TAG, "\tInitializing" );

    /* Initialize TCP/IP */
    err = esp_netif_init();
    if ( err != ESP_OK )
    {
        ESP_LOGE( _TAG, "TCP/IP initialization failed: 0x%x", err );
        return err;
    }

    /* Initialize the event loop */
    err = esp_event_loop_create_default();
    if ( err != ESP_OK && err != ESP_ERR_INVALID_STATE )
    {
        ESP_LOGE( _TAG, "Default event loop initialization failed: 0x%x", err );
        return err;
    }

    wifi_event_group = xEventGroupCreate();
    if ( wifi_event_group == NULL )
    {
        return ESP_ERR_NO_MEM;
    }

    /* Initialize Wi-Fi including netif with default config */
    _wifi_netif = esp_netif_create_default_wifi_sta();
    if ( _wifi_netif == NULL )
    {
        err = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    err = esp_event_handler_register( IP_EVENT, ESP_EVENT_ANY_ID, &_on_got_ip, NULL );
    if ( err != ESP_OK ) goto cleanup;
    ip_handler_registered = true;

    err = esp_event_handler_register( WIFI_EVENT, WIFI_EVENT_STA_START, &_on_wifi_start, _wifi_netif );
    if ( err != ESP_OK ) goto cleanup;
    wifi_start_handler_registered = true;

    err = esp_event_handler_register( WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &_on_wifi_connect, _wifi_netif );
    if ( err != ESP_OK ) goto cleanup;
    wifi_connected_handler_registered = true;

    err = esp_event_handler_register( WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &_on_wifi_disconnect, NULL );
    if ( err != ESP_OK ) goto cleanup;
    wifi_disconnected_handler_registered = true;

    err = esp_event_handler_register( WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &_on_prov_event_handler, NULL );
    if ( err != ESP_OK ) goto cleanup;
    provisioning_handler_registered = true;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init( &cfg );
    if ( err != ESP_OK ) goto cleanup;

    _wifi_initialized = true;
    ESP_LOGD( _TAG, "Initialized" );

    return ESP_OK;

cleanup:
    ESP_LOGE( _TAG, "Wi-Fi initialization failed: 0x%x", err );

    if ( provisioning_handler_registered )
        esp_event_handler_unregister( WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &_on_prov_event_handler );
    if ( wifi_disconnected_handler_registered )
        esp_event_handler_unregister( WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &_on_wifi_disconnect );
    if ( wifi_connected_handler_registered )
        esp_event_handler_unregister( WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &_on_wifi_connect );
    if ( wifi_start_handler_registered )
        esp_event_handler_unregister( WIFI_EVENT, WIFI_EVENT_STA_START, &_on_wifi_start );
    if ( ip_handler_registered )
        esp_event_handler_unregister( IP_EVENT, ESP_EVENT_ANY_ID, &_on_got_ip );

    if ( _wifi_netif != NULL )
    {
        esp_wifi_clear_default_wifi_driver_and_handlers( _wifi_netif );
        esp_netif_destroy( _wifi_netif );
        _wifi_netif = NULL;
    }

    vEventGroupDelete( wifi_event_group );
    wifi_event_group = NULL;

    return err;
}

static esp_err_t _core2foraws_wifi_start_locked( void )
{
    if ( !_wifi_initialized )
    {
        return ESP_ERR_INVALID_STATE;
    }
    if( atomic_load( &_wifi_started ) )
    {
        return ESP_OK;
    }

    /* Configuration for the provisioning manager */
    wifi_prov_mgr_config_t config = 
    {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };

    /* Initialize provisioning manager with the
     * configuration parameters set above */
    esp_err_t err = wifi_prov_mgr_init( config );
    if ( err != ESP_OK )
    {
        return err;
    }

    bool wifi_is_provisioned = false;
    /* Let's find out if the device is provisioned */
    err = wifi_prov_mgr_is_provisioned( &wifi_is_provisioned );
    if ( err != ESP_OK )
    {
        wifi_prov_mgr_deinit();
        return err;
    }

    if ( _service_name_mutex == NULL )
    {
        _service_name_mutex = xSemaphoreCreateMutex();
        if ( _service_name_mutex == NULL )
        {
            wifi_prov_mgr_deinit();
            return ESP_ERR_NO_MEM;
        }
    }

    err = _device_service_name_set();
    if ( err != ESP_OK )
    {
        wifi_prov_mgr_deinit();
        return err;
    }
    ESP_LOGD( _TAG, "\tService Name: %s", service_name );

    /* If device is not yet provisioned start provisioning service */
    if ( !wifi_is_provisioned )
    {
        ESP_LOGI( _TAG, "\tStarting Wi-Fi provisioning over BLE" );

        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

        const char *service_key = NULL;

        /* This step is only useful when scheme is wifi_prov_scheme_ble. This will
         * set a custom 128 bit UUID which will be included in the BLE advertisement
         * and will correspond to the primary GATT service that provides provisioning
         * endpoints as GATT characteristics. Each GATT characteristic will be
         * formed using the primary service UUID as base, with different auto assigned
         * 12th and 13th bytes (assume counting starts from 0th byte). The client side
         * applications must identify the endpoints by reading the User Characteristic
         * Description descriptor (0x2901) for each characteristic, which contains the
         * endpoint name of the characteristic */
        uint8_t custom_service_uuid[] = 
        {
            /* LSB <---------------------------------------
             * ---------------------------------------> MSB */
            0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
            0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
        };
        err = wifi_prov_scheme_ble_set_service_uuid( custom_service_uuid );
        if ( err != ESP_OK )
        {
            ESP_LOGE( _TAG, "\tFailed to set BLE service UUID: 0x%x", err );
            wifi_prov_mgr_deinit();
            return err;
        }

        if ( xSemaphoreTake( _service_name_mutex, portMAX_DELAY ) == pdTRUE )
        {
            char pop[ PROV_POP_STR_SIZE ];
            err = _get_pop( pop, sizeof( pop ) );
            if ( err == ESP_OK )
            {
                err = wifi_prov_mgr_start_provisioning( security, pop, service_name, service_key );
            }
            xSemaphoreGive( _service_name_mutex );
        }
        else
        {
            wifi_prov_mgr_deinit();
            return ESP_ERR_TIMEOUT;
        }

        if ( err != ESP_OK )
        {
            wifi_prov_mgr_deinit();
            return err;
        }

        atomic_store( &_provisioning_active, true );
        atomic_store( &_wifi_started, true );

        /* Print QR code for provisioning */
        err = _wifi_prov_qr_print();
        if( err != ESP_OK )
        {
            ESP_LOGW( _TAG,
                      "Provisioning started, but QR generation failed: 0x%x",
                      err );
        }
        return ESP_OK;
    }
    else
    {
        ESP_LOGI( _TAG, "\tAlready provisioned, starting Wi-Fi STA");

        wifi_prov_mgr_deinit();
        err = esp_wifi_set_mode( WIFI_MODE_STA );
        if ( err != ESP_OK )
        {
            return err;
        }
        err = esp_wifi_start();
        if( err == ESP_OK )
        {
            atomic_store( &_wifi_started, true );
        }
        return err;
    }
}

esp_err_t core2foraws_wifi_start( void )
{
    esp_err_t err = _wifi_lifecycle_lock();
    if( err != ESP_OK ) return err;
    err = _core2foraws_wifi_start_locked();
    _wifi_lifecycle_unlock();
    return err;
}

static esp_err_t _core2foraws_wifi_deinit_locked( void )
{
    if ( !_wifi_initialized )
    {
        return ESP_OK;
    }

    if( atomic_exchange( &_provisioning_active, false ) )
    {
        wifi_prov_mgr_deinit();
    }

    esp_err_t ret = ESP_OK;
    esp_err_t err = ESP_OK;

    if( atomic_load( &_wifi_started ) )
    {
        err = esp_wifi_stop();
        if( err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED )
        {
            return err;
        }
        atomic_store( &_wifi_started, false );
    }

    err = esp_wifi_deinit();
    if( err != ESP_OK )
    {
        return err;
    }

    err = esp_event_handler_unregister( IP_EVENT, ESP_EVENT_ANY_ID, &_on_got_ip );
    if ( err != ESP_OK && ret == ESP_OK ) ret = err;
    err = esp_event_handler_unregister( WIFI_EVENT, WIFI_EVENT_STA_START, &_on_wifi_start );
    if ( err != ESP_OK && ret == ESP_OK ) ret = err;
    err = esp_event_handler_unregister( WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &_on_wifi_connect );
    if ( err != ESP_OK && ret == ESP_OK ) ret = err;
    err = esp_event_handler_unregister( WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &_on_wifi_disconnect );
    if ( err != ESP_OK && ret == ESP_OK ) ret = err;
    err = esp_event_handler_unregister( WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &_on_prov_event_handler );
    if ( err != ESP_OK && ret == ESP_OK ) ret = err;

    if ( _wifi_netif != NULL )
    {
        err = esp_wifi_clear_default_wifi_driver_and_handlers( _wifi_netif );
        if ( err != ESP_OK && ret == ESP_OK ) ret = err;
        esp_netif_destroy( _wifi_netif );
        _wifi_netif = NULL;
    }

    if ( wifi_event_group != NULL )
    {
        vEventGroupDelete( wifi_event_group );
        wifi_event_group = NULL;
    }

    if ( _service_name_mutex != NULL )
    {
        vSemaphoreDelete( _service_name_mutex );
        _service_name_mutex = NULL;
    }

    _wifi_initialized = false;
    return ret;
}

esp_err_t core2foraws_wifi_deinit( void )
{
    esp_err_t err = _wifi_lifecycle_lock();
    if( err != ESP_OK ) return err;
    err = _core2foraws_wifi_deinit_locked();
    _wifi_lifecycle_unlock();
    return err;
}

esp_err_t core2foraws_wifi_disconnect( void )
{
    return esp_wifi_disconnect();
}

esp_err_t core2foraws_wifi_connect( void )
{
    return esp_wifi_connect();
}

esp_err_t core2foraws_wifi_reset( void )
{
    /* esp_wifi_restore() clears only persistent Wi-Fi configuration. It does
     * not erase the default NVS partition or unrelated application keys. */
    return esp_wifi_restore();
}

esp_err_t core2foraws_wifi_prov_str_get( char *wifi_prov_str )
{
    if ( wifi_prov_str == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ( _service_name_mutex == NULL )
    {
        return ESP_ERR_INVALID_STATE;
    }

    int err = -1;
    if ( xSemaphoreTake( _service_name_mutex, pdMS_TO_TICKS( 40 ) ) == pdTRUE )
    {
        char pop[ PROV_POP_STR_SIZE ];
        esp_err_t pop_err = _get_pop( pop, sizeof( pop ) );
        if ( pop_err == ESP_OK )
        {
            err = snprintf( wifi_prov_str, WIFI_PROV_STR_LEN, "{\"ver\":\"%s\",\"name\":\"%s\"" \
                        ",\"pop\":\"%s\",\"transport\":\"%s\"}",
                        PROV_QR_VERSION, service_name, pop, PROV_TRANSPORT );
        }
        xSemaphoreGive( _service_name_mutex );

        if ( err > 0 )
        {
            ESP_LOGD( _TAG, "Provisioning string generated (%d bytes)", err );
            err = 0;
        }
        else
            ESP_LOGE( _TAG, "\tFailed to write provisioning string." );
    }

    return core2foraws_common_error( err );
}