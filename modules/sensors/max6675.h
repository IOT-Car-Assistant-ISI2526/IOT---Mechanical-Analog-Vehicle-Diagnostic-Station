#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#define MAX6675_CS_PIN 15
#define MAX6675_FREQ_HZ 1000000 // 1 MHz

/**
 * @brief Initialize the MAX6675 sensor
 *
 * @param cfg Configuration struct
 * @param out_handle Pointer to store the resulting device handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max6675_init();

/**
 * @brief Delete the MAX6675 sensor device
 *
 * @param handle Device handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max6675_delete();
/**
 * @brief Check if thermocouple is open.
 * The third least-significant bit (bit 2) indicates an open thermocouple.
 * If thermocouple is open, the reading is invalid
 *
 * @param value Raw data value
 * @return true if thermocouple is open
 */
static bool check_open_thermocouple(uint16_t value);

/**
 * @brief Convert raw data to Celsius.
 * 1) The temperature bits are in D14..D3, so shift right by 3 to remove status bits.
 * 2) Multiply by 0.25 to convert to degrees Celsius (Each unit represents 0.25°C according to the MAX6675 datasheet).
 *
 * @param value Raw data value
 * @return float Temperature in Celsius
 */
static float convert_raw_data(uint16_t value);

/**
 * @brief Read temperature in Celsius
 * MAX6675 outputs 16 bits.
 * D15: Dummy Sign Bit
 * D14-D3: Temperature Data (12 bits)
 * D2: Thermocouple Input Bit (1 = Open circuit)
 * D1: Device ID
 * D0: State
 *
 * @param handle Device handle
 * @return float Temperature in Celsius, or -1.0 if reading failed/thermocouple open
 */
float max6675_read_celsius();