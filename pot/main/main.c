/*
 * Bring-up firmware for the M5Stack Unit PoE-P4 pot module.
 *
 * Phase 1 (done): confirm the esp32p4 toolchain, build, flash (via the
 * Pi, same as src/) and serial boot all work on real hardware.
 *
 * Phase 2 (done): Ethernet (internal EMAC + IP101 PHY over the RJ45/PoE
 * port) -- link up, DHCP-assigned IP, confirmed on hardware.
 *
 * Phase 3 (done): first real sensor. Capacitive soil moisture sensor
 * on CONFIG_WC_POT_MOISTURE_ADC_GPIO (default GPIO19, Hat2-Bus header),
 * calibrated to a 0-100% scale against raw readings measured on
 * hardware in air (dry) and fully submerged in water (wet) -- see
 * Kconfig.projbuild. DS18B20 (soil temp) and the DFRobot pH sensor
 * aren't wired up yet -- pH not purchased yet.
 */
#include <stdio.h>
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "eth_bringup.h"
#include "moisture_sensor.h"

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
    ESP_LOGI(TAG, "goal of this build: calibrated moisture %% readings on the serial log.");
    ESP_LOGI(TAG, "================================================");

    esp_err_t moisture_err = moisture_sensor_init();
    if (moisture_err != ESP_OK) {
        ESP_LOGE(TAG, "moisture_sensor_init failed: %s", esp_err_to_name(moisture_err));
    }

    esp_err_t err = eth_bringup_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eth_bringup_init failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "ethernet connected");
    }

    int count = 0;
    while (1) {
        if (moisture_err == ESP_OK) {
            int raw = 0, mv = 0, percent = 0;
            if (moisture_sensor_read(&raw, &mv, &percent) == ESP_OK) {
                if (mv >= 0) {
                    ESP_LOGI(TAG, "alive (%d) | moisture: raw=%d mv=%d (%d%%)", count, raw, mv, percent);
                } else {
                    ESP_LOGI(TAG, "alive (%d) | moisture: raw=%d (%d%%, uncalibrated mv)", count, raw, percent);
                }
            } else {
                ESP_LOGW(TAG, "alive (%d) | moisture read failed", count);
            }
        } else {
            ESP_LOGI(TAG, "alive (%d)", count);
        }
        count++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
