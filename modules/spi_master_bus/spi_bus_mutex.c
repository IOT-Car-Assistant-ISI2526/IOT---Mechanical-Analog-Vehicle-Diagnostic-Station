#include "spi_bus_mutex.h"

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

    return ESP_OK;
}

void spi_bus_mutex_lock(void)
{
    if (s_spi_bus_mutex == NULL) {
        if (spi_bus_mutex_init() != ESP_OK) {
            return;
        }
    }

    xSemaphoreTake(s_spi_bus_mutex, portMAX_DELAY);
}

void spi_bus_mutex_unlock(void)
{
    if (s_spi_bus_mutex == NULL) {
        return;
    }

    xSemaphoreGive(s_spi_bus_mutex);
}
