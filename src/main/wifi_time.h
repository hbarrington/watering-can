#pragma once

#include "esp_err.h"

// Brings up WiFi station mode (using the example_connect() helper and its
// Kconfig-configured SSID/password) and starts SNTP so localtime() reflects
// real wall-clock time once synced. Blocks until WiFi is connected AND
// until the first SNTP sync completes (or SNTP_FIRST_SYNC_TIMEOUT_MS
// elapses, logged loudly if so) -- callers can rely on localtime() being
// correct as soon as this returns, instead of racing the background sync.
esp_err_t wifi_time_init(void);
