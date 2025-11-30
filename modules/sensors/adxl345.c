#include "adxl345.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <math.h>

// I2C Configuration
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_SCL_IO 22      // ESP32 GPIO for SCL
#define I2C_SDA_IO 21      // ESP32 GPIO for SDA
#define I2C_FREQ_HZ 100000 // I2C Speed (100kHz)

// ADXL345 Registers
#define REG_POWER_CTL 0x2D   // Power Control Register
#define REG_DATA_FORMAT 0x31 // Data Format Register
#define REG_DATAX0 0x32      // Start of Data Registers (X-Low)

#define ADXL345_LSB_TO_MS2 (0.004f * 9.80665f)
#define ADXL345_X_AXIS_CORRECTION -0.037f
#define ADXL345_Y_AXIS_CORRECTION -2.205f
#define ADXL345_EARTH_GRAVITY_MS2 8.318f
static const char *TAG = "ADXL345";

esp_err_t adxl345_init(void)
{
    // 3. Wake up the ADXL345
    // We write to the POWER_CTL register (0x2D)
    // Bit 3 (0x08) is the Measure bit. Setting it to 1 enables measurement.
    uint8_t data_enable[2] = {REG_POWER_CTL, 0x08};
    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_NUM, ADXL345_ADDR, data_enable, 2, 1000 / portTICK_PERIOD_MS);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "ADXL345 Initialized successfully");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize ADXL345");
    }

    return err;
}

esp_err_t adxl345_read_data(float *x, float *y, float *z)
{
    uint8_t reg = REG_DATAX0;
    uint8_t raw[6];

    esp_err_t err = i2c_master_write_read_device(
        I2C_MASTER_NUM, ADXL345_ADDR,
        &reg, 1, raw, 6,
        100 / portTICK_PERIOD_MS);

    if (err != ESP_OK) return err;

    int16_t rx = (int16_t)(raw[1] << 8 | raw[0]);
    int16_t ry = (int16_t)(raw[3] << 8 | raw[2]);
    int16_t rz = (int16_t)(raw[5] << 8 | raw[4]);

    *x = rx * ADXL345_LSB_TO_MS2;
    *y = ry * ADXL345_LSB_TO_MS2;
    *z = rz * ADXL345_LSB_TO_MS2;

    return ESP_OK;
}


void adxl345_task(void *arg)
{
    float combined_acceleration = 0.0f;
    float x, y, z;
    if (adxl345_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Sensor init failed! Restarting...");
    }


    while (1)
    {
        // Read data from sensor
        if (adxl345_read_data(&x, &y, &z) == ESP_OK)
        {
            x = x - ADXL345_X_AXIS_CORRECTION;
            y = y - ADXL345_Y_AXIS_CORRECTION;
            z = z - ADXL345_EARTH_GRAVITY_MS2;
            combined_acceleration = sqrtf(x*x + y*y + z*z);
            // Display data on Monitor
            // Note: These are raw values. For G-force, multiply by 0.0039 (approx 4mg/LSB)
            // ESP_LOGI(TAG, "X=%.3f m/s²  Y=%.3f m/s²  Z=%.3f m/s², ALL=%.3f m/s²", x, y, z, combined_acceleration);
            *(double *)arg = combined_acceleration;
        }

        // Wait for 500ms before next read
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void adxl345_start_task(double*parameter)
{
    xTaskCreate(adxl345_task, "ADXL345_Task", 4096, parameter, 10, NULL);
}