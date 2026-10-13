/*
 * Bring-up firmware for the M5Stack Unit PoE-P4 pot module.
 *
 * Scope of THIS build, deliberately: confirm the esp32p4 toolchain,
 * build, flash (via the Pi, same as src/) and serial boot all work on
 * real hardware. No Ethernet/PoE, sensors or relays yet -- following
 * the same "prove it bit by bit" approach used for the main module.
 *
 * Once this boots and logs over serial, the next step is Ethernet
 * bring-up (DHCP + link-up logging) using the RMII pin mapping recorded
 * in hardware_bom.md. Those pin numbers come from M5Stack's own docs
 * page (not the schematic directly), so treat them as a first guess to
 * verify against actual link-up behavior, not as confirmed-correct.
 */
#include <stdio.h>
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
    ESP_LOGI(TAG, "goal of this build: toolchain + flash + boot only.");
    ESP_LOGI(TAG, "ethernet/PoE bring-up is the next step once this is confirmed.");

    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "alive (%d)", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
