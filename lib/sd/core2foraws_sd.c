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
 * @file core2foraws_sd.c
 * @brief Core2 for AWS IoT Kit SD card hardware driver APIs
 */

#include <string.h>
#include <stdatomic.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_log.h>

#include "core2foraws_sd.h"
#include "core2foraws_common.h"

static sdmmc_card_t* _sd_card;
static const char *_mount_path = "/sd_card";
static size_t _mount_path_len;
static StaticSemaphore_t _sd_mutex_storage;
static SemaphoreHandle_t _sd_mutex = NULL;
static atomic_uchar _sd_mutex_state;

#define SD_IO_CHUNK_SIZE 4096U

static const char *_TAG = "CORE2FORAWS_SD";

static esp_err_t _sd_lock( void )
{
    if( atomic_load( &_sd_mutex_state ) != 2 )
    {
        unsigned char expected = 0;
        if( atomic_compare_exchange_strong( &_sd_mutex_state, &expected, 1 ) )
        {
            _sd_mutex = xSemaphoreCreateMutexStatic( &_sd_mutex_storage );
            atomic_store( &_sd_mutex_state, _sd_mutex != NULL ? 2 : 0 );
        }
        else
        {
            while( atomic_load( &_sd_mutex_state ) == 1 ) taskYIELD();
        }
    }

    if( _sd_mutex == NULL ) return ESP_ERR_NO_MEM;
    return xSemaphoreTake( _sd_mutex, portMAX_DELAY ) == pdTRUE
               ? ESP_OK
               : ESP_ERR_TIMEOUT;
}

static void _sd_unlock( void )
{
    xSemaphoreGive( _sd_mutex );
}

/**
 * @brief The SPI2 peripheral that controls the SPI bus.
 *
 * This is peripheral type used for initializing the SPI bus.
 * There are two peripherals connected to the SPI bus, the
 * ILI9342C display controller and the TF/SD card slot.
 *
 */
/* @[declare_spi_host_use] */
#define SPI_HOST_USE HSPI_HOST
/* @[declare_spi_host_use] */

/* SD card SPI bus pins (shared with the LCD, see schema.yml). These must
 * match the pins the display driver uses so both devices share one bus. */
#define SD_SPI_MOSI GPIO_NUM_23
#define SD_SPI_MISO GPIO_NUM_38
#define SD_SPI_SCLK GPIO_NUM_18
#define SD_SPI_CS   GPIO_NUM_4

esp_err_t core2foraws_sd_mount( void )
{
    esp_err_t err = core2foraws_common_spi_bus_init();
    if( err != ESP_OK ) return err;

    err = _sd_lock();
    if( err != ESP_OK ) return err;

    /* Already mounted — nothing to do. */
    if( _sd_card != NULL )
    {
        _sd_unlock();
        return ESP_OK;
    }

    _mount_path_len = strlen( _mount_path );

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 512
    };
    sdmmc_card_t *card;

    host.slot = SPI_HOST_USE;

#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 1, 0)
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.host_id = host.slot;
#else
    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
#endif
    slot_config.gpio_cs = SD_SPI_CS;
    xSemaphoreTake( core2foraws_common_spi_semaphore, portMAX_DELAY );
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL( 4, 1, 0 )
    err = esp_vfs_fat_sdspi_mount( _mount_path, &host, &slot_config, &mount_config, &card );
#else
    err = esp_vfs_fat_sdmmc_mount( _mount_path, &host, &slot_config, &mount_config, &card );
#endif
    xSemaphoreGive( core2foraws_common_spi_semaphore );
    if ( err == ESP_OK )
    {
        ESP_LOGI( _TAG, "Mounted SD card %s at mount point %s", card->cid.name, _mount_path );
        _sd_card = card;
    }
    else
    {
        ESP_LOGE( _TAG, "Failed to mount SD card at %s: 0x%x (%s)",
                  _mount_path, err, esp_err_to_name( err ) );
    }
    
    _sd_unlock();
    return err;
}

esp_err_t core2foraws_sd_read( const char *file_name, char *message, size_t to_read_length )
{
    esp_err_t err = ESP_OK;
    FILE *f = NULL;
    char *path = NULL;

    if( file_name == NULL || message == NULL || to_read_length == 0 )
    {
        return ESP_ERR_INVALID_ARG;
    }

    err = _sd_lock();
    if( err != ESP_OK ) return err;

    if( _sd_card == NULL )
    {
        _sd_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    const size_t file_name_len = strlen( file_name );
    path = malloc( _mount_path_len + file_name_len + 1 );
    if( path == NULL )
    {
        _sd_unlock();
        return ESP_ERR_NO_MEM;
    }

    memcpy( path, _mount_path, _mount_path_len );
    memcpy( path + _mount_path_len, file_name, file_name_len + 1 );
    
    xSemaphoreTake( core2foraws_common_spi_semaphore, portMAX_DELAY );
    f = fopen( path, "r" );
    xSemaphoreGive( core2foraws_common_spi_semaphore );
    if ( f == NULL )
    {
        err = ESP_FAIL;
        ESP_LOGE( _TAG, "Failed to open SD card path %s", path );
        goto cleanup;
    }

    size_t bytes_read = 0;
    size_t remaining = to_read_length - 1;
    while( remaining > 0 )
    {
        size_t chunk = remaining < SD_IO_CHUNK_SIZE
                           ? remaining
                           : SD_IO_CHUNK_SIZE;
        xSemaphoreTake( core2foraws_common_spi_semaphore, portMAX_DELAY );
        size_t got = fread( message + bytes_read, 1, chunk, f );
        bool read_error = ferror( f );
        xSemaphoreGive( core2foraws_common_spi_semaphore );
        bytes_read += got;
        remaining -= got;
        if( read_error )
        {
            err = ESP_FAIL;
            ESP_LOGE( _TAG, "Failed to read from SD card" );
            break;
        }
        if( got < chunk ) break;
    }
    message[ bytes_read ] = '\0';
    ESP_LOGV( _TAG, "Read %u bytes from %s", ( unsigned int )bytes_read,
              path );

cleanup:
    if( f != NULL )
    {
        xSemaphoreTake( core2foraws_common_spi_semaphore, portMAX_DELAY );
        fclose( f );
        xSemaphoreGive( core2foraws_common_spi_semaphore );
    }

    free( path );
    _sd_unlock();

    return err;
}

esp_err_t core2foraws_sd_write( const char *file_name, const char* message, size_t *wrote_length )
{
    esp_err_t err = ESP_OK;
    FILE *f = NULL;
    char *path = NULL;

    if( file_name == NULL || message == NULL || wrote_length == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }
    *wrote_length = 0;

    err = _sd_lock();
    if( err != ESP_OK ) return err;

    /* The SD card must be mounted before writing. */
    if( _sd_card == NULL )
    {
        _sd_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    const size_t file_name_len = strlen( file_name );
    path = malloc( _mount_path_len + file_name_len + 1 );
    if( path == NULL )
    {
        _sd_unlock();
        return ESP_ERR_NO_MEM;
    }

    memcpy( path, _mount_path, _mount_path_len );
    memcpy( path + _mount_path_len, file_name, file_name_len + 1 );

    xSemaphoreTake( core2foraws_common_spi_semaphore, portMAX_DELAY );
    f = fopen(path, "w");
    xSemaphoreGive( core2foraws_common_spi_semaphore );
    if (f == NULL) {
        err = ESP_FAIL;
        ESP_LOGE( _TAG, "Failed to open SD card path %s for writing", path );
        *wrote_length = 0;
        goto cleanup;
    }

    size_t message_length = strlen( message );
    while( *wrote_length < message_length )
    {
        size_t remaining = message_length - *wrote_length;
        size_t chunk = remaining < SD_IO_CHUNK_SIZE
                           ? remaining
                           : SD_IO_CHUNK_SIZE;
        xSemaphoreTake( core2foraws_common_spi_semaphore, portMAX_DELAY );
        size_t wrote = fwrite( message + *wrote_length, 1, chunk, f );
        bool write_error = ferror( f );
        xSemaphoreGive( core2foraws_common_spi_semaphore );
        *wrote_length += wrote;
        if( write_error || wrote != chunk )
        {
            err = ESP_FAIL;
            ESP_LOGE( _TAG, "Failed to write to SD card" );
            break;
        }
    }
    ESP_LOGV( _TAG, "Wrote %u bytes to %s",
              ( unsigned int )*wrote_length, path );

cleanup:
    if( f != NULL )
    {
        xSemaphoreTake( core2foraws_common_spi_semaphore, portMAX_DELAY );
        fclose( f );
        xSemaphoreGive( core2foraws_common_spi_semaphore );
    }

    free( path );
    _sd_unlock();

    return err;
}

esp_err_t core2foraws_sd_unmount( void )
{
    esp_err_t err = _sd_lock();
    if( err != ESP_OK ) return err;

    /* Nothing to unmount if no card is mounted. */
    if( _sd_card == NULL )
    {
        _sd_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake( core2foraws_common_spi_semaphore, portMAX_DELAY );
    err = esp_vfs_fat_sdcard_unmount( _mount_path, _sd_card );
    xSemaphoreGive( core2foraws_common_spi_semaphore );
    
    if ( err == ESP_OK )
    {
        _sd_card = NULL;
    }

    _sd_unlock();
    return err;
}