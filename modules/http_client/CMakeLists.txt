idf_component_register(
    SRCS "http_client.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES wifi_station freertos log lwip
)
