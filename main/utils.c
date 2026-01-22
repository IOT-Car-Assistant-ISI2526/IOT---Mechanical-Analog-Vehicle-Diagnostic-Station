#include "utils.h"
#include <time.h>
#include "esp_log.h" 

uint32_t get_timestamp(void)
{
    return (uint32_t)time(NULL);
}

void print_sensor(const char *name, float value, const char *unit)
{
    printf("%s: %.3f %s\n", name, value, unit);
}

void save_sensor_to_storage(const char *name, float value)
{
    char line[128];
    snprintf(line, sizeof(line), "%s;%lu;%.3f", name, get_timestamp(), value);
    if (storage_write_line(line))
    {
        ESP_LOGI("APP_MAIN", "Saved: %s", line);
    }
    else
    {
        ESP_LOGW("APP_MAIN", "Failed to save: %s", line);
    }
}


void print_all_sensors(float bmp, float lux, float eng, float dist, float accel)
{
    print_sensor("BMP280", bmp, "C");
    print_sensor("VEML7700", lux, "Lux");
    print_sensor("MAX6675", eng, "C");
    print_sensor("HC-SR04", dist, "cm");
    print_sensor("ADXL345", accel, "m/s2");
}

void save_all_sensors(float bmp, float lux, float eng, float dist, float accel)
{
    save_sensor_to_storage("BMP280", bmp);
    save_sensor_to_storage("VEML7700", lux);
    save_sensor_to_storage("MAX6675", eng);
    save_sensor_to_storage("HC-SR04", dist);
    save_sensor_to_storage("ADXL345", accel);
}
