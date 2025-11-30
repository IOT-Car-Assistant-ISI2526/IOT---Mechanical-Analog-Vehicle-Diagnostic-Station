/* veml7700.c */
#include "veml7700.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <math.h> // Needed for the correction formula (powf)

static const char *TAG = "VEML7700";

/* I2C Settings */
#define I2C_PORT I2C_NUM_1
#define I2C_FREQ_HZ 100000
#define VEML7700_ADDR 0x10 // Fixed I2C address for VEML7700

/* Registers */
#define CMD_ALS_CONF 0x00 // Configuration Register
#define CMD_ALS_DATA 0x04 // Ambient Light Data

/*
 * Configuration Constants
 * We use Gain 1/8 and Integration Time 100ms.
 * This gives a range up to ~37,000 Lux (Good for indoors and outdoors).
 * Resolution = 0.576 lux/count.
 */
#define CONF_GAIN_1_8 (0x02 << 11) // Bits 11:12
#define CONF_IT_100MS (0x00 << 6)  // Bits 6:9
#define CONF_SHUTDOWN (0x01)       // Bit 0

#define LUX_RESOLUTION 0.576f

/* Non-linear correction coefficients (from Vishay Datasheet) */
#define COEF_A 6.0135e-13
#define COEF_B -9.3924e-9
#define COEF_C 8.1488e-5
#define COEF_D 1.0023

// --- Helper: Write 16-bit word ---
static esp_err_t write_reg(uint8_t reg, uint16_t val)
{
    uint8_t data[3];
    data[0] = reg;
    data[1] = (uint8_t)(val & 0xFF);        // LSB
    data[2] = (uint8_t)((val >> 8) & 0xFF); // MSB
    return i2c_master_write_to_device(I2C_PORT, VEML7700_ADDR, data, 3, pdMS_TO_TICKS(100));
}

// --- Helper: Read 16-bit word ---
static esp_err_t read_reg(uint8_t reg, uint16_t *val)
{
    uint8_t raw[2];
    esp_err_t ret = i2c_master_write_read_device(I2C_PORT, VEML7700_ADDR, &reg, 1, raw, 2, pdMS_TO_TICKS(100));
    if (ret == ESP_OK)
    {
        *val = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8);
    }
    return ret;
}

// --- Public: Init ---
esp_err_t veml7700_init(int sda_pin, int scl_pin)
{
    // 1. Setup I2C Config
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    // Install driver (checks if already installed to be safe)
    // if (i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0) != ESP_OK)
    // {
    //     i2c_param_config(I2C_PORT, &conf);
    //     i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
    // }
    // else
    // {
    //     i2c_param_config(I2C_PORT, &conf);
    // }

    // 2. Configure Sensor
    // Value: Gain 1/8 | IT 100ms | No Interrupts | Power On (Shutdown bit = 0)
    uint16_t config_val = CONF_GAIN_1_8 | CONF_IT_100MS;

    // Note: We write 0 to the shutdown bit implicitly to turn it on
    esp_err_t ret = write_reg(CMD_ALS_CONF, config_val);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write config. Check wiring.");
        return ret;
    }

    // Wait 5ms for power up (as per datasheet)
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG, "Initialized. Gain: 1/8, IT: 100ms");
    return ESP_OK;
}

// --- Public: Read Lux ---
esp_err_t veml7700_read_lux(float *lux_val)
{
    uint16_t raw_counts = 0;

    // 1. Read raw data
    esp_err_t ret = read_reg(CMD_ALS_DATA, &raw_counts);
    if (ret != ESP_OK)
        return ret;

    // 2. Convert to basic Lux
    float lux = raw_counts * LUX_RESOLUTION;

    // 3. Apply non-linear correction (Required by datasheet for VEML7700)
    // This sensor is non-linear at higher light levels.
    if (lux > 1000.0f)
    {
        lux = (COEF_A * powf(lux, 4)) +
              (COEF_B * powf(lux, 3)) +
              (COEF_C * powf(lux, 2)) +
              (COEF_D * lux);
    }

    *lux_val = lux;
    return ESP_OK;
}

void veml7700_task(void *arg)
{
    // 1. Initialize sensor
    if (veml7700_init(25, 26) == ESP_OK)
    {
        ESP_LOGI(TAG, "Sensor Init Success!");
    }
    else
    {
        ESP_LOGE(TAG, "Sensor Init Failed!");
    }

    // 2. Loop to take measurements
    while (1)
    {
        float lux = 0.0;

        // This is the function you wanted
        if (veml7700_read_lux(&lux) == ESP_OK)
        {
            ESP_LOGI(TAG, "Light Level: %.2f Lux", lux);
            // TODO: save the light measurement
            vTaskDelay(pdMS_TO_TICKS(3600000));
        }
        else
        {
            ESP_LOGE(TAG, "Failed to read sensor");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void veml7700_start_task()
{
    xTaskCreate(veml7700_task, "VEML7700_Task", 4096, NULL, 10, NULL);
}