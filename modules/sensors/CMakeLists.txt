idf_component_register(
    SRCS "bmp280.c" "hcsr04.c" "veml7700.c" "max6675.c" "adxl345.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos esp_timer esp_driver_i2c buzzer
    PRIV_REQUIRES vgerwen__hcsr04 storage_manager
)
