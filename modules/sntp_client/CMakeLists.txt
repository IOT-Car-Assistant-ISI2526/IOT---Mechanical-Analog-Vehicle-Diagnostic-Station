idf_component_register(
    SRCS "sntp_client.c"
    INCLUDE_DIRS "."
    REQUIRES esp_common nvs_flash esp_timer
)
