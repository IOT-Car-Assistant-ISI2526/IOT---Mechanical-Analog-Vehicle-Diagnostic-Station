#include "max6675_task.h"

void max6675_task(void *arg)
{
    while (1)
    {
        float engine_temp = max6675_read_celsius();
        if (engine_temp != -1.0f)
        {
            *(float *)arg = engine_temp;
            vTaskDelay(MAX6675_MEASUREMENT_INTERVAL_MS); // 5 minutes
        }
        else
        {
            printf("Error reading MAX6675 temperature.\n");
            vTaskDelay(SENSOR_MEASUREMENT_FAIL_INTERVAL_MS);
        }
    }
}

void max6675_start_task(float *engine_temp)
{
    xTaskCreate(max6675_task, "max6675_task", 2048, engine_temp, 5, NULL);
}