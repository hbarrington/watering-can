#pragma once

#include "esp_err.h"

// Brings up WiFi station mode (using the example_connect() helper and its
// Kconfig-configured SSID/password) and starts SNTP so localtime() reflects
// real wall-clock time once synced. Blocks until WiFi is connected.
esp_err_t wifi_time_init(void);
