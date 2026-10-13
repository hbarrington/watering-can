idf_component_register(SRCS "main.c" "eth_bringup.c" "moisture_sensor.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_netif esp_driver_gpio esp_eth nvs_flash esp_event spi_flash esp_adc)
