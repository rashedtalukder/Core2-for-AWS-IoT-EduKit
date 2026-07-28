/*
 * MPU6886 6-axis IMU (accelerometer + gyroscope) driver for the Core2 for AWS IoT Kit BSP.
 *
 * Written from scratch based on the InvenSense MPU-6886 Product Specification (rev. 1.2).
 * No third-party source code was referenced or copied.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file mpu6886.c
 * @brief MPU6886 I2C driver implementation for the Core2 for AWS IoT Kit BSP.
 */

#include <freertos/FreeRTOS.h>
#include <esp_log.h>

#include "core2foraws_i2c.h"
#include "mpu6886.h"

/* ------------------------------------------------------------------ */
/* Module state                                                       */
/* ------------------------------------------------------------------ */
static core2foraws_i2c_port_t _i2c_port;
static i2c_master_dev_handle_t _mpu6886_dev;
static gyro_scale_t gyro_scale = MPU6886_GFS_2000DPS;
static acc_scale_t  acc_scale  = MPU6886_AFS_8G;
static float acc_res, gyro_res;

static const char *TAG = "MPU6886";

/* ------------------------------------------------------------------ */
/* I2C helpers                                                        */
/* ------------------------------------------------------------------ */

static esp_err_t mpu6886_i2c_init( core2foraws_i2c_port_t port )
{
    if( _mpu6886_dev != NULL )
    {
        return ESP_OK;
    }
    _i2c_port = port;
    return core2foraws_i2c_device_add( port, MPU6886_ADDRESS,
                                       100000, &_mpu6886_dev );
}

static esp_err_t read_reg( uint8_t reg, uint8_t num_bytes,
                           uint8_t *read_buffer )
{
    esp_err_t res = core2foraws_i2c_read( _i2c_port, _mpu6886_dev,
                                          reg, read_buffer, num_bytes );
    if ( res != ESP_OK )
    {
        ESP_LOGE( TAG, "\tCould not read from register 0x%02x", reg );
    }
    return res;
}

static esp_err_t write_reg( uint8_t reg, const uint8_t value )
{
    esp_err_t res = core2foraws_i2c_write( _i2c_port, _mpu6886_dev,
                                           reg, &value, 1 );
    if ( res != ESP_OK )
    {
        ESP_LOGE( TAG, "\tCould not write 0x%02x to register 0x%02x",
                  value, reg );
    }
    return res;
}

/* ------------------------------------------------------------------ */
/* Initialization                                                     */
/* ------------------------------------------------------------------ */

esp_err_t mpu6886_init( core2foraws_i2c_port_t port )
{
    uint8_t device_id;
    esp_err_t err = mpu6886_i2c_init( port );
    if ( err != ESP_OK )
    {
        return err;
    }

    /* Verify the device is an MPU6886 by reading WHO_AM_I register */
    err = read_reg( MPU6886_WHOAMI, 1, &device_id );
    if ( err != ESP_OK )
    {
        return err;
    }
    if ( device_id != 0x19 )
    {
        ESP_LOGE( TAG, "\tUnexpected WHO_AM_I: 0x%02x (expected 0x19)",
                  device_id );
        return ESP_ERR_NOT_FOUND;
    }

    /* Reset the device and wait for it to come back up */
    err = write_reg( MPU6886_PWR_MGMT_1, ( 1 << 7 ) );
    if ( err != ESP_OK )
    {
        return err;
    }
    vTaskDelay( pdMS_TO_TICKS( 100 ) );

    /* Select the best available clock source (auto-select) */
    err = write_reg( MPU6886_PWR_MGMT_1, 0x01 );
    if ( err != ESP_OK )
    {
        return err;
    }
    vTaskDelay( pdMS_TO_TICKS( 10 ) );

    /* ACCEL_INTEL_CTRL (0x69): WoM (Wake-on-Motion) intelligence control.
     * Per the MPU-6886 application note, bit 1 must be written after every
     * device reset.  Setting it ensures accelerometer output values are not
     * clamped during internal WoM comparisons, giving full-range readings
     * at all times.  The INT pin is not connected in this hardware design,
     * but this write is still required for correct ADC output. */
    err = write_reg( MPU6886_ACCEL_INTEL_CTRL, 0x02 );
    if ( err != ESP_OK )
    {
        return err;
    }
    vTaskDelay( pdMS_TO_TICKS( 1 ) );

    /* Set accelerometer full-scale range: ±8 G */
    err = write_reg( MPU6886_ACCEL_CONFIG, ( MPU6886_AFS_8G << 3 ) );
    if ( err != ESP_OK )
    {
        return err;
    }
    vTaskDelay( pdMS_TO_TICKS( 1 ) );

    /* Set gyroscope full-scale range: ±2000 DPS */
    err = write_reg( MPU6886_GYRO_CONFIG, ( MPU6886_GFS_2000DPS << 3 ) );
    if ( err != ESP_OK )
    {
        return err;
    }
    vTaskDelay( pdMS_TO_TICKS( 1 ) );

    /* CONFIG (0x1A) DLPF_CFG=1: enables the digital low-pass filter.
     * Effect: gyroscope 3-dB bandwidth = 184 Hz, delay = 2.9 ms;
     *         internal gyro/accel sample rate (Fs) becomes 1 kHz.
     * (See MPU-6886 datasheet Table 1, DLPF_CFG register description.) */
    err = write_reg( MPU6886_CONFIG, 0x01 );
    if ( err != ESP_OK )
    {
        return err;
    }
    vTaskDelay( pdMS_TO_TICKS( 1 ) );

    /* Sample rate = 1 kHz / (1 + 5) ≈ 167 Hz */
    err = write_reg( MPU6886_SMPLRT_DIV, 0x05 );
    if ( err != ESP_OK )
    {
        return err;
    }
    vTaskDelay( pdMS_TO_TICKS( 1 ) );

    /* Disable all interrupts initially */
    err = write_reg( MPU6886_INT_ENABLE, 0x00 );
    if ( err != ESP_OK )
    {
        return err;
    }
    vTaskDelay( pdMS_TO_TICKS( 1 ) );

    /* Accelerometer DLPF: use default bandwidth */
    err = write_reg( MPU6886_ACCEL_CONFIG2, 0x00 );
    if ( err != ESP_OK )
    {
        return err;
    }
    vTaskDelay( pdMS_TO_TICKS( 1 ) );

    /* Disable FIFO and I2C master mode */
    err = write_reg( MPU6886_USER_CTRL, 0x00 );
    if ( err != ESP_OK )
    {
        return err;
    }
    vTaskDelay( pdMS_TO_TICKS( 1 ) );

    /* Disable FIFO for all sensor types */
    err = write_reg( MPU6886_FIFO_EN, 0x00 );
    if ( err != ESP_OK )
    {
        return err;
    }
    vTaskDelay( pdMS_TO_TICKS( 1 ) );

    /* Configure interrupt pin: latch until status register is read */
    err = write_reg( MPU6886_INT_PIN_CFG, 0x20 );
    if ( err != ESP_OK )
    {
        return err;
    }
    vTaskDelay( pdMS_TO_TICKS( 1 ) );

    /* Enable data-ready interrupt source.
     * Note: the INT pin is not wired in this hardware design (schema.yml),
     * so no physical interrupt fires.  The INT_STATUS register can still
     * be polled if needed; this setting is kept for completeness. */
    err = write_reg( MPU6886_INT_ENABLE, 0x01 );
    if ( err != ESP_OK )
    {
        return err;
    }
    vTaskDelay( pdMS_TO_TICKS( 10 ) );

    /* Cache the resolution values for the configured scales */
    mpu6886_gyro_res_get( gyro_scale, &gyro_res );
    mpu6886_accel_res_get( acc_scale, &acc_res );

    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Raw ADC reads                                                      */
/* ------------------------------------------------------------------ */

esp_err_t mpu6886_adc_accel_get( int16_t *ax, int16_t *ay, int16_t *az )
{
    uint8_t buf[ MPU6886_ADC_ACCEL_NUM_BYTES ];

    esp_err_t err = read_reg( MPU6886_ACCEL_XOUT_H,
                              MPU6886_ADC_ACCEL_NUM_BYTES, buf );
    if ( err != ESP_OK )
    {
        return err;
    }

    *ax = ( ( int16_t )buf[ 0 ] << 8 ) | buf[ 1 ];
    *ay = ( ( int16_t )buf[ 2 ] << 8 ) | buf[ 3 ];
    *az = ( ( int16_t )buf[ 4 ] << 8 ) | buf[ 5 ];

    return ESP_OK;
}

esp_err_t mpu6886_adc_gyro_get( int16_t *gx, int16_t *gy, int16_t *gz )
{
    uint8_t buf[ MPU6886_ADC_GYRO_NUM_BYTES ];

    esp_err_t err = read_reg( MPU6886_GYRO_XOUT_H,
                              MPU6886_ADC_GYRO_NUM_BYTES, buf );
    if ( err != ESP_OK )
    {
        return err;
    }

    *gx = ( ( int16_t )buf[ 0 ] << 8 ) | buf[ 1 ];
    *gy = ( ( int16_t )buf[ 2 ] << 8 ) | buf[ 3 ];
    *gz = ( ( int16_t )buf[ 4 ] << 8 ) | buf[ 5 ];

    return ESP_OK;
}

esp_err_t mpu6886_adc_temp_get( int16_t *t )
{
    uint8_t buf[ MPU6886_ADC_TEMP_NUM_BYTES ];

    esp_err_t err = read_reg( MPU6886_TEMP_OUT_H,
                              MPU6886_ADC_TEMP_NUM_BYTES, buf );
    if ( err != ESP_OK )
    {
        return err;
    }

    *t = ( ( int16_t )buf[ 0 ] << 8 ) | buf[ 1 ];

    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Resolution helpers                                                 */
/* ------------------------------------------------------------------ */

esp_err_t mpu6886_gyro_res_get( gyro_scale_t scale, float *resolution )
{
    switch ( scale )
    {
        case MPU6886_GFS_250DPS:
            *resolution = 250.0f / 32768.0f;
            break;
        case MPU6886_GFS_500DPS:
            *resolution = 500.0f / 32768.0f;
            break;
        case MPU6886_GFS_1000DPS:
            *resolution = 1000.0f / 32768.0f;
            break;
        case MPU6886_GFS_2000DPS:
        default:
            *resolution = 2000.0f / 32768.0f;
            break;
    }

    return ESP_OK;
}

esp_err_t mpu6886_accel_res_get( acc_scale_t scale, float *resolution )
{
    switch ( scale )
    {
        case MPU6886_AFS_2G:
            *resolution = 2.0f / 32768.0f;
            break;
        case MPU6886_AFS_4G:
            *resolution = 4.0f / 32768.0f;
            break;
        case MPU6886_AFS_8G:
            *resolution = 8.0f / 32768.0f;
            break;
        case MPU6886_AFS_16G:
        default:
            *resolution = 16.0f / 32768.0f;
            break;
    }

    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Full-scale range setters                                           */
/* ------------------------------------------------------------------ */

esp_err_t mpu6886_fsr_gyro_set( gyro_scale_t scale )
{
    uint8_t regdata = ( scale << 3 );

    esp_err_t err = write_reg( MPU6886_GYRO_CONFIG, regdata );
    if ( err != ESP_OK )
    {
        return err;
    }

    gyro_scale = scale;
    mpu6886_gyro_res_get( scale, &gyro_res );

    return ESP_OK;
}

esp_err_t mpu6886_fsr_accel_set( acc_scale_t scale )
{
    uint8_t regdata = ( scale << 3 );

    esp_err_t err = write_reg( MPU6886_ACCEL_CONFIG, regdata );
    if ( err != ESP_OK )
    {
        return err;
    }

    acc_scale = scale;
    mpu6886_accel_res_get( scale, &acc_res );

    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Scaled data reads                                                  */
/* ------------------------------------------------------------------ */

esp_err_t mpu6886_accel_data_get( float *ax, float *ay, float *az )
{
    int16_t raw_x = 0, raw_y = 0, raw_z = 0;
    esp_err_t err = mpu6886_adc_accel_get( &raw_x, &raw_y, &raw_z );

    if ( err == ESP_OK )
    {
        *ax = ( float )raw_x * acc_res;
        *ay = ( float )raw_y * acc_res;
        *az = ( float )raw_z * acc_res;
    }

    return err;
}

esp_err_t mpu6886_gyro_data_get( float *gx, float *gy, float *gz )
{
    int16_t raw_x = 0, raw_y = 0, raw_z = 0;
    esp_err_t err = mpu6886_adc_gyro_get( &raw_x, &raw_y, &raw_z );

    if ( err == ESP_OK )
    {
        *gx = ( float )raw_x * gyro_res;
        *gy = ( float )raw_y * gyro_res;
        *gz = ( float )raw_z * gyro_res;
    }

    return err;
}

esp_err_t mpu6886_temp_data_get( float *t )
{
    int16_t raw_temp = 0;
    esp_err_t err = mpu6886_adc_temp_get( &raw_temp );

    if ( err == ESP_OK )
    {
        *t = ( float )raw_temp / 326.8f + 25.0f;
    }

    return err;
}
