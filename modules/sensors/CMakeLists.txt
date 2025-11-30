idf_component_register(
    SRCS "bmp280.c" "hcsr04.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos esp_timer esp_driver_i2c
    PRIV_REQUIRES vgerwen__hcsr04
)
