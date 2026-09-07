#pragma once
#include "rmt_tx.h"
typedef struct {
    struct { unsigned int duration0, level0, duration1, level1; } bit0, bit1;
    struct { bool msb_first; } flags;
} rmt_bytes_encoder_config_t;
esp_err_t rmt_new_bytes_encoder(const rmt_bytes_encoder_config_t *config, rmt_encoder_handle_t *encoder);
esp_err_t rmt_del_encoder(rmt_encoder_handle_t encoder);