#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../lib/motion/mpu6886.c"

static uint8_t registers[256];
static unsigned int lock_depth;
static bool require_atomic;
static esp_err_t lock_error, transfer_error;
static float *completed_axis;
static float expected_axis;

esp_err_t core2foraws_i2c_lock(core2foraws_i2c_port_t port)
{
    assert(port == CORE2FORAWS_I2C_INTERNAL);
    if (lock_error != ESP_OK) return lock_error;
    lock_depth++;
    return ESP_OK;
}
esp_err_t core2foraws_i2c_unlock(core2foraws_i2c_port_t port)
{
    assert(port == CORE2FORAWS_I2C_INTERNAL && lock_depth > 0);
    if (require_atomic && transfer_error == ESP_OK)
    {
        assert((unsigned int)acc_scale == (registers[MPU6886_ACCEL_CONFIG] >> 3));
        assert((unsigned int)gyro_scale == (registers[MPU6886_GYRO_CONFIG] >> 3));
        if (completed_axis != NULL) assert(fabsf(*completed_axis - expected_axis) < 0.0001f);
    }
    lock_depth--;
    return ESP_OK;
}
esp_err_t core2foraws_i2c_device_add(core2foraws_i2c_port_t port, uint16_t address, uint32_t speed, i2c_master_dev_handle_t *handle)
{
    assert(port == CORE2FORAWS_I2C_INTERNAL && address == 0x68 && speed == 100000);
    *handle = (i2c_master_dev_handle_t)registers;
    return ESP_OK;
}
esp_err_t core2foraws_i2c_read(core2foraws_i2c_port_t port, i2c_master_dev_handle_t handle, uint32_t reg, uint8_t *data, uint16_t size)
{
    assert(port == CORE2FORAWS_I2C_INTERNAL && handle != NULL && reg + size <= sizeof(registers));
    if (require_atomic) assert(lock_depth == 1);
    if (transfer_error != ESP_OK) return transfer_error;
    memcpy(data, registers + reg, size);
    return ESP_OK;
}
esp_err_t core2foraws_i2c_write(core2foraws_i2c_port_t port, i2c_master_dev_handle_t handle, uint32_t reg, const uint8_t *data, uint16_t size)
{
    assert(port == CORE2FORAWS_I2C_INTERNAL && handle != NULL && reg + size <= sizeof(registers));
    if (require_atomic) assert(lock_depth == 1);
    if (transfer_error != ESP_OK) return transfer_error;
    memcpy(registers + reg, data, size);
    return ESP_OK;
}
void vTaskDelay(TickType_t ticks) { (void)ticks; }

int main(void)
{
    registers[MPU6886_WHOAMI] = 0x19;
    assert(mpu6886_init(CORE2FORAWS_I2C_INTERNAL) == ESP_OK);
    const uint8_t samples[] = { 0x40, 0x00, 0xc0, 0x00, 0x00, 0x00 };
    memcpy(registers + MPU6886_ACCEL_XOUT_H, samples, sizeof(samples));
    memcpy(registers + MPU6886_GYRO_XOUT_H, samples, sizeof(samples));
    require_atomic = true;
    for (unsigned int range = 0; range < 4; ++range)
    {
        assert(mpu6886_fsr_accel_set((acc_scale_t)range) == ESP_OK);
        assert(mpu6886_fsr_gyro_set((gyro_scale_t)range) == ESP_OK);
        float axis_x = 0, axis_y = 0, axis_z = 0;
        completed_axis = &axis_x;
        expected_axis = (float)(1U << range);
        assert(mpu6886_accel_data_get(&axis_x, &axis_y, &axis_z) == ESP_OK);
        assert(axis_y == -expected_axis && axis_z == 0);
        expected_axis = 125.0f * (1U << range);
        assert(mpu6886_gyro_data_get(&axis_x, &axis_y, &axis_z) == ESP_OK);
        assert(axis_y == -expected_axis && axis_z == 0);
        completed_axis = NULL;
    }
    float old_acc_res = acc_res, old_gyro_res = gyro_res;
    transfer_error = ESP_ERR_TIMEOUT;
    assert(mpu6886_fsr_accel_set(MPU6886_AFS_2G) == ESP_ERR_TIMEOUT);
    assert(mpu6886_fsr_gyro_set(MPU6886_GFS_250DPS) == ESP_ERR_TIMEOUT);
    assert(acc_res == old_acc_res && gyro_res == old_gyro_res && lock_depth == 0);
    float axis_x = 123, axis_y = 456, axis_z = 789;
    assert(mpu6886_accel_data_get(&axis_x, &axis_y, &axis_z) == ESP_ERR_TIMEOUT);
    assert(axis_x == 123 && axis_y == 456 && axis_z == 789 && lock_depth == 0);
    transfer_error = ESP_OK;
    lock_error = ESP_ERR_TIMEOUT;
    assert(mpu6886_gyro_data_get(&axis_x, &axis_y, &axis_z) == ESP_ERR_TIMEOUT);
    assert(mpu6886_fsr_accel_set(MPU6886_AFS_2G) == ESP_ERR_TIMEOUT);
    lock_error = ESP_OK;
    assert(mpu6886_fsr_accel_set((acc_scale_t)-1) == ESP_ERR_INVALID_ARG);
    assert(mpu6886_fsr_gyro_set((gyro_scale_t)4) == ESP_ERR_INVALID_ARG);
    assert(mpu6886_accel_data_get(NULL, &axis_y, &axis_z) == ESP_ERR_INVALID_ARG);
    assert(lock_depth == 0);
    puts("Motion register/cache atomicity, scaled-read locking and error tests passed");
    return 0;
}