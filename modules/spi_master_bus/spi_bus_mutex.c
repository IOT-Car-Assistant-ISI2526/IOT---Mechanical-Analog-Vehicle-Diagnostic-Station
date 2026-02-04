#include "spi_bus_mutex.h"
#include "esp_log.h"

static const char *TAG = "SPI_MUTEX";
static SemaphoreHandle_t s_spi_bus_mutex = NULL;

esp_err_t spi_bus_mutex_init(void)
{
    if (s_spi_bus_mutex != NULL) {
        return ESP_OK;
    }

    s_spi_bus_mutex = xSemaphoreCreateMutex();
    if (s_spi_bus_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "SPI Mutex initialized");
    return ESP_OK;
}

void spi_bus_mutex_lock(void)
{
    if (s_spi_bus_mutex == NULL) {
        if (spi_bus_mutex_init() != ESP_OK) {
            return;
        }
    }

    ESP_LOGI(TAG, "[LOCK] Attempting to acquire SPI mutex from task: %s", pcTaskGetName(NULL));
    xSemaphoreTake(s_spi_bus_mutex, portMAX_DELAY);
    ESP_LOGI(TAG, "[LOCK] SPI mutex acquired by task: %s", pcTaskGetName(NULL));
}

void spi_bus_mutex_unlock(void)
{
    if (s_spi_bus_mutex == NULL) {
        return;
    }

    ESP_LOGI(TAG, "[UNLOCK] Releasing SPI mutex from task: %s", pcTaskGetName(NULL));
    xSemaphoreGive(s_spi_bus_mutex);
    ESP_LOGI(TAG, "[UNLOCK] SPI mutex released by task: %s", pcTaskGetName(NULL));
}
