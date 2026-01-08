#include "max6675.h"

static const char *TAG = "MAX6675";

spi_device_handle_t max6675_handle;

esp_err_t max6675_init()
{
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = MAX6675_FREQ_HZ,
        .mode = 0, // SPI Mode 0: CPOL=0, CPHA=0
        .spics_io_num = MAX6675_CS_PIN,
        .queue_size = 1,
    };

    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &max6675_handle));
    ESP_LOGI(TAG, "device added to SPI bus");
    return ESP_OK;
}

esp_err_t max6675_delete()
{
    return spi_bus_remove_device(max6675_handle);
}

static bool check_open_thermocouple(uint16_t value)
{
    if (value & 0x04)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static float convert_raw_data(uint16_t value)
{
    value >>= 3;
    return value * 0.25f;
}

float max6675_read_celsius()
{
    uint8_t raw_data[2] = {0};

    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_RXDATA,
        .length = 16, // Read 16 bits
        .rxlength = 16};

    if (spi_device_transmit(max6675_handle, &t) != ESP_OK)
    {
        return -1.0f;
    }

    // Combine bytes (Big Endian)
    // `t.rx_data` contains the received bytes (big-endian): first byte = high 8 bits
    uint16_t value = (t.rx_data[0] << 8) | t.rx_data[1];

    if (check_open_thermocouple(value))
    {
        return -1.0f; // Error sentinel: thermocouple open or disconnected
    }

    return convert_raw_data(value);
}