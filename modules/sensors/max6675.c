#include "project_config.h"

#include "max6675.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "storage_manager.h"
#include <string.h>
#include <freertos/semphr.h>

#define PIN_CLK 14
#define PIN_CS 15
#define PIN_SO 12 // MISO (Master In Slave Out) - data from MAX6675 to ESP32
#define MY_SPI_HOST SPI2_HOST

static const char *TAG = "MAX6675";
static SemaphoreHandle_t max6675_mutex = NULL;

esp_err_t max6675_init(const max6675_config_t *cfg, max6675_handle_t *out_handle)
{

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,      // 1 MHz
        .mode = 0,                      // SPI Mode 0
        .spics_io_num = cfg->cs_io_num, // CS Pin
        .queue_size = 1,                // Queue 1 transaction
    };

    esp_err_t ret = spi_bus_add_device(cfg->spi_host, &devcfg, &out_handle->spi_dev);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add device to SPI bus. Check main.c SPI init.");
    }
    return ret;
}

float max6675_read_celsius(max6675_handle_t *handle)
{
    // Wait max 100ms for access
    if (xSemaphoreTake(max6675_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "MAX6675 busy");
        return -1.0f;
    }
    
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_RXDATA,
        .length = 16,
        .rxlength = 16};

    esp_err_t ret = spi_device_transmit(handle->spi_dev, &t);
    xSemaphoreGive(max6675_mutex);

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
            vTaskDelay(pdMS_TO_TICKS(INCORRECT_MEASUREMENT_INTERVAL_MS));
        }
        else
        {
            *(float *)arg = temp;


            static int max_saved_count = 0;
            if (max_saved_count < MAX6675_SAVE_LIMIT) {
                char line[80];
                int64_t ts = esp_timer_get_time();
                int r = snprintf(line, sizeof(line), "MAX_NORMAL;%lld;%.2f", (long long)ts, temp);
                if (r > 0) {
                    if (storage_write_line(line)) {
                        max_saved_count++;
                        ESP_LOGI(TAG, "Saved MAX6675 normal sample %d/%d", max_saved_count, MAX6675_SAVE_LIMIT);
                    } else {
                        ESP_LOGW(TAG, "Failed to save MAX6675 normal sample (no space)");
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(MAX6675_MEASUREMENT_INTERVAL_MS));
        }
    }
}

void max6675_profile_task(void *arg)
{
    max6675_sample_t *results = (max6675_sample_t *)arg;

    max6675_config_t config = {
        .cs_io_num = PIN_CS,
        .spi_host = MY_SPI_HOST
    };

    max6675_handle_t sensor;

    if (max6675_init(&config, &sensor) != ESP_OK)
    {
        ESP_LOGE(TAG, "Init Failed! Deleting Task.");
        vTaskDelete(NULL);
    }
    TickType_t start_ticks = xTaskGetTickCount();

    int measurement_idx = 0;
    while (measurement_idx < MAX6675_PROFILE_SAMPLES_COUNT)
    {
        float temp = max6675_read_celsius(&sensor);

        if (temp < 0)
        {
            ESP_LOGE(TAG, "Error: Thermocouple open or disconnected");
            vTaskDelay(pdMS_TO_TICKS(INCORRECT_MEASUREMENT_INTERVAL_MS));
            continue;
        }

        TickType_t now = xTaskGetTickCount();
        uint32_t elapsed_ms = (now - start_ticks) * (1000 / configTICK_RATE_HZ);

        results[measurement_idx].temperature = temp;
        results[measurement_idx].seconds = elapsed_ms / 1000;

        char line[80];
        int64_t ts = esp_timer_get_time();
        int r = snprintf(line, sizeof(line), "MAX_PROFILE;%lld;%.2f;idx=%d;sec=%lu", (long long)ts, temp, measurement_idx, results[measurement_idx].seconds);
        if (r > 0) {
            if (!storage_write_line(line)) {
                ESP_LOGW(TAG, "Failed to save MAX6675 profile sample (no space)");
            }
        }

        measurement_idx++;

        vTaskDelay(pdMS_TO_TICKS(FREQUENT_MEASUREMENT_INTERVAL_MS));
    }
    vTaskDelete(NULL);
}


void max6675_start_task(float *parameter)
{
    if (max6675_mutex == NULL)
        max6675_mutex = xSemaphoreCreateMutex();
    xTaskCreate(max6675_task, "max6675_task", 4096, parameter, 5, NULL);
}

void max6675_start_profile_task(max6675_sample_t *result_array)
{
    if (max6675_mutex == NULL)
        max6675_mutex = xSemaphoreCreateMutex();
    xTaskCreate(max6675_profile_task, "max6675_profile_task", 4096, result_array, 5, NULL);
}