/* veml7700.c */
#include "veml7700.h"

static const char *TAG = "VEML7700";

static i2c_master_dev_handle_t veml7700_handle;

static esp_err_t write_reg(uint8_t reg, uint16_t val)
{
    uint8_t data[3];
    data[0] = reg;
    data[1] = (uint8_t)(val & 0xFF);        // LSB
    data[2] = (uint8_t)((val >> 8) & 0xFF); // MSB
    return i2c_master_transmit(veml7700_handle, data, sizeof(data), 1000);
}

static esp_err_t read_reg(uint8_t reg, uint16_t *val)
{
    uint8_t raw[2];
    esp_err_t ret = i2c_master_transmit_receive(veml7700_handle, &reg, 1, raw, 2, 1000);
    if (ret == ESP_OK)
    {
        *val = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8);
    }
    return ret;
}

esp_err_t veml7700_init(i2c_master_bus_handle_t bus_handle)
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = VEML7700_ADDR,
        .scl_speed_hz = VEML7700_SPEED_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &veml7700_handle));
    ESP_LOGI(TAG, "VEML7700 initialized on I2C address 0x%02X", VEML7700_ADDR);
    return ESP_OK;
}

esp_err_t veml7700_delete()
{
    return i2c_master_bus_rm_device(veml7700_handle);
}

void veml7700_wake_up()
{
    // Command 0x00 (ALS_CONF)
    // Data LSB: 0x00
    // Data MSB: 0x00
    uint8_t write_buf[3] = {0x00, 0x00, 0x00};

    ESP_ERROR_CHECK(i2c_master_transmit(veml7700_handle, write_buf, sizeof(write_buf), -1));

    ESP_LOGI("VEML7700", "Sensor powered ON.");
}

static float convert_raw_data(uint16_t raw_counts)
{
    float lux = raw_counts * LUX_RESOLUTION;

    if (lux > 1000.0f)
    {
        lux = (COEF_A * powf(lux, 4)) +
              (COEF_B * powf(lux, 3)) +
              (COEF_C * powf(lux, 2)) +
              (COEF_D * lux);
    }

    return lux;
}

float veml7700_read_lux()
{
    uint16_t raw_counts = 0;

    esp_err_t ret = read_reg(CMD_ALS_DATA, &raw_counts);
    if (ret != ESP_OK)
        return -1.0f;

    float lux = raw_counts * LUX_RESOLUTION;

    return convert_raw_data(raw_counts);
}