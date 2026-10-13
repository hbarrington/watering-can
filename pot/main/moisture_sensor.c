#include "moisture_sensor.h"

#include <stdbool.h>
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "moisture";

// Covers the full 0-3.3V input range.
#define MOISTURE_ATTEN ADC_ATTEN_DB_12

// Calibration bounds, measured on real hardware (see Kconfig.projbuild
// for the readings these defaults came from). Raw counts, NOT mv --
// clamping happens in raw-count space since that's what the sensor
// actually reports; mv is a calibrated courtesy value for the log, not
// what percent is computed from.
#define MOISTURE_RAW_DRY CONFIG_WC_POT_MOISTURE_RAW_DRY
#define MOISTURE_RAW_WET CONFIG_WC_POT_MOISTURE_RAW_WET

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;
static adc_channel_t s_channel;
static bool s_calibrated = false;
static bool s_inited = false;

// Maps a raw ADC reading to 0-100%, clamped at both ends. Higher raw
// (drier) -> lower percent; lower raw (wetter) -> higher percent --
// matches this sensor's actual behavior (capacitance rises, output
// voltage falls, as moisture increases).
static int raw_to_percent(int raw)
{
    if (raw >= MOISTURE_RAW_DRY) {
        return 0;
    }
    if (raw <= MOISTURE_RAW_WET) {
        return 100;
    }
    return (int)(((int64_t)(MOISTURE_RAW_DRY - raw) * 100) / (MOISTURE_RAW_DRY - MOISTURE_RAW_WET));
}

static bool calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten)
{
    esp_err_t ret = ESP_FAIL;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .chan = channel,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &s_cali_handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &s_cali_handle);
#endif

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration ready");
        return true;
    }
    ESP_LOGW(TAG, "ADC calibration unavailable (eFuse not burnt?) -- raw counts only");
    return false;
}

esp_err_t moisture_sensor_init(void)
{
    int gpio = CONFIG_WC_POT_MOISTURE_ADC_GPIO;
    adc_unit_t unit;
    esp_err_t err = adc_oneshot_io_to_channel(gpio, &unit, &s_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO %d is not ADC-capable: %s", gpio, esp_err_to_name(err));
        return err;
    }

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = unit,
    };
    err = adc_oneshot_new_unit(&init_config, &s_adc_handle);
    if (err != ESP_OK) {
        return err;
    }

    adc_oneshot_chan_cfg_t chan_config = {
        .atten = MOISTURE_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(s_adc_handle, s_channel, &chan_config);
    if (err != ESP_OK) {
        return err;
    }

    s_calibrated = calibration_init(unit, s_channel, MOISTURE_ATTEN);
    s_inited = true;
    ESP_LOGI(TAG, "moisture sensor on GPIO %d (ADC unit %d, channel %d)", gpio, unit + 1, s_channel);
    return ESP_OK;
}

esp_err_t moisture_sensor_read(int *raw_out, int *mv_out, int *percent_out)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    int raw = 0;
    esp_err_t err = adc_oneshot_read(s_adc_handle, s_channel, &raw);
    if (err != ESP_OK) {
        return err;
    }
    if (raw_out) {
        *raw_out = raw;
    }

    if (mv_out) {
        if (s_calibrated) {
            int mv = 0;
            err = adc_cali_raw_to_voltage(s_cali_handle, raw, &mv);
            if (err != ESP_OK) {
                return err;
            }
            *mv_out = mv;
        } else {
            *mv_out = -1;
        }
    }

    if (percent_out) {
        *percent_out = raw_to_percent(raw);
    }

    return ESP_OK;
}
