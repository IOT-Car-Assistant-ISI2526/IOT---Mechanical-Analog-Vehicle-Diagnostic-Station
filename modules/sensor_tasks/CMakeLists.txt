idf_component_register(
    SRCS "bmp280_task.c" "max6675_task.c" "veml7700_task.c" "adxl345_task.c"
    INCLUDE_DIRS "."
    REQUIRES "sensors" "ble_service" "main"
)