#include "web_api.h"

#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <time.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "light_schedule.h"
#include "light_state.h"

static const char *TAG = "web_api";

// Generous but bounded -- these are small hand-typed JSON bodies
// ({"on_hour":4,...} / {"on":true}), not firmware uploads (that's /ota,
// which streams instead of buffering).
#define JSON_BODY_MAX_LEN 512
#define OTA_RECV_BUF_SIZE 1024

static bool current_schedule_on(void)
{
    daily_window_t window = light_state_get_window();
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    return light_schedule_is_on(&window, &timeinfo);
}

static esp_err_t send_json(httpd_req_t *req, cJSON *root)
{
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, json);
    free(json);
    return err;
}

// Reads the whole request body into `buf` (NUL-terminated). Rejects bodies
// that are missing or too large for `buf_len` rather than silently
// truncating -- these handlers all expect small, complete JSON objects.
static esp_err_t read_body(httpd_req_t *req, char *buf, size_t buf_len)
{
    if (req->content_len <= 0 || (size_t)req->content_len >= buf_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    int received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, buf + received, req->content_len - received);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[received] = '\0';
    return ESP_OK;
}

static cJSON *schedule_to_json(const daily_window_t *window)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "on_hour", window->on_hour);
    cJSON_AddNumberToObject(obj, "on_minute", window->on_minute);
    cJSON_AddNumberToObject(obj, "off_hour", window->off_hour);
    cJSON_AddNumberToObject(obj, "off_minute", window->off_minute);
    return obj;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    bool schedule_on = current_schedule_on();
    light_status_t status = light_state_get_status(schedule_on);

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char time_buf[16];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &timeinfo);

    const esp_app_desc_t *app_desc = esp_app_get_description();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "lights_on", status.actual_on);
    cJSON_AddBoolToObject(root, "schedule_says_on", status.schedule_says_on);
    cJSON_AddBoolToObject(root, "override_active", status.override_active);
    cJSON_AddItemToObject(root, "schedule", schedule_to_json(&status.window));
    cJSON_AddStringToObject(root, "now", time_buf);
    cJSON_AddStringToObject(root, "firmware_version", app_desc->version);
    cJSON_AddNumberToObject(root, "uptime_s", (double)(esp_timer_get_time() / 1000000));

    return send_json(req, root);
}

static esp_err_t schedule_get_handler(httpd_req_t *req)
{
    daily_window_t window = light_state_get_window();
    return send_json(req, schedule_to_json(&window));
}

static esp_err_t schedule_put_handler(httpd_req_t *req)
{
    char buf[JSON_BODY_MAX_LEN];
    if (read_body(req, buf, sizeof(buf)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body missing or too large");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Starts from the current window so a request can update just one field
    // (e.g. {"off_hour":15}) without having to resend the whole schedule.
    daily_window_t window = light_state_get_window();
    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "on_hour")) && cJSON_IsNumber(item)) {
        window.on_hour = item->valueint;
    }
    if ((item = cJSON_GetObjectItem(root, "on_minute")) && cJSON_IsNumber(item)) {
        window.on_minute = item->valueint;
    }
    if ((item = cJSON_GetObjectItem(root, "off_hour")) && cJSON_IsNumber(item)) {
        window.off_hour = item->valueint;
    }
    if ((item = cJSON_GetObjectItem(root, "off_minute")) && cJSON_IsNumber(item)) {
        window.off_minute = item->valueint;
    }
    cJSON_Delete(root);

    if (window.on_hour < 0 || window.on_hour > 23 || window.off_hour < 0 || window.off_hour > 23 ||
        window.on_minute < 0 || window.on_minute > 59 || window.off_minute < 0 || window.off_minute > 59) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Hours must be 0-23, minutes 0-59");
        return ESP_FAIL;
    }

    esp_err_t err = light_state_set_window(&window);
    if (err != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    return send_json(req, schedule_to_json(&window));
}

static esp_err_t override_post_handler(httpd_req_t *req)
{
    char buf[JSON_BODY_MAX_LEN];
    if (read_body(req, buf, sizeof(buf)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body missing or too large");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    cJSON *item = root ? cJSON_GetObjectItem(root, "on") : NULL;
    if (!root || !item || !cJSON_IsBool(item)) {
        if (root) {
            cJSON_Delete(root);
        }
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected JSON body: {\"on\": true|false}");
        return ESP_FAIL;
    }
    bool desired_on = cJSON_IsTrue(item);
    cJSON_Delete(root);

    light_state_set_override(desired_on, current_schedule_on());

    return status_get_handler(req);
}

static esp_err_t resume_post_handler(httpd_req_t *req)
{
    light_state_clear_override();
    return status_get_handler(req);
}

static esp_err_t temperature_get_handler(httpd_req_t *req)
{
    // Stub -- the DHT11 ambient temp sensor is owned but not wired into
    // firmware yet. Deliberately a 200 with available:false rather than an
    // HTTP error: "not implemented yet" isn't really a client error.
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "available", false);
    cJSON_AddStringToObject(root, "reason", "DHT11 driver not wired up yet");
    return send_json(req, root);
}

static esp_err_t ota_post_handler(httpd_req_t *req)
{
    if (req->content_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing firmware body");
        return ESP_FAIL;
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA update partition available -- was CONFIG_PARTITION_TABLE_TWO_OTA actually flashed?");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    if ((size_t)req->content_len > update_partition->size) {
        ESP_LOGE(TAG, "OTA image (%d bytes) too large for partition %s (%u bytes)",
                 req->content_len, update_partition->label, (unsigned)update_partition->size);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Firmware image too large for OTA partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA starting: %d bytes -> partition %s", req->content_len, update_partition->label);

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_partition, req->content_len, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buf[OTA_RECV_BUF_SIZE];
    int remaining = req->content_len;
    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, (int)sizeof(buf)));
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "OTA recv failed (%d), aborting", recv_len);
            esp_ota_abort(ota_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        err = esp_ota_write(ota_handle, buf, recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        remaining -= recv_len;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        // esp_ota_end() is what actually validates the image (size/SHA256/
        // magic byte) -- a failure here usually means a corrupt or
        // non-firmware body, not a transport error.
        ESP_LOGE(TAG, "esp_ota_end failed (bad image?): %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware image");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA complete, rebooting into %s", update_partition->label);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"rebooting\":true}");

    // Give the response a moment to actually flush over the socket before
    // esp_restart() cuts everything off.
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK; // unreachable
}

esp_err_t web_api_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_WC_HTTP_PORT;
    config.max_uri_handlers = 10;

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    static const httpd_uri_t routes[] = {
        { .uri = "/status", .method = HTTP_GET, .handler = status_get_handler },
        { .uri = "/schedule", .method = HTTP_GET, .handler = schedule_get_handler },
        { .uri = "/schedule", .method = HTTP_PUT, .handler = schedule_put_handler },
        { .uri = "/override", .method = HTTP_POST, .handler = override_post_handler },
        { .uri = "/resume", .method = HTTP_POST, .handler = resume_post_handler },
        { .uri = "/temperature", .method = HTTP_GET, .handler = temperature_get_handler },
        { .uri = "/ota", .method = HTTP_POST, .handler = ota_post_handler },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &routes[i]));
    }

    ESP_LOGI(TAG, "Web API listening on port %d", config.server_port);
    return ESP_OK;
}
