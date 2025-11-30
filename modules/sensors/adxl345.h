#ifndef ADXL345_H
#define ADXL345_H

#include <stdint.h>
#include "esp_err.h"

// ADXL345 I2C Address (Assumes ALT ADDRESS pin is Grounded)
#define ADXL345_ADDR 0x53

// Structure to hold accelerometer data
typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} adxl345_data_t;

/**
 * @brief Initialize the ADXL345 sensor
 *Configures the I2C bus and wakes up the sensor.
 */
esp_err_t adxl345_init(void);

/**
 * @brief Read X, Y, Z data from the sensor
 * @param data Pointer to the structure to store the results
 */
esp_err_t adxl345_read_data(float *x, float *y, float *z);

void adxl345_task(void *arg);

void adxl345_start_task(double *parameter);

#endif // ADXL345_H