idf_component_register(
    SRCS "hcsr04.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos esp_timer
    PRIV_REQUIRES vgerwen__hcsr04 
)
