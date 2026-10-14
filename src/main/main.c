#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "light_schedule.h"
#include "light_state.h"
#include "relay_controller.h"
#include "web_api.h"
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
    ESP_LOGI(TAG, "watering-can starting (phase 1.5: grow light scheduler)");

    configure_timezone();

    // The schedule is now mutable at runtime (via the web API's PUT
    // /schedule), persisted in NVS -- this seeds it with the Kconfig values
    // only on first boot / if NVS has nothing saved yet. Doesn't need WiFi,
    // so it's safe to do before wifi_time_init().
    const daily_window_t default_window = {
        .on_hour = CONFIG_WC_LIGHT_ON_HOUR,
        .on_minute = CONFIG_WC_LIGHT_ON_MINUTE,
        .off_hour = CONFIG_WC_LIGHT_OFF_HOUR,
        .off_minute = CONFIG_WC_LIGHT_OFF_MINUTE,
    };
    ESP_ERROR_CHECK(light_state_init(&default_window));

    const uint8_t relay_gpios[RELAY_MAX_CHANNELS] = {
        CONFIG_WC_LIGHT_RELAY1_GPIO,
        CONFIG_WC_LIGHT_RELAY2_GPIO,
        CONFIG_WC_LIGHT_RELAY3_GPIO,
        CONFIG_WC_LIGHT_RELAY4_GPIO,
    };
    relay_controller_init(relay_gpios, CONFIG_WC_NUM_LIGHT_RELAYS, CONFIG_WC_RELAY_ACTIVE_LOW);

    ESP_ERROR_CHECK(wifi_time_init());
    ESP_ERROR_CHECK(web_api_start());

    {
        daily_window_t window = light_state_get_window();
        ESP_LOGI(TAG, "Grow lights scheduled ON %02d:%02d, OFF %02d:%02d, across %d relay(s)",
                 window.on_hour, window.on_minute, window.off_hour, window.off_minute,
                 CONFIG_WC_NUM_LIGHT_RELAYS);
    }

    // Forces a log line + relay write on the first pass through the loop,
    // regardless of which state the schedule says we should be in.
    bool lights_on = false;
    bool first_pass = true;

    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        // Re-fetched every poll (not just at boot) since the web API can
        // change it at any time via PUT /schedule.
        daily_window_t light_window = light_state_get_window();
        bool schedule_on = light_schedule_is_on(&light_window, &timeinfo);
        // Folds in any active manual override (POST /lights), which
        // auto-clears itself here once the schedule's own decision changes.
        bool should_be_on = light_state_resolve(schedule_on);

        if (should_be_on != lights_on || first_pass) {
            char time_buf[16];
            strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &timeinfo);
            ESP_LOGI(TAG, "%s -> turning grow lights %s", time_buf, should_be_on ? "ON" : "OFF");
            for (int i = 0; i < CONFIG_WC_NUM_LIGHT_RELAYS; i++) {
                relay_controller_set(i, should_be_on);
            }
            light_state_set_actual(should_be_on);
            lights_on = should_be_on;
            first_pass = false;
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}
