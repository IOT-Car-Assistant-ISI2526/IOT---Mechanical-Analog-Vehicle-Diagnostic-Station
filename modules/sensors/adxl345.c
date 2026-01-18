#include "adxl345.h"

static const char *TAG = "ADXL345";

uint8_t link = 1;
uint8_t auto_sleep = 1;
uint8_t measure_bit = 1;
uint8_t sleep_bit = 0;
uint8_t sleep_mode_frequency = 0; // 8 Hz
uint8_t low_power = 0;
uint8_t low_power_frequency = 0x0A; // 100 Hz
uint64_t activity_threshold = 0;
uint64_t inactivity_threshold = 0;
uint64_t inactivity_time = 60;

static i2c_master_dev_handle_t adxl345_handle;

static esp_err_t read_register_adxl345(uint8_t reg_addr, uint8_t *data, size_t len);
static esp_err_t write_register_adxl345(uint8_t reg_addr, uint8_t value);
static esp_err_t configure_power_ctrl();
static esp_err_t cofigure_bw_rate();
static void convert_raw_data_to_ms2(int16_t rx, int16_t ry, int16_t rz, float *x, float *y, float *z);
static float calculate_acceleration(float x, float y, float z);

esp_err_t adxl345_init(i2c_master_bus_handle_t bus_handle)
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ADXL345_ADDR,
        .scl_speed_hz = ADXL345_SPEED_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &adxl345_handle));
    ESP_LOGI(TAG, "ADXL345 initialized on I2C address 0x%02X", ADXL345_ADDR);
    return ESP_OK;
}

esp_err_t adxl345_delete()
{
    return i2c_master_bus_rm_device(adxl345_handle);
}

static esp_err_t read_register_adxl345(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(adxl345_handle, &reg_addr, 1, data, len, 1000);
}

static esp_err_t write_register_adxl345(uint8_t reg_addr, uint8_t value)
{
    uint8_t buf[2] = {reg_addr, value};
    return i2c_master_transmit(adxl345_handle, buf, sizeof(buf), 1000);
}

static esp_err_t configure_power_ctrl()
{
    uint8_t power_ctl = (link << 5) | (auto_sleep << 4) | (measure_bit << 3) | (sleep_bit << 2) | sleep_mode_frequency;
    return write_register_adxl345(REG_POWER_CTL, power_ctl);
}

static esp_err_t cofigure_bw_rate()
{
    uint8_t bw_rate = (low_power << 4) | low_power_frequency;
    return write_register_adxl345(BW_RATE, bw_rate);
}

esp_err_t adxl345_configure()
{
    esp_err_t err = configure_power_ctrl();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure power control: %s", esp_err_to_name(err));
        return err;
    }
    err = cofigure_bw_rate();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure bandwidth rate: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t adxl345_set_measurement_mode()
{
    measure_bit = 1;
    esp_err_t err = configure_power_ctrl();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set measurement mode: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t adxl345_set_standby_mode()
{
    measure_bit = 0;
    esp_err_t err = configure_power_ctrl();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set standby mode: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t adxl345_set_sleep_mode(bool enable)
{
    sleep_bit = enable ? 1 : 0;
    esp_err_t err = configure_power_ctrl();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set sleep mode: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t adxl345_set_low_power_mode(bool enable)
{
    low_power = enable ? 1 : 0;
    esp_err_t err = cofigure_bw_rate();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set low power mode: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t adxl345_set_low_power_rate(uint8_t rate)
{
    low_power_frequency = rate;
    esp_err_t err = cofigure_bw_rate();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set data rate: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t adxl345_set_activity_threshold(uint8_t threshold)
{
    activity_threshold = threshold;
    return write_register_adxl345(THRESH_ACT, activity_threshold);
}

esp_err_t adxl345_set_inactivity_threshold(uint8_t threshold)
{
    inactivity_threshold = threshold;
    return write_register_adxl345(THRESH_INACT, inactivity_threshold);
}

esp_err_t adxl345_set_inactivity_time(uint8_t time)
{
    inactivity_time = time;
    return write_register_adxl345(TIME_INACT, inactivity_time);
}

esp_err_t adxl345_set_frequency_in_sleep_mode(uint8_t frequency)
{
    sleep_mode_frequency = frequency;
    esp_err_t err = configure_power_ctrl();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set frequency in sleep mode: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t adxl345_enable_auto_sleep(bool enable)
{
    auto_sleep = enable ? 1 : 0;
    esp_err_t err = configure_power_ctrl();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set auto sleep: %s", esp_err_to_name(err));
        return err;
    }
    err = adxl345_enable_all_axis_activity_detection();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable all axis activity detection: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t adxl345_enable_all_axis_activity_detection()
{
    uint8_t act_inact_ctl;
    esp_err_t err = read_register_adxl345(ACT_INACT_CTL, &act_inact_ctl, 1);
    if (err != ESP_OK)
    {
        return err;
    }
    act_inact_ctl |= 0x07; // Set bits for X, Y, Z axes
    return write_register_adxl345(ACT_INACT_CTL, act_inact_ctl);
}

static void convert_raw_data_to_ms2(int16_t rx, int16_t ry, int16_t rz, float *x, float *y, float *z)
{
    *x = rx * ADXL345_LSB_TO_MS2;
    *y = ry * ADXL345_LSB_TO_MS2;
    *z = rz * ADXL345_LSB_TO_MS2;
}

static float calculate_acceleration(float x, float y, float z)
{
    x = x - ADXL345_X_AXIS_CORRECTION;
    y = y - ADXL345_Y_AXIS_CORRECTION;
    z = z - ADXL345_EARTH_GRAVITY_MS2;

    float combined_acceleration = sqrtf(x * x + y * y + z * z);
    return combined_acceleration;
}

float adxl345_read_data()
{
    uint8_t raw[6];
    esp_err_t err = read_register_adxl345(REG_DATAX0, raw, 6);

    if (err != ESP_OK)
        return -1.0f;

    int16_t rx = (int16_t)(raw[1] << 8 | raw[0]);
    int16_t ry = (int16_t)(raw[3] << 8 | raw[2]);
    int16_t rz = (int16_t)(raw[5] << 8 | raw[4]);

    float x, y, z;
    convert_raw_data_to_ms2(rx, ry, rz, &x, &y, &z);

    return calculate_acceleration(x, y, z);
}