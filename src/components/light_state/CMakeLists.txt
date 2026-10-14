idf_component_register(SRCS "light_state.c"
                       INCLUDE_DIRS "include"
                       REQUIRES light_schedule
                       PRIV_REQUIRES nvs_flash)
