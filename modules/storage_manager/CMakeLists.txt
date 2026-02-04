idf_component_register(
    SRCS "storage_manager.c"
    INCLUDE_DIRS "."
    REQUIRES fatfs sdmmc driver spi_master_bus spiffs
)
