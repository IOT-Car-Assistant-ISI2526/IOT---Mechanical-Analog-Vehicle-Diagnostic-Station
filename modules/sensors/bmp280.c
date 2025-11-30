/* bmp280.c */
#include "bmp280.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "BMP280";

/* Configuration */
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define BMP280_ADDR 0x77 // Change to 0x77 if 0x76 doesn't work!

/* Registers */
#define REG_DIG_T1 0x88
#define REG_ID 0xD0
#define REG_CTRL_MEAS 0xF4
#define REG_CONFIG 0xF5
#define REG_PRESS_MSB 0xF7

/* Calibration Data (Stored globally for simplicity) */
static uint16_t dig_T1;
static int16_t dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static int32_t t_fine;

/* I2C Helper Functions */
static esp_err_t read_regs(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, BMP280_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    uint8_t data[] = {reg, val};
    return i2c_master_write_to_device(I2C_MASTER_NUM, BMP280_ADDR, data, 2, pdMS_TO_TICKS(100));
}

static void parse_calibration_data(uint8_t *reg_data)
{
    dig_T1 = (reg_data[1] << 8) | reg_data[0];
    dig_T2 = (int16_t)((reg_data[3] << 8) | reg_data[2]);
    dig_T3 = (int16_t)((reg_data[5] << 8) | reg_data[4]);
    dig_P1 = (reg_data[7] << 8) | reg_data[6];
    dig_P2 = (int16_t)((reg_data[9] << 8) | reg_data[8]);
    dig_P3 = (int16_t)((reg_data[11] << 8) | reg_data[10]);
    dig_P4 = (int16_t)((reg_data[13] << 8) | reg_data[12]);
    dig_P5 = (int16_t)((reg_data[15] << 8) | reg_data[14]);
    dig_P6 = (int16_t)((reg_data[17] << 8) | reg_data[16]);
    dig_P7 = (int16_t)((reg_data[19] << 8) | reg_data[18]);
    dig_P8 = (int16_t)((reg_data[21] << 8) | reg_data[20]);
    dig_P9 = (int16_t)((reg_data[23] << 8) | reg_data[22]);
}

/* Public API Implementation */

esp_err_t bmp280_setup(int sda_pin, int scl_pin)
{
    // 1. Initialize I2C Bus
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    // Check if driver is already installed to avoid crashing in multi-sensor projects
    if (i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0) != ESP_OK)
    {
        i2c_param_config(I2C_MASTER_NUM, &conf);
        i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    }
    else
    {
        // Re-apply config if it was already installed
        i2c_param_config(I2C_MASTER_NUM, &conf);
    }

    // 2. Check Device ID
    uint8_t id = 0;
    if (read_regs(REG_ID, &id, 1) != ESP_OK)
    {
        ESP_LOGE(TAG, "Sensor not found at 0x%X", BMP280_ADDR);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Found BMP280, ID: 0x%02X", id);

    // 3. Read Calibration
    uint8_t cal_data[24];
    if (read_regs(REG_DIG_T1, cal_data, 24) != ESP_OK)
        return ESP_FAIL;
    parse_calibration_data(cal_data);

    // 4. Configure: Normal Mode, Temp Oversampling x2, Pressure Oversampling x16
    write_reg(REG_CONFIG, 0xA0);    // Standby 1000ms, Filter off
    write_reg(REG_CTRL_MEAS, 0x57); // osrs_t x2, osrs_p x16, normal mode

    return ESP_OK;
}

esp_err_t take_measurement(float *temperature, float *pressure)
{
    uint8_t raw_data[6];

    // Read Pressure (msb, lsb, xlsb) and Temp (msb, lsb, xlsb)
    if (read_regs(REG_PRESS_MSB, raw_data, 6) != ESP_OK)
        return ESP_FAIL;

    int32_t adc_P = (raw_data[0] << 12) | (raw_data[1] << 4) | (raw_data[2] >> 4);
    int32_t adc_T = (raw_data[3] << 12) | (raw_data[4] << 4) | (raw_data[5] >> 4);

    // --- Compensate Temperature ---
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;

    if (temperature)
    {
        *temperature = ((t_fine * 5 + 128) >> 8) / 100.0f;
    }

    // --- Compensate Pressure ---
    if (pressure)
    {
        int64_t p_var1, p_var2, p;
        p_var1 = (int64_t)t_fine - 128000;
        p_var2 = p_var1 * p_var1 * (int64_t)dig_P6;
        p_var2 = p_var2 + ((p_var1 * (int64_t)dig_P5) << 17);
        p_var2 = p_var2 + (((int64_t)dig_P4) << 35);
        p_var1 = ((p_var1 * p_var1 * (int64_t)dig_P3) >> 8) + ((p_var1 * (int64_t)dig_P2) << 12);
        p_var1 = (((((int64_t)1) << 47) + p_var1)) * ((int64_t)dig_P1) >> 33;

        if (p_var1 == 0)
        {
            *pressure = 0; // Avoid divide by zero
        }
        else
        {
            p = 1048576 - adc_P;
            p = (((p << 31) - p_var2) * 3125) / p_var1;
            p_var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
            p_var2 = (((int64_t)dig_P8) * p) >> 19;
            p = ((p + p_var1 + p_var2) >> 8) + (((int64_t)dig_P7) << 4);
            *pressure = (float)p / 256.0f;
        }
    }
    return ESP_OK;
}

void bmp280_task(void *arg)
{

    if (bmp280_setup(21, 22) == ESP_OK)
    {
        printf("BMP280 init success!\n");
        while (1)
        {
            float temperature = 0.0f, pressure = 0.0f;

            if (take_measurement(&temperature, &pressure) == ESP_OK)
            {
                ESP_LOGI(TAG, "Temp: %.2f C | Pres: %.2f Pa", temperature, pressure);
            }
            else
            {
                ESP_LOGE(TAG, "Measurement failed");
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
    else
    {
        printf("BMP280 init failed. Check wiring or I2C address.\n");
    }
}