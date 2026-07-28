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
 * @file core2foraws_common.c
 * @brief Core2 for AWS IoT Kit helper library used across BSP drivers
 */

#include <stdint.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/spi_master.h>

#include "core2foraws_common.h"

SemaphoreHandle_t core2foraws_common_spi_semaphore = NULL;

static StaticSemaphore_t _spi_semaphore_storage;
static atomic_uchar _spi_semaphore_state;
static atomic_uchar _spi_bus_state;

enum
{
    COMMON_RESOURCE_UNINITIALIZED = 0,
    COMMON_RESOURCE_INITIALIZING,
    COMMON_RESOURCE_READY,
};

#define SHARED_SPI_HOST SPI2_HOST
#define SHARED_SPI_MOSI GPIO_NUM_23
#define SHARED_SPI_MISO GPIO_NUM_38
#define SHARED_SPI_SCLK GPIO_NUM_18
#define SHARED_SPI_MAX_TRANSFER_BYTES ( 320 * 50 * sizeof( uint16_t ) )

static const char *_TAG = "CORE2FORAWS_COMMON";

esp_err_t core2foraws_common_spi_semaphore_init( void )
{
    if( atomic_load( &_spi_semaphore_state ) == COMMON_RESOURCE_READY )
    {
        return ESP_OK;
    }

    unsigned char expected = COMMON_RESOURCE_UNINITIALIZED;
    if( atomic_compare_exchange_strong( &_spi_semaphore_state, &expected,
                                        COMMON_RESOURCE_INITIALIZING ) )
    {
        core2foraws_common_spi_semaphore =
            xSemaphoreCreateBinaryStatic( &_spi_semaphore_storage );
        if( core2foraws_common_spi_semaphore == NULL )
        {
            atomic_store( &_spi_semaphore_state,
                          COMMON_RESOURCE_UNINITIALIZED );
            return ESP_ERR_NO_MEM;
        }

        xSemaphoreGive( core2foraws_common_spi_semaphore );
        atomic_store( &_spi_semaphore_state, COMMON_RESOURCE_READY );
        return ESP_OK;
    }

    while( atomic_load( &_spi_semaphore_state ) ==
           COMMON_RESOURCE_INITIALIZING )
    {
        taskYIELD();
    }

    return atomic_load( &_spi_semaphore_state ) == COMMON_RESOURCE_READY
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}

esp_err_t core2foraws_common_spi_bus_init( void )
{
    esp_err_t err = core2foraws_common_spi_semaphore_init();
    if( err != ESP_OK )
    {
        return err;
    }

    if( atomic_load( &_spi_bus_state ) == COMMON_RESOURCE_READY )
    {
        return ESP_OK;
    }

    unsigned char expected = COMMON_RESOURCE_UNINITIALIZED;
    if( atomic_compare_exchange_strong( &_spi_bus_state, &expected,
                                        COMMON_RESOURCE_INITIALIZING ) )
    {
        const spi_bus_config_t bus_cfg = {
            .mosi_io_num = SHARED_SPI_MOSI,
            .miso_io_num = SHARED_SPI_MISO,
            .sclk_io_num = SHARED_SPI_SCLK,
            .quadwp_io_num = GPIO_NUM_NC,
            .quadhd_io_num = GPIO_NUM_NC,
            .max_transfer_sz = SHARED_SPI_MAX_TRANSFER_BYTES,
        };

        err = spi_bus_initialize( SHARED_SPI_HOST, &bus_cfg,
                                  SPI_DMA_CH_AUTO );
        atomic_store( &_spi_bus_state,
                      err == ESP_OK ? COMMON_RESOURCE_READY
                                    : COMMON_RESOURCE_UNINITIALIZED );
        return err;
    }

    while( atomic_load( &_spi_bus_state ) == COMMON_RESOURCE_INITIALIZING )
    {
        taskYIELD();
    }

    return atomic_load( &_spi_bus_state ) == COMMON_RESOURCE_READY
               ? ESP_OK
               : ESP_FAIL;
}

esp_err_t core2foraws_common_error( int32_t error_code )
{
    ESP_LOGV( _TAG, "Original error code: %" PRId32, error_code );

    return ( error_code == 0 ) ? ESP_OK : ESP_FAIL;
}

esp_err_t core2foraws_common_task_stack_watermark( const char *tag,
                                                   TaskHandle_t task,
                                                   size_t *watermark_bytes )
{
    if( watermark_bytes == NULL )
    {
        return ESP_ERR_INVALID_ARG;
    }

    /* uxTaskGetStackHighWaterMark() reports the minimum free stack in words
       (StackType_t units); scale to bytes for a human-readable figure. */
    *watermark_bytes = ( size_t ) uxTaskGetStackHighWaterMark( task ) *
                       sizeof( StackType_t );
    const char *name = pcTaskGetName( task );

    ESP_LOGI( tag != NULL ? tag : _TAG,
              "Task '%s' minimum free stack: %u bytes",
              name != NULL ? name : "self",
              ( unsigned int ) *watermark_bytes );

    return ESP_OK;
}