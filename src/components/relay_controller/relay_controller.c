#include "relay_controller.h"

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "relay_controller";

static uint8_t s_gpio_nums[RELAY_MAX_CHANNELS];
static uint8_t s_count = 0;
static bool s_active_low = true;
static bool s_energized[RELAY_MAX_CHANNELS];

void relay_controller_init(const uint8_t *gpio_nums, uint8_t count, bool active_low)
{
    if (count > RELAY_MAX_CHANNELS) {
        ESP_LOGE(TAG, "Requested %d relay channels, clamping to max %d", count, RELAY_MAX_CHANNELS);
        count = RELAY_MAX_CHANNELS;
    }
    s_count = count;
    s_active_low = active_low;

    for (uint8_t i = 0; i < s_count; i++) {
        s_gpio_nums[i] = gpio_nums[i];
        gpio_reset_pin(s_gpio_nums[i]);
        gpio_set_direction(s_gpio_nums[i], GPIO_MODE_OUTPUT);

        // Drive to de-energized right away. On the old demo, the pin sat at
        // its power-on-reset level (LOW, i.e. energized under active-low)
        // for the first several seconds of boot before the app loop got
        // around to setting it -- not something we want with real mains
        // loads attached.
        s_energized[i] = false;
        gpio_set_level(s_gpio_nums[i], s_active_low ? 1 : 0);
        ESP_LOGI(TAG, "Relay %d on GPIO %d initialized (de-energized)", i, s_gpio_nums[i]);
    }
}

void relay_controller_set(uint8_t index, bool energized)
{
    if (index >= s_count) {
        ESP_LOGE(TAG, "relay_controller_set: index %d out of range (count=%d)", index, s_count);
        return;
    }
    int level = s_active_low ? !energized : energized;
    gpio_set_level(s_gpio_nums[index], level);
    s_energized[index] = energized;
    ESP_LOGI(TAG, "Relay %d (GPIO %d) -> %s", index, s_gpio_nums[index], energized ? "energized" : "de-energized");
}

bool relay_controller_is_energized(uint8_t index)
{
    if (index >= s_count) {
        return false;
    }
    return s_energized[index];
}
