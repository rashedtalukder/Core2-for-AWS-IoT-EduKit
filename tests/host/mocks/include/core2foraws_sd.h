#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_err.h"

#define ESP_IDF_VERSION_VAL(major, minor, patch) ((major) * 10000 + (minor) * 100 + (patch))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(6, 0, 1)
#define GPIO_NUM_4 4
typedef struct { struct { char name[8]; } cid; } sdmmc_card_t;
typedef struct { int slot; } sdmmc_host_t;
typedef struct { int host_id, gpio_cs; } sdspi_device_config_t;
typedef struct { bool format_if_mount_failed; int max_files, allocation_unit_size; } esp_vfs_fat_mount_config_t;
#define SDSPI_HOST_DEFAULT() ((sdmmc_host_t){0})
#define SDSPI_DEVICE_CONFIG_DEFAULT() ((sdspi_device_config_t){0})
esp_err_t esp_vfs_fat_sdspi_mount(const char *path, const sdmmc_host_t *host, const sdspi_device_config_t *slot, const esp_vfs_fat_mount_config_t *config, sdmmc_card_t **card);
esp_err_t esp_vfs_fat_sdcard_unmount(const char *path, sdmmc_card_t *card);