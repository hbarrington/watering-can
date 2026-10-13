#pragma once

#include "esp_err.h"

// Brings up the onboard Ethernet (internal EMAC + IP101 PHY, over the
// Unit PoE-P4's RJ45/PoE port) using the example_connect() helper and its
// Kconfig-configured pins (see sdkconfig.defaults). Logs link up/down and
// the DHCP-assigned IP. Blocks until link + DHCP succeed (example_connect()
// has its own internal retry/timeout handling).
//
// No SNTP/time sync here yet -- add it when sensor readings actually need
// wall-clock timestamps (mirrors src/wifi_time.c's pattern if/when that
// happens).
esp_err_t eth_bringup_init(void);
