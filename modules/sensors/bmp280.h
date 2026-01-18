#include <stdio.h>
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "esp_log.h"

#define BMP280_PORT I2C_NUM_0  /*!< I2C port number for BMP280 sensor */
#define BMP280_SPEED_HZ 100000 /*!< I2C clock line frequency for BMP280 sensor */
#define BMP280_ADDR 0x77       /*!< Address of the BMP280 sensor */
#define BMP280_ADDR_ALT 0x76   /*!< Alternate address of the BMP280 sensor */
#define BMP280_TEMP_MSB 0xFA   /*!< Address of the most significant bit temperature register */
#define BMP280_PRES_MSB 0xF7   /*!< Address of the most significant bit pressure register */
#define REG_CONFIG 0xF5        /*!< Address of the configuration register */
#define REG_CTRL_MEAS 0xF4     /*!< Address of the control measurement register */
#define BMP280_REG_STATUS 0xF3 /*!< Address of the status register */

/**
 * @brief Initialize and configure the BMP280 device on I2C bus. Then add the deivce to the bus
 *
 * @param bus_handle The I2C bus handle
 * @param addr The I2C address of the BMP280 device
 * @param speed The I2C clock line frequency of this device
 * @return **i2c_master_dev_handle_t**  - The device handle
 */
esp_err_t bmp280_init(i2c_master_bus_handle_t bus_handle, uint8_t address);
/**
 * @brief Delete the BMP280 device from the I2C bus to release the underlying hardware (reccommended to remove all attached
 * devices before deleting the bus).
 *
 * @param dev_handle The device handle
 * @return **esp_err_t** - ESP_OK on success, error code otherwise
 */
esp_err_t bmp280_delete(i2c_master_dev_handle_t dev_handle);

/**
 * @brief Read calibration data and configure the BMP280 device with default settings:
 * - IIR filter off
 * - Temperature oversampling x1
 * - Pressure oversampling x1
 * - Standby time 0.5ms
 * - Sleep mode
 *
 * @param dev_handle The device handle
 * @return **esp_err_t** - ESP_OK on success, error code otherwise
 */
esp_err_t bmp280_configure();

/**
 * @brief Trigger sleep mode.
 * No measurements are performed
 *
 * @param dev_handle
 * @return **esp_err_t** - ESP_OK on success, error code otherwise
 */
esp_err_t bmp280_trigger_sleep_mode();
/**
 * @brief Trigger forced mode.
 * Take one measurement and return to sleep mode
 *
 * @param dev_handle The device handle
 * @return **esp_err_t** - ESP_OK on success, error code otherwise
 */
esp_err_t bmp280_trigger_forced_mode();
/**
 * @brief Trigger normal mode.
 * Automated cycling between an active measurement periods and an inactive standby periods
 *
 * @param dev_handle The device handle
 * @return **esp_err_t** - ESP_OK on success, error code otherwise
 */
esp_err_t bmp280_trigger_normal_mode();

/**
 * @brief Take one measurement of temperature in Celsius
 *
 * @param dev_handle The device handle
 * @return **double**  - Temperature in Celsius
 */
float bmp280_read_temp();
/**
 * @brief Take one measurement of pressure in hPa
 *
 * @param dev_handle The device handle
 * @return **float**  - Pressure in hPa
 */
float bmp280_read_pres();

/**
 * @brief Change the oversampling setting for temperature measurements.
 *
 * Possible values:
 * - 0 -> skipped
 * - 1 -> x1
 * - 2 -> x2
 * - 3 -> x4
 * - 4 -> x8
 * - 5 -> x16
 *
 * @param dev_handle The device handle
 * @param resolution The new temperature resolution
 * @return **esp_err_t** - ESP_OK on success, error code otherwise
 */
esp_err_t bmp280_change_temp_resolution(uint8_t resolution);
/**
 * @brief Change the oversampling setting for pressure measurements.
 *
 * Possible values:
 * - 0 -> skipped
 * - 1 -> x1
 * - 2 -> x2
 * - 3 -> x4
 * - 4 -> x8
 * - 5 -> x16
 *
 * @param dev_handle The device handle
 * @param resolution The new pressure resolution
 * @return **esp_err_t** - ESP_OK on success, error code otherwise
 */
esp_err_t bmp280_change_pres_resolution(uint8_t resolution);
/**
 * @brief Change IIR filter coefficient.
 *
 * Possible values:
 * - 0 -> filter off
 * - 1 -> coefficient 2
 * - 2 -> coefficient 4
 * - 3 -> coefficient 8
 * - 4 -> coefficient 16
 *
 * @param dev_handle The device handle
 * @param filter_value The new filter value
 * @return **esp_err_t** - ESP_OK on success, error code otherwise
 */
esp_err_t bmp280_trigger_filter(uint8_t filter_value);
/**
 * @brief Change standby time.
 *
 * Possible values:
 * - 0 -> 0.5ms
 * - 1 -> 62.5ms
 * - 2 -> 125ms
 * - 3 -> 250ms
 * - 4 -> 500ms
 * - 5 -> 1000ms
 * - 6 -> 2000ms
 * - 7 -> 4000ms
 *
 * @param dev_handle The device handle
 * @param time The new standby time
 * @return **esp_err_t** - ESP_OK on success, error code otherwise
 */
esp_err_t bmp280_change_standby_time(uint8_t time);