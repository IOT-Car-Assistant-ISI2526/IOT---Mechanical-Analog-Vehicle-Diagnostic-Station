/* bmp280.h */
#ifndef BMP280_H
#define BMP280_H

#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize the I2C driver and the BMP280 sensor.
     *
     * @param sda_pin GPIO number for SDA
     * @param scl_pin GPIO number for SCL
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t bmp280_setup(int sda_pin, int scl_pin);

    /**
     * @brief Reads the temperature and pressure.
     *
     * @param temperature Pointer to float where temperature (deg C) will be stored.
     * @param pressure Pointer to float where pressure (Pa) will be stored.
     * @return esp_err_t ESP_OK on success, ESP_FAIL on I2C error
     */
    esp_err_t take_measurement(float *temperature, float *pressure);

    void init_i2c_bus(void);

    void bmp280_task(void *arg);

    void bmp280_start_task(void);
    void bmp280_single_measurement(float *temperature, float *pressure);

#ifdef __cplusplus
}
#endif

#endif // BMP280_H