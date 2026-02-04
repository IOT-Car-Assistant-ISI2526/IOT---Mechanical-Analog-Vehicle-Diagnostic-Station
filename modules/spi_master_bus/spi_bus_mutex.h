#ifndef SPI_BUS_MUTEX_H
#define SPI_BUS_MUTEX_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t spi_bus_mutex_init(void);
void spi_bus_mutex_lock(void);
void spi_bus_mutex_unlock(void);

#ifdef __cplusplus
}
#endif

#endif // SPI_BUS_MUTEX_H
