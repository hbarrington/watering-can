#include "light_state.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "light_state";
static const char *NVS_NAMESPACE = "wc_light";
static const char *NVS_KEY = "window";

static SemaphoreHandle_t s_mutex;
static daily_window_t s_window;
static bool s_override_active = false;
static bool s_override_desired_on = false;
static bool s_override_baseline_schedule_on = false;
static bool s_actual_on = false;

esp_err_t light_state_init(const daily_window_t *default_window)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        return ESP_ERR_NO_MEM;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed (%s) -- using Kconfig default schedule, changes won't persist",
                 esp_err_to_name(err));
        s_window = *default_window;
        return ESP_OK;
    }

    size_t size = sizeof(s_window);
    err = nvs_get_blob(handle, NVS_KEY, &s_window, &size);
    if (err != ESP_OK || size != sizeof(s_window)) {
        ESP_LOGI(TAG, "No saved schedule in NVS -- seeding with Kconfig default (ON %02d:%02d, OFF %02d:%02d)",
                 default_window->on_hour, default_window->on_minute,
                 default_window->off_hour, default_window->off_minute);
        s_window = *default_window;
        esp_err_t seed_err = nvs_set_blob(handle, NVS_KEY, &s_window, sizeof(s_window));
        if (seed_err == ESP_OK) {
            seed_err = nvs_commit(handle);
        }
        if (seed_err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to seed NVS with default schedule: %s", esp_err_to_name(seed_err));
        }
    } else {
        ESP_LOGI(TAG, "Loaded saved schedule from NVS: ON %02d:%02d, OFF %02d:%02d",
                 s_window.on_hour, s_window.on_minute, s_window.off_hour, s_window.off_minute);
    }
    nvs_close(handle);
    return ESP_OK;
}

daily_window_t light_state_get_window(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    daily_window_t copy = s_window;
    xSemaphoreGive(s_mutex);
    return copy;
}

esp_err_t light_state_set_window(const daily_window_t *window)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(handle, NVS_KEY, window, sizeof(*window));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist schedule to NVS: %s", esp_err_to_name(err));
        return err;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_window = *window;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Schedule updated: ON %02d:%02d, OFF %02d:%02d",
             window->on_hour, window->on_minute, window->off_hour, window->off_minute);
    return ESP_OK;
}

void light_state_set_override(bool desired_on, bool current_schedule_on)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_override_active = true;
    s_override_desired_on = desired_on;
    s_override_baseline_schedule_on = current_schedule_on;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Manual override engaged: forcing lights %s until the next schedule transition",
             desired_on ? "ON" : "OFF");
}

void light_state_clear_override(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool was_active = s_override_active;
    s_override_active = false;
    xSemaphoreGive(s_mutex);
    if (was_active) {
        ESP_LOGI(TAG, "Manual override cleared, resuming schedule");
    }
}

bool light_state_resolve(bool schedule_on)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool result;
    if (s_override_active) {
        if (schedule_on != s_override_baseline_schedule_on) {
            s_override_active = false;
            result = schedule_on;
            ESP_LOGI(TAG, "Schedule transitioned since override was set -- auto-clearing it");
        } else {
            result = s_override_desired_on;
        }
    } else {
        result = schedule_on;
    }
    xSemaphoreGive(s_mutex);
    return result;
}

void light_state_set_actual(bool energized)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_actual_on = energized;
    xSemaphoreGive(s_mutex);
}

light_status_t light_state_get_status(bool schedule_on)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    light_status_t status = {
        .window = s_window,
        .schedule_says_on = schedule_on,
        .override_active = s_override_active,
        .actual_on = s_actual_on,
    };
    xSemaphoreGive(s_mutex);
    return status;
}
