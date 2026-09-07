#pragma once
#include "esp_err.h"
#include "freertos/semphr.h"
#define CORE2FORAWS_SPI_LOCK_TIMEOUT_MS 500
extern SemaphoreHandle_t core2foraws_common_spi_semaphore;
esp_err_t core2foraws_common_spi_bus_init(void);
esp_err_t core2foraws_common_error(esp_err_t error);