#pragma once

#include "esp_err.h"

// One-shot ADC read of the capacitive soil moisture sensor's analog
// output (GPIO set by CONFIG_WC_POT_MOISTURE_ADC_GPIO), calibrated to a
// 0-100% scale against CONFIG_WC_POT_MOISTURE_RAW_DRY/_WET (measured on
// hardware in air and fully submerged in water, respectively -- see
// Kconfig.projbuild for the readings those defaults came from).
esp_err_t moisture_sensor_init(void);

// Takes one reading. Any out-pointer may be NULL if not needed.
// mv_out is set to -1 if hardware calibration isn't available (eFuse not
// burnt) -- raw_out and percent_out are still valid in that case.
// percent_out is clamped to [0, 100] even if the raw reading falls
// outside the calibrated dry/wet bounds (e.g. a soil dryer than the
// air-calibration point, or a sensor fault) -- it never goes negative
// or over 100.
// Returns ESP_ERR_INVALID_STATE if moisture_sensor_init() wasn't called.
esp_err_t moisture_sensor_read(int *raw_out, int *mv_out, int *percent_out);
