#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "light_schedule.h"
#include "relay_controller.h"
#include "wifi_time.h"

static const char *TAG = "watering-can";

#define POLL_INTERVAL_MS 30000

static void configure_timezone(void)
{
    setenv("TZ", CONFIG_WC_TIMEZONE, 1);
    tzset();
}

void app_main(void)
{
    ESP_LOGI(TAG, "watering-can starting (phase 1: grow light scheduler)");

    configure_timezone();
    ESP_ERROR_CHECK(wifi_time_init());

    const uint8_t relay_gpios[RELAY_MAX_CHANNELS] = {
        CONFIG_WC_LIGHT_RELAY1_GPIO,
        CONFIG_WC_LIGHT_RELAY2_GPIO,
        CONFIG_WC_LIGHT_RELAY3_GPIO,
        CONFIG_WC_LIGHT_RELAY4_GPIO,
    };
    relay_controller_init(relay_gpios, CONFIG_WC_NUM_LIGHT_RELAYS, CONFIG_WC_RELAY_ACTIVE_LOW);

    const daily_window_t light_window = {
        .on_hour = CONFIG_WC_LIGHT_ON_HOUR,
        .on_minute = CONFIG_WC_LIGHT_ON_MINUTE,
        .off_hour = CONFIG_WC_LIGHT_OFF_HOUR,
        .off_minute = CONFIG_WC_LIGHT_OFF_MINUTE,
    };

    ESP_LOGI(TAG, "Grow lights scheduled ON %02d:%02d, OFF %02d:%02d, across %d relay(s)",
             light_window.on_hour, light_window.on_minute,
             light_window.off_hour, light_window.off_minute,
             CONFIG_WC_NUM_LIGHT_RELAYS);

    // Forces a log line + relay write on the first pass through the loop,
    // regardless of which state the schedule says we should be in.
    bool lights_on = false;
    bool first_pass = true;

    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        bool should_be_on = light_schedule_is_on(&light_window, &timeinfo);
        if (should_be_on != lights_on || first_pass) {
            char time_buf[16];
            strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &timeinfo);
            ESP_LOGI(TAG, "%s -> turning grow lights %s", time_buf, should_be_on ? "ON" : "OFF");
            for (int i = 0; i < CONFIG_WC_NUM_LIGHT_RELAYS; i++) {
                relay_controller_set(i, should_be_on);
            }
            lights_on = should_be_on;
            first_pass = false;
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}
