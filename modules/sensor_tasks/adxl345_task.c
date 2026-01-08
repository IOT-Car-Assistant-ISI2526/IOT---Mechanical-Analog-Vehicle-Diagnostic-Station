#include "adxl345_task.h"

void adxl345_task(void *arg)
{
    while (1)
    {
        float acceleration = adxl345_read_data();
        if (acceleration != -1.0f)
        {
            *(float *)arg = acceleration;
            vTaskDelay(FREQUENT_MEASUREMENT_INTERVAL_MS);
        }
        else
        {
            printf("Failed to read ADXL345 data");
            vTaskDelay(SENSOR_MEASUREMENT_FAIL_INTERVAL_MS);
        }
    }
}

void adxl345_start_task(float *parameter)
{
    xTaskCreate(adxl345_task, "adxl345_task", 4096, parameter, 5, NULL);
}