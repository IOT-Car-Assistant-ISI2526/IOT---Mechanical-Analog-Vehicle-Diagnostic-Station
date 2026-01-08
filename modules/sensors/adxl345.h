#include <stdint.h>
#include <math.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

// ADXL345 I2C Address (Assumes ALT ADDRESS pin is Grounded)
#define ADXL345_PORT I2C_NUM_0
#define ADXL345_SPEED_HZ 100000 // I2C Speed (100kHz)
#define ADXL345_ADDR 0x53
// #define I2C_SCL_IO 22      // ESP32 GPIO for SCL
// #define I2C_SDA_IO 21      // ESP32 GPIO for SDA

// ADXL345 Registers
#define REG_POWER_CTL 0x2D // Power Control Register
#define BW_RATE 0x2C       // Data Rate

#define REG_DATA_FORMAT 0x31 // Data Format Register
#define REG_DATAX0 0x32      // Start of Data Registers (X-Low)
#define OFFSET_X 0x1E        // X-Axis Offset
#define OFFSET_Y 0x1F        // Y-Axis Offset
#define OFFSET_Z 0x20        // Z-Axis Offset

#define THRESH_ACT 0x24    // Activity Threshold
#define THRESH_INACT 0x25  // Inactivity Threshold
#define TIME_INACT 0x26    // Inactivity Time
#define ACT_INACT_CTL 0x27 // Activity/Inactivity Control

#define ADXL345_LSB_TO_MS2 (0.004f * 9.80665f)
#define ADXL345_X_AXIS_CORRECTION -0.037f
#define ADXL345_Y_AXIS_CORRECTION -2.205f
#define ADXL345_EARTH_GRAVITY_MS2 8.318f

esp_err_t adxl345_init(i2c_master_bus_handle_t bus_handle);
esp_err_t adxl345_delete();
esp_err_t adxl345_configure();

esp_err_t adxl345_set_measurement_mode();
esp_err_t adxl345_set_standby_mode();
esp_err_t adxl345_set_sleep_mode(bool enable);
esp_err_t adxl345_set_low_power_mode(bool enable);
esp_err_t adxl345_set_low_power_rate(uint8_t rate);

esp_err_t adxl345_set_activity_threshold(uint8_t threshold);
esp_err_t adxl345_set_inactivity_threshold(uint8_t threshold);
esp_err_t adxl345_set_inactivity_time(uint8_t time);

esp_err_t adxl345_set_frequency_in_sleep_mode(uint8_t frequency);
esp_err_t adxl345_enable_auto_sleep(bool enable);
esp_err_t adxl345_enable_all_axis_activity_detection();

float adxl345_read_data();