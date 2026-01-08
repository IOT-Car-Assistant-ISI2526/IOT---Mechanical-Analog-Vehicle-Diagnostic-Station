idf_component_register(
    SRCS "mqtt_client_app.c"
    PRIV_REQUIRES
        mqtt
        esp_partition
        nvs_flash
        esp_netif
        app_update
        esp_timer
        storage_manager
        wifi_station
        log
        ble_service
        buzzer
    INCLUDE_DIRS
        "."
)