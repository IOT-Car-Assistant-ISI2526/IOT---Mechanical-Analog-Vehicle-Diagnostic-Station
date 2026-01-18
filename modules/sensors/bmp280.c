#include "bmp280.h"

typedef struct
{
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
} bmp280_calib_data_t;

bmp280_calib_data_t cal_data; // Global instance
static int32_t t_fine;

static const char *TAG = "BMP280";

uint8_t filter_value = 0; // Default filter coefficient
uint8_t osrs_t = 1;       // Default temperature oversampling x1
uint8_t osrs_p = 1;       // Default pressure oversampling x1
uint8_t standby = 0;      // Default standby time 1000ms
uint8_t mode = 0;         // Default mode sleep
uint8_t spi = 0;          // Default SPI disabled

static i2c_master_dev_handle_t bmp280_handle;

static esp_err_t read_register_bmp280(uint8_t reg_addr, uint8_t *data, size_t len);
static esp_err_t write_register_bmp280(uint8_t reg_addr, uint8_t value);

static void read_calibration_data();
static esp_err_t configure_ctrl_meas();
static esp_err_t configure_config();

static float convert_temperature(int32_t raw_temp);
static float convert_pressure(int32_t raw_pres);

esp_err_t bmp280_init(i2c_master_bus_handle_t bus_handle, uint8_t address)
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = BMP280_SPEED_HZ,
    };

    esp_err_t err = i2c_master_bus_add_device(bus_handle, &dev_config, &bmp280_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add BMP280 device to I2C bus: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "BMP280 initialized on I2C address 0x%02X", address);
    return ESP_OK;
}

esp_err_t bmp280_delete(i2c_master_dev_handle_t dev_handle)
{
    return i2c_master_bus_rm_device(bmp280_handle);
}

static esp_err_t read_register_bmp280(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(bmp280_handle, &reg_addr, 1, data, len, 1000);
}

static esp_err_t write_register_bmp280(uint8_t reg_addr, uint8_t value)
{
    uint8_t buf[2] = {reg_addr, value};
    return i2c_master_transmit(bmp280_handle, buf, sizeof(buf), 1000);
}

static void read_calibration_data()
{
    uint8_t reg_addr = 0x88;
    uint8_t raw_data[24];

    // Write register address 0x88, then read 24 bytes
    write_register_bmp280(reg_addr, 0);
    read_register_bmp280(reg_addr, raw_data, 24);

    // Parse data (LSB first)
    cal_data.dig_T1 = (raw_data[1] << 8) | raw_data[0];
    cal_data.dig_T2 = (int16_t)((raw_data[3] << 8) | raw_data[2]);
    cal_data.dig_T3 = (int16_t)((raw_data[5] << 8) | raw_data[4]);

    cal_data.dig_P1 = (raw_data[7] << 8) | raw_data[6];
    cal_data.dig_P2 = (int16_t)((raw_data[9] << 8) | raw_data[8]);
    cal_data.dig_P3 = (int16_t)((raw_data[11] << 8) | raw_data[10]);
    cal_data.dig_P4 = (int16_t)((raw_data[13] << 8) | raw_data[12]);
    cal_data.dig_P5 = (int16_t)((raw_data[15] << 8) | raw_data[14]);
    cal_data.dig_P6 = (int16_t)((raw_data[17] << 8) | raw_data[16]);
    cal_data.dig_P7 = (int16_t)((raw_data[19] << 8) | raw_data[18]);
    cal_data.dig_P8 = (int16_t)((raw_data[21] << 8) | raw_data[20]);
    cal_data.dig_P9 = (int16_t)((raw_data[23] << 8) | raw_data[22]);
}

static esp_err_t configure_ctrl_meas()
{
    uint8_t ctrl_meas = (osrs_t << 5) | (osrs_p << 2) | mode;
    return write_register_bmp280(REG_CTRL_MEAS, ctrl_meas);
}

static esp_err_t configure_config()
{
    uint8_t config = (standby << 5) | (filter_value << 2) | spi;
    return write_register_bmp280(REG_CONFIG, config);
}

esp_err_t bmp280_configure()
{
    read_calibration_data();
    esp_err_t err = configure_ctrl_meas();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write ctrl_meas: %s", esp_err_to_name(err));
        return err;
    }
    err = configure_config();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write config: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t bmp280_trigger_normal_mode()
{
    mode = 3; // Set mode to normal
    esp_err_t err = configure_ctrl_meas();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to trigger normal mode: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t bmp280_trigger_forced_mode()
{
    mode = 1; // Set mode to forced
    esp_err_t err = configure_ctrl_meas();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to trigger forced mode: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t bmp280_trigger_sleep_mode()
{
    mode = 0; // Set mode to sleep
    esp_err_t err = configure_ctrl_meas();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to trigger sleep mode: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

float bmp280_read_temp()
{
    uint8_t data[3];
    esp_err_t err = read_register_bmp280(BMP280_TEMP_MSB, data, 3);

    while (1)
    {
        uint8_t status;
        read_register_bmp280(BMP280_REG_STATUS, &status, 1);
        if ((status & 0x08) == 0)
        {          // Bit 3 is 'measuring'
            break; // Measurement finished
        }
        vTaskDelay(2 / portTICK_PERIOD_MS); // Wait a bit
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading temperature data: %s", esp_err_to_name(err));
        return -100.0f;
    }
    int32_t raw_temperature = (int32_t)((data[0] << 12) | (data[1] << 4) | (data[2] >> 4));
    float temperature = convert_temperature(raw_temperature);
    // printf("Temperature: %.2f °C\n", temperature);
    // return ESP_OK;
    return temperature;
}

float bmp280_read_pres()
{
    uint8_t data[6];
    esp_err_t err = read_register_bmp280(BMP280_PRES_MSB, data, 6);

    while (1)
    {
        uint8_t status;
        read_register_bmp280(BMP280_REG_STATUS, &status, 1);
        if ((status & 0x08) == 0)
        {          // Bit 3 is 'measuring'
            break; // Measurement finished
        }
        vTaskDelay(2 / portTICK_PERIOD_MS); // Wait a bit
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading pressure data: %s", esp_err_to_name(err));
        return -1.0f;
    }
    int32_t raw_pressure = (int32_t)((data[0] << 12) | (data[1] << 4) | (data[2] >> 4));
    int32_t raw_temp = (int32_t)((data[3] << 12) | (data[4] << 4) | (data[5] >> 4));
    float temperature = convert_temperature(raw_temp);
    float pressure = convert_pressure(raw_pressure);
    // printf("Pressure: %.2f hPa\n", pressure);
    // return ESP_OK;
    return pressure;
}

esp_err_t bmp280_trigger_filter(uint8_t filter)
{
    filter_value = filter;
    esp_err_t err = configure_config();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to trigger filter: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t bmp280_change_temp_resolution(uint8_t resolution)
{
    osrs_t = resolution;
    esp_err_t err = configure_ctrl_meas();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to change temperature resolution: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t bmp280_change_pres_resolution(uint8_t resolution)
{
    osrs_p = resolution;
    esp_err_t err = configure_ctrl_meas();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to change pressure resolution: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t bmp280_change_standby_time(uint8_t time)
{
    standby = time;
    esp_err_t err = configure_config();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to change standby time: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

static float convert_temperature(int32_t raw_temp)
{
    int32_t var1, var2;
    var1 = ((((raw_temp >> 3) - ((int32_t)cal_data.dig_T1 << 1))) * ((int32_t)cal_data.dig_T2)) >> 11;
    var2 = (((((raw_temp >> 4) - ((int32_t)cal_data.dig_T1)) * ((raw_temp >> 4) - ((int32_t)cal_data.dig_T1))) >> 12) * ((int32_t)cal_data.dig_T3)) >> 14;
    t_fine = var1 + var2;
    int32_t T = (t_fine * 5 + 128) >> 8;
    return (float)T / 100.0;
}

static float convert_pressure(int32_t raw_pres)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)cal_data.dig_P6;
    var2 = var2 + ((var1 * (int64_t)cal_data.dig_P5) << 17);
    var2 = var2 + ((int64_t)cal_data.dig_P4 << 35);
    var1 = ((var1 * var1 * (int64_t)cal_data.dig_P3) >> 8) + ((var1 * (int64_t)cal_data.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1) * (int64_t)cal_data.dig_P1) >> 33;
    if (var1 == 0)
    {
        return 0; // avoid exception caused by division by zero
    }
    p = 1048576 - raw_pres;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)cal_data.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)cal_data.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)cal_data.dig_P7) << 4);
    return (float)p / 256.0 / 100.0;
}