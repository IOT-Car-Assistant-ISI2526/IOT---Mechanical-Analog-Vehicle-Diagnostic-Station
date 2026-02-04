#include "bmp280_task.h"
#include "ble_server.h"
#include "esp_log.h"
#include "utils.h"

#define BMP280_TEMP_MIN 26.0f
#define BMP280_TEMP_MAX 28.0f

void bmp280_task(void *arg)
{
    float temp;
    while (1)
    {
        bmp280_trigger_forced_mode();
        vTaskDelay(100 / portTICK_PERIOD_MS);
        temp = bmp280_read_temp();
        if (temp != -100.0f)
        {
            *(float *)arg = temp;

            if (temp < BMP280_TEMP_MIN || temp > BMP280_TEMP_MAX)
            {
                char alert_msg[32];
                snprintf(alert_msg, sizeof(alert_msg), "%.1f", temp);
                ble_send_alert("BMP280", alert_msg);
            }

            save_sensor_to_storage("BMP280", temp);

            vTaskDelay(BMP280_MEASUREMENT_INTERVAL_MS);
        }
        else
        {
            printf("Failed to read temperature");
            vTaskDelay(SENSOR_MEASUREMENT_FAIL_INTERVAL_MS);
        }
    }
}

void bmp280_start_task(float *temperature)
{
    xTaskCreate(bmp280_task, "bmp280_task", 4096, temperature, 5, NULL);
}