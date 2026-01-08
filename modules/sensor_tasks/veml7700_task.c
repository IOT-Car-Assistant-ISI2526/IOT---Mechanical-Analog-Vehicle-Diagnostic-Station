#include "veml7700_task.h"

void veml7700_task(void *arg)
{
    while (1)
    {
        float lux = veml7700_read_lux();
        if (lux >= 0.0f)
        {
            *(float *)arg = lux;
            vTaskDelay(VEML7700_MEASUREMENT_INTERVAL_MS);
        }
        else
        {
            printf("Failed to read sensor");
            vTaskDelay(SENSOR_MEASUREMENT_FAIL_INTERVAL_MS);
        }
    }
}

void veml7700_start_task(float *parameter)
{
    xTaskCreate(veml7700_task, "VEML7700_Task", 4096, parameter, 5, NULL);
}