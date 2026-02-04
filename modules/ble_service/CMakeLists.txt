idf_component_register(
    SRCS "ble_server_core.c" "ble_services.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash bt storage_manager wifi_station sntp_client
)