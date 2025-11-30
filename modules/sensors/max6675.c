#include "max6675.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

#define PIN_CLK 14
#define PIN_CS 15
#define PIN_SO 12 // MISO (Master In Slave Out) - data from MAX6675 to ESP32
#define MY_SPI_HOST SPI2_HOST

static const char *TAG = "MAX6675";

esp_err_t max6675_init(const max6675_config_t *cfg, max6675_handle_t *out_handle)
{
    // We assume main.c ran spi_bus_initialize().
    // We only add the device here.

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,      // 1 MHz
        .mode = 0,                      // SPI Mode 0
        .spics_io_num = cfg->cs_io_num, // CS Pin
        .queue_size = 1,                // Queue 1 transaction
    };

    // Add device to the SPI Host
    esp_err_t ret = spi_bus_add_device(cfg->spi_host, &devcfg, &out_handle->spi_dev);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add device to SPI bus. Check main.c SPI init.");
    }

    return ret;
}

float max6675_read_celsius(max6675_handle_t *handle)
{
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_RXDATA,
        .length = 16,
        .rxlength = 16};

    esp_err_t ret = spi_device_transmit(handle->spi_dev, &t);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI Transmission Failed");
        return -1.0f;
    }

    uint16_t value = (t.rx_data[0] << 8) | t.rx_data[1];

    // D2 is Open Circuit Flag
    if (value & 0x04)
    {
        return -1.0f; // Error: Thermocouple unplugged
    }

    value >>= 3;          // Discard status bits
    return value * 0.25f; // 0.25 C per LSB
}

void max6675_task(void *arg)
{

    max6675_config_t config = {
        // SCK and MISO are ignored by our new init function,
        // but we keep CS and HOST.
        .cs_io_num = PIN_CS,
        .spi_host = MY_SPI_HOST};

    max6675_handle_t sensor;

    if (max6675_init(&config, &sensor) != ESP_OK)
    {
        ESP_LOGE(TAG, "Init Failed! Deleting Task.");
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "Initialized. Reading...");

    while (1)
    {
        float temp = max6675_read_celsius(&sensor);

        if (temp < 0)
        {
            ESP_LOGE(TAG, "Error: Thermocouple open or disconnected");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else
        {
            ESP_LOGI(TAG, "Temperature: %.2f C", temp);
            // TODO: save the temaperature measurement
            vTaskDelay(pdMS_TO_TICKS(3600000));
        }
    }
}

void max6675_start_task(void)
{
    xTaskCreate(max6675_task, "max6675_task", 4096, NULL, 5, NULL);
}