idf_component_register(
    SRCS "wifi_station.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES freertos esp_wifi esp_event esp_system log nvs_flash
)
