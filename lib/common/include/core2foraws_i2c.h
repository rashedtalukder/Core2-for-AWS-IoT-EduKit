/*
 * Core2 for AWS IoT Kit BSP
 * Copyright (C) 2026 Rashed Talukder.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _CORE2FORAWS_I2C_H_
#define _CORE2FORAWS_I2C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <esp_err.h>
#include <driver/i2c_master.h>

/**
 * @brief Flags for register address handling in read/write operations.
 *
 * These mirror the legacy i2c_manager flags for backward compatibility
 * with existing driver code.
 */
#define CORE2FORAWS_I2C_NO_REG  ( 1 << 30 )
#define CORE2FORAWS_I2C_REG_16  ( 1 << 31 )

/**
 * @brief I2C bus port identifiers.
 */
typedef enum {
    CORE2FORAWS_I2C_INTERNAL = 0,   /**< Internal board bus: AXP192, MPU6886, BM8563, FT6336, ATECC608, and J3. */
    CORE2FORAWS_I2C_EXTERNAL,       /**< Reopenable external Port A bus for one or more accessories. */
    CORE2FORAWS_I2C_PORT_MAX
} core2foraws_i2c_port_t;

/**
 * @brief Initialize an I2C master bus.
 *
 * Creates a board-defined bus and its static recursive mutex. Safe to call
 * multiple times and concurrently. The internal bus is initialized during BSP
 * startup and remains active for board lifetime. The external bus is opened on
 * demand by the expansion-port API.
 *
 * @param[in] port The I2C bus port to initialize.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t core2foraws_i2c_init( core2foraws_i2c_port_t port );

/**
 * @brief Deinitialize an I2C master bus.
 *
 * Removes all managed devices and deletes the external Port A bus. The internal
 * board bus cannot be deinitialized because fixed BSP peripherals retain
 * handles for board lifetime.
 *
 * @param[in] port The I2C bus port to deinitialize.
 * @return ESP_OK on success or ESP_ERR_NOT_SUPPORTED for the internal bus.
 */
esp_err_t core2foraws_i2c_deinit( core2foraws_i2c_port_t port );

/**
 * @brief Get the ESP-IDF master bus handle for a port.
 *
 * This is an integration escape hatch for drivers such as `esp_lcd_touch`.
 * Any transaction issued through the raw handle must still be enclosed by
 * core2foraws_i2c_lock()/core2foraws_i2c_unlock().
 *
 * @param[in]  port   The I2C bus port.
 * @param[out] handle Pointer to receive the bus handle.
 * @return ESP_OK on success.
 */
esp_err_t core2foraws_i2c_get_bus_handle( core2foraws_i2c_port_t port,
                                          i2c_master_bus_handle_t *handle );

/**
 * @brief Register a managed I2C device on a bus.
 *
 * Multiple devices with different addresses and speeds are supported on each
 * bus. Registering the same address and speed again returns the existing handle
 * and increments its reference count.
 *
 * @param[in]  port         The I2C bus port.
 * @param[in]  dev_addr     7-bit device address.
 * @param[in]  scl_speed_hz SCL clock speed in Hz for this device.
 * @param[out] dev_handle   Pointer to receive the device handle.
 * @return ESP_OK on success.
 */
esp_err_t core2foraws_i2c_device_add( core2foraws_i2c_port_t port,
                                      uint16_t dev_addr,
                                      uint32_t scl_speed_hz,
                                      i2c_master_dev_handle_t *dev_handle );

/**
 * @brief Release a previously registered managed I2C device.
 *
 * The underlying ESP-IDF device is removed when its final reference is
 * released.
 *
 * @param[in] dev_handle The device handle to remove.
 * @return ESP_OK on success.
 */
esp_err_t core2foraws_i2c_device_remove( i2c_master_dev_handle_t dev_handle );

/**
 * @brief Thread-safe read from an I2C device register.
 *
 * Acquires the recursive bus mutex before performing the transaction.
 * Handles CORE2FORAWS_I2C_NO_REG and CORE2FORAWS_I2C_REG_16 flags in the register address.
 *
 * @param[in]  port         The I2C bus port.
 * @param[in]  dev_handle   Device handle from core2foraws_i2c_device_add().
 * @param[in]  reg          Register address (or CORE2FORAWS_I2C_NO_REG).
 * @param[out] buffer       Buffer to store read data.
 * @param[in]  size         Number of bytes to read.
 * @return ESP_OK on success.
 */
esp_err_t core2foraws_i2c_read( core2foraws_i2c_port_t port,
                                i2c_master_dev_handle_t dev_handle,
                                uint32_t reg,
                                uint8_t *buffer,
                                uint16_t size );

/**
 * @brief Thread-safe write to an I2C device register.
 *
 * Acquires the recursive bus mutex before performing the transaction.
 * Handles CORE2FORAWS_I2C_NO_REG and CORE2FORAWS_I2C_REG_16 flags in the register address.
 *
 * @param[in] port       The I2C bus port.
 * @param[in] dev_handle Device handle from core2foraws_i2c_device_add().
 * @param[in] reg        Register address (or CORE2FORAWS_I2C_NO_REG).
 * @param[in] buffer     Data to write.
 * @param[in] size       Number of bytes to write.
 * @return ESP_OK on success.
 */
esp_err_t core2foraws_i2c_write( core2foraws_i2c_port_t port,
                                 i2c_master_dev_handle_t dev_handle,
                                 uint32_t reg,
                                 const uint8_t *buffer,
                                 uint16_t size );

/**
 * @brief Acquire the I2C bus mutex.
 *
 * Use this for multi-operation sequences that must be atomic or when issuing a
 * transaction through a raw bus handle. The lock is recursive: the owning task
 * may call core2foraws_i2c_read()/write() while holding it. Every successful
 * lock must be paired with core2foraws_i2c_unlock() on the same task.
 *
 * @param[in] port The I2C bus port.
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if mutex could not be acquired.
 */
esp_err_t core2foraws_i2c_lock( core2foraws_i2c_port_t port );

/**
 * @brief Release the I2C bus mutex.
 *
 * @param[in] port The I2C bus port.
 * @return ESP_OK on success.
 */
esp_err_t core2foraws_i2c_unlock( core2foraws_i2c_port_t port );

#ifdef __cplusplus
}
#endif

#endif /* _CORE2FORAWS_I2C_H_ */
