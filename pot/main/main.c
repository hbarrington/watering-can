/*
 * Bring-up firmware for the M5Stack Unit PoE-P4 pot module.
 *
 * Phase 1 (done): confirm the esp32p4 toolchain, build, flash (via the
 * Pi, same as src/) and serial boot all work on real hardware.
 *
 * Phase 2 (this build): bring up the onboard Ethernet (internal EMAC +
 * IP101 PHY over the RJ45/PoE port) and confirm link-up + DHCP. Pin
 * numbers (MDC/MDIO/PHY-reset/RMII data+clock) come from
 * ETH_ESP32_EMAC_DEFAULT_CONFIG() for esp32p4 in ESP-IDF's own
 * esp_eth_mac_esp.h, which match hardware_bom.md's pin table pulled
 * from M5Stack's docs -- two independent sources agreeing, though the
 * PHY address (CONFIG_EXAMPLE_ETH_PHY_ADDR, sdkconfig.defaults) is
 * still just the generic example default (1), unconfirmed for this
 * board specifically. If link doesn't come up, that's the first thing
 * to try changing.
 *
 * Still no sensors/relays -- one thing at a time.
 */
#include <stdio.h>
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "eth_bringup.h"

static const char *TAG = "pot-bringup";

void app_main(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "watering-can: pot module bring-up (Unit PoE-P4)");
    ESP_LOGI(TAG, "chip: %s, %d core(s), rev v%d.%d",
             CONFIG_IDF_TARGET, chip_info.cores,
             chip_info.revision / 100, chip_info.revision % 100);
    ESP_LOGI(TAG, "flash: %u MB", (unsigned)(flash_size / (1024 * 1024)));
    ESP_LOGI(TAG, "free heap: %u bytes", (unsigned)esp_get_free_heap_size());
    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "goal of this build: ethernet link-up + DHCP over the PoE port.");
    ESP_LOGI(TAG, "================================================");

    esp_err_t err = eth_bringup_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eth_bringup_init failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "ethernet connected");
    }

    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "alive (%d)", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
