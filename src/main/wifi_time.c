#include "wifi_time.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

static const char *TAG = "wifi_time";

// How long to block waiting for the *first* SNTP sync before giving up and
// proceeding with whatever the clock currently reads. esp_netif_sntp_start()
// only kicks the sync off in the background -- it does not block -- so
// without this wait, app_main() was evaluating the grow light schedule
// against the unsynced post-boot clock (epoch 0, i.e. Dec 31 1969 19:00:00
// in EST) well before the real sync callback fired a few seconds later.
#define SNTP_FIRST_SYNC_TIMEOUT_MS 15000

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized via SNTP");
}

esp_err_t wifi_time_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = time_sync_notification_cb;
    ESP_ERROR_CHECK(esp_netif_sntp_init(&config));
    ESP_ERROR_CHECK(esp_netif_sntp_start());

    esp_err_t sync_err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(SNTP_FIRST_SYNC_TIMEOUT_MS));
    if (sync_err != ESP_OK) {
        // Don't hang forever if NTP is unreachable (e.g. router down) --
        // proceed with whatever the clock reads, but make it loud in the
        // log rather than silently scheduling against a wrong clock.
        ESP_LOGW(TAG, "SNTP sync did not complete within %d ms (%s) -- "
                 "proceeding with unsynced clock, schedule may be wrong "
                 "until the background sync eventually completes",
                 SNTP_FIRST_SYNC_TIMEOUT_MS, esp_err_to_name(sync_err));
    }

    return ESP_OK;
}
