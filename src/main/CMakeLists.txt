idf_component_register(SRCS "main.c" "wifi_time.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_netif esp_driver_gpio esp_driver_uart esp_wifi vfs nvs_flash console esp_eth openthread esp_event
                       REQUIRES relay_controller light_schedule)
