idf_component_register(SRCS "web_api.c"
                       INCLUDE_DIRS "include"
                       REQUIRES light_state light_schedule
                       PRIV_REQUIRES esp_http_server app_update esp_app_format json esp_timer esp_partition)
