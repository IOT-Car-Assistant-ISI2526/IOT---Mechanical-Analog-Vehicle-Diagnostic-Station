#include <esp_err.h>
#include <math.h>
#include "driver/i2c_master.h"
#include "esp_log.h"

#define VEML7700_PORT I2C_NUM_1
#define VEML7700_SPEED_HZ 100000
#define VEML7700_ADDR 0x10
#define CMD_ALS_CONF 0x00
#define CMD_ALS_DATA 0x04

#define CONF_GAIN_1_8 (0x02 << 11)
#define CONF_IT_100MS (0x00 << 6)
#define CONF_SHUTDOWN (0x01)

#define LUX_RESOLUTION 0.576f

#define COEF_A 6.0135e-13
#define COEF_B -9.3924e-9
#define COEF_C 8.1488e-5
#define COEF_D 1.0023

/**
 * @brief Initialize and add VEML7700 device to I2C bus.
 *
 * @param bus_handle I2C master bus handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t veml7700_init(i2c_master_bus_handle_t bus_handle);

/**
 * @brief Delete the VEML7700 device from the I2C bus to release the underlying hardware.
 *
 * @param dev_handle The device handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t veml7700_delete();

void veml7700_wake_up();

/**
 * @brief Reads the Ambient Light in Lux.
 *
 * Includes automatic correction for high-brightness non-linearity.
 *
 * @param lux_val Pointer to a float where the result will be stored.
 * @return esp_err_t ESP_OK on success
 */
float veml7700_read_lux();