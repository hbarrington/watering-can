#pragma once

#include "esp_err.h"

// Starts the HTTP API server:
//   GET  /status       -- lights_on, schedule, override state, firmware version, uptime
//   GET  /schedule      -- current ON/OFF window
//   PUT  /schedule       -- update the window (any subset of on_hour/on_minute/off_hour/off_minute),
//                          persists to NVS
//   POST /lights        -- {"on": true|false}; forces lights to that state until the
//                          next scheduled transition, then auto-clears
//   POST /resume         -- clears an active override immediately
//   GET  /temperature    -- stub; DHT11 driver isn't wired up yet
//   POST /ota            -- raw firmware image in the body; writes it to the next OTA
//                          partition and reboots into it on success
//
// No authentication -- same LAN-trust model as the rest of this project
// (WiFi password is the only gate). Anyone on the network can query/control
// this, including pushing new firmware via /ota.
//
// Requires WiFi to already be up (wifi_time_init()) and light_state_init()
// to have already run.
esp_err_t web_api_start(void);
