#include "spi_master_bus.h"

static const char *TAG = "SPI_MASTER_BUS";

esp_err_t spi_initialize_master(int miso, int mosi, int sck)
{
    spi_bus_config_t buscfg = {
        .miso_io_num = miso,
        .mosi_io_num = mosi,
        .sclk_io_num = sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = MAX_TRANSFER_SIZE,
    };
    
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    return ESP_OK;
}