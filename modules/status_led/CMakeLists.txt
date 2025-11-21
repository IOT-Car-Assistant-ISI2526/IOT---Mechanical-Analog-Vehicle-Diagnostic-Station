idf_component_register(
    SRCS "status_led.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver wifi_station freertos log
)
