#pragma once

#include "esp_err.h"

// One-shot ADC read of the capacitive soil moisture sensor's analog
// output (GPIO set by CONFIG_WC_POT_MOISTURE_ADC_GPIO). Deliberately not
// mapped to a 0-100% moisture scale yet -- this phase is just getting
// real raw-count/voltage numbers onto the serial log so we can see the
// sensor's actual dry/wet range on real hardware before calibrating.
esp_err_t moisture_sensor_init(void);

// Takes one reading. Either out-pointer may be NULL if not needed.
// mv_out is set to -1 if hardware calibration isn't available (eFuse not
// burnt) -- raw_out is still valid in that case.
// Returns ESP_ERR_INVALID_STATE if moisture_sensor_init() wasn't called.
esp_err_t moisture_sensor_read(int *raw_out, int *mv_out);
