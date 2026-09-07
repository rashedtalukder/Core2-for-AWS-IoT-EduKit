#include <assert.h>
#include <stdio.h>
#include "../../lib/rgb_led/core2foraws_rgb_led.c"

struct test_rmt_channel { bool allocated, enabled, pending; };
struct test_rmt_encoder { bool allocated; };
static struct test_rmt_channel channel_storage;
static struct test_rmt_encoder encoder_storage;
static esp_err_t wait_error, disable_error, delete_encoder_error, delete_channel_error;
static esp_err_t new_encoder_error, enable_error;
static unsigned int disable_calls, encoder_deletes;
static uint8_t submitted[30];

SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage) { return storage; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t timeout)
{
    assert(timeout == 100 && mutex->depth == 0);
    mutex->depth++;
    return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex) { assert(mutex->depth == 1); mutex->depth--; return pdTRUE; }
void vTaskDelay(TickType_t ticks) { assert(ticks == 1); }
void esp_rom_delay_us(unsigned int microseconds) { assert(microseconds == 80); }
esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t *config, rmt_channel_handle_t *channel)
{
    assert(config->gpio_num == 25 && !channel_storage.allocated);
    channel_storage.allocated = true;
    *channel = &channel_storage;
    return ESP_OK;
}
esp_err_t rmt_new_bytes_encoder(const rmt_bytes_encoder_config_t *config, rmt_encoder_handle_t *encoder)
{
    assert(config->flags.msb_first && !encoder_storage.allocated);
    if (new_encoder_error != ESP_OK) return new_encoder_error;
    encoder_storage.allocated = true;
    *encoder = &encoder_storage;
    return ESP_OK;
}
esp_err_t rmt_enable(rmt_channel_handle_t channel)
{
    assert(channel->allocated && !channel->enabled);
    if (enable_error != ESP_OK) return enable_error;
    channel->enabled = true;
    return ESP_OK;
}
esp_err_t rmt_tx_wait_all_done(rmt_channel_handle_t channel, int timeout_ms)
{
    assert(channel->enabled && timeout_ms == 1000);
    if (wait_error != ESP_OK) return wait_error;
    if (channel->pending) assert(memcmp(submitted, _tx_buf, sizeof(submitted)) == 0);
    channel->pending = false;
    return ESP_OK;
}
esp_err_t rmt_transmit(rmt_channel_handle_t channel, rmt_encoder_handle_t encoder,
                       const void *buffer, size_t size, const rmt_transmit_config_t *config)
{
    assert(channel->enabled && encoder->allocated && !channel->pending && config->loop_count == 0);
    assert(size == sizeof(submitted));
    memcpy(submitted, buffer, size);
    channel->pending = true;
    return ESP_OK;
}
esp_err_t rmt_disable(rmt_channel_handle_t channel)
{
    assert(channel->enabled && !channel->pending);
    disable_calls++;
    if (disable_error != ESP_OK) return disable_error;
    channel->enabled = false;
    return ESP_OK;
}
esp_err_t rmt_del_encoder(rmt_encoder_handle_t encoder)
{
    assert(encoder->allocated && !channel_storage.enabled);
    encoder_deletes++;
    if (delete_encoder_error != ESP_OK) return delete_encoder_error;
    encoder->allocated = false;
    return ESP_OK;
}
esp_err_t rmt_del_channel(rmt_channel_handle_t channel)
{
    assert(channel->allocated && !channel->enabled && !encoder_storage.allocated);
    if (delete_channel_error != ESP_OK) return delete_channel_error;
    channel->allocated = false;
    return ESP_OK;
}

static void check_pending_cleanup(void)
{
    assert(core2foraws_rgb_led_write() == ESP_ERR_INVALID_STATE);
    assert(core2foraws_rgb_led_init() == ESP_ERR_INVALID_STATE);
    assert(channel_storage.allocated);
}

int main(void)
{
    assert(core2foraws_rgb_led_init() == ESP_OK);
    assert(core2foraws_rgb_led_init() == ESP_OK);
    assert(core2foraws_rgb_led_single_color_set(0, 0x123456) == ESP_OK);
    assert(core2foraws_rgb_led_write() == ESP_OK);
    assert(submitted[0] == 0x34 && submitted[1] == 0x12 && submitted[2] == 0x56);
    assert(core2foraws_rgb_led_single_color_set(0, 0xffffff) == ESP_OK);
    wait_error = ESP_ERR_TIMEOUT;
    assert(core2foraws_rgb_led_write() == ESP_ERR_TIMEOUT);
    assert(memcmp(submitted, _tx_buf, sizeof(submitted)) == 0);
    assert(core2foraws_rgb_led_deinit() == ESP_ERR_TIMEOUT);
    check_pending_cleanup();
    wait_error = ESP_OK;
    disable_error = ESP_FAIL;
    assert(core2foraws_rgb_led_deinit() == ESP_FAIL);
    check_pending_cleanup();
    disable_error = ESP_OK;
    delete_encoder_error = ESP_ERR_TIMEOUT;
    assert(core2foraws_rgb_led_deinit() == ESP_ERR_TIMEOUT);
    unsigned int stopped_count = disable_calls;
    check_pending_cleanup();
    delete_encoder_error = ESP_OK;
    delete_channel_error = ESP_FAIL;
    assert(core2foraws_rgb_led_deinit() == ESP_FAIL);
    unsigned int deleted_count = encoder_deletes;
    check_pending_cleanup();
    delete_channel_error = ESP_OK;
    assert(core2foraws_rgb_led_deinit() == ESP_OK);
    assert(disable_calls == stopped_count && encoder_deletes == deleted_count);
    assert(core2foraws_rgb_led_deinit() == ESP_OK);
    new_encoder_error = ESP_ERR_NO_MEM;
    delete_channel_error = ESP_FAIL;
    assert(core2foraws_rgb_led_init() == ESP_ERR_NO_MEM);
    check_pending_cleanup();
    new_encoder_error = ESP_OK;
    delete_channel_error = ESP_OK;
    assert(core2foraws_rgb_led_deinit() == ESP_OK);
    enable_error = ESP_FAIL;
    delete_encoder_error = ESP_ERR_TIMEOUT;
    assert(core2foraws_rgb_led_init() == ESP_FAIL);
    check_pending_cleanup();
    enable_error = ESP_OK;
    delete_encoder_error = ESP_OK;
    assert(core2foraws_rgb_led_deinit() == ESP_OK);
    assert(core2foraws_rgb_led_init() == ESP_OK);
    assert(core2foraws_rgb_led_deinit() == ESP_OK);
    assert(!channel_storage.allocated && !encoder_storage.allocated && _rgb_mutex->depth == 0);
    puts("RGB buffer lifetime, partial-init and teardown failure/retry tests passed");
    return 0;
}