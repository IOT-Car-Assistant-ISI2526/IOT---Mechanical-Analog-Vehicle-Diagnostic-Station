#include <stdint.h>
#include <math.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

// ADXL345 I2C Address (Assumes ALT ADDRESS pin is Grounded)
#define ADXL345_PORT I2C_NUM_0
#define ADXL345_SPEED_HZ 100000
#define ADXL345_ADDR 0x53


// ADXL345 Registers
#define REG_POWER_CTL 0x2D
#define BW_RATE 0x2C

#define REG_DATA_FORMAT 0x31
#define REG_DATAX0 0x32
#define OFFSET_X 0x1E
#define OFFSET_Y 0x1F
#define OFFSET_Z 0x20

#define THRESH_ACT 0x24
#define THRESH_INACT 0x25
#define TIME_INACT 0x26
#define ACT_INACT_CTL 0x27

#define ADXL345_LSB_TO_MS2 (0.004f * 9.80665f)
// #define ADXL345_X_AXIS_CORRECTION -0.039f
// #define ADXL345_Y_AXIS_CORRECTION -2.760f
// #define ADXL345_EARTH_GRAVITY_MS2 8.318f
#define ADXL345_X_AXIS_CORRECTION 0.0f
#define ADXL345_Y_AXIS_CORRECTION 0.0f
#define ADXL345_EARTH_GRAVITY_MS2 0.0f

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