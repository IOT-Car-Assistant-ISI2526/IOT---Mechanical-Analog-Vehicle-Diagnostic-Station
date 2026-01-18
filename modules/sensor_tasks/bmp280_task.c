#include "bmp280_task.h"

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
            vTaskDelay(BMP280_MEASUREMENT_INTERVAL_MS); // 15 minutes
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