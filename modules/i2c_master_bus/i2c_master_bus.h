#include "driver/i2c_master.h"
#include "esp_log.h"

/* --- I2C for BMP280 & ADXL345 --- */
#define I2C_PORT_0_SDA_PIN 21
#define I2C_PORT_0_SCL_PIN 22

/* --- I2C for VEML7700 --- */
#define I2C_PORT_1_SDA_PIN 25
#define I2C_PORT_1_SCL_PIN 26

i2c_master_bus_handle_t i2c_initialize_master(i2c_port_t port, int sda, int scl);

// esp_err_t init_i2c_bus(i2c_port_t port, int sda, int scl, uint32_t freq);