#include "hcsr04.h"

void hcsr04_task(void *arg)
{
    esp_err_t return_value = ESP_OK;
    UltrasonicInit();

    static uint32_t distance = 0;
    float *shared_value = (float *)arg;

    int success_count = 0;
    bool medium_mode = false;
    bool fast_mode = false;
    int64_t fast_mode_start_time = 0;

    while (1)
    {
        return_value = UltrasonicMeasure(200, &distance);

        if (return_value == ESP_OK)
        {
            if (!medium_mode)
            {
                medium_mode = true;
            }
            *shared_value = (float)distance;
            success_count++;
            buzzer_set_distance(distance);

            if (!fast_mode && success_count >= HCSR04_TRIGGER_COUNT)
            {
                medium_mode = false;
                fast_mode = true;
                fast_mode_start_time = esp_timer_get_time();

                buzzer_enable_park(true);
            }
        }
        else
        {
            success_count = 0;
            medium_mode = false;
        }

        if (fast_mode)
        {
            int64_t elapsed = esp_timer_get_time() - fast_mode_start_time;

            if (elapsed >= (int64_t)HCSR04_FASTMODE_TIMEOUT_MS * 1000)
            {
                fast_mode = false;
                success_count = 0;

                buzzer_enable_park(false);
            }
        }

        if (fast_mode)
            vTaskDelay(pdMS_TO_TICKS(HCSR04_FASTMODE_INTERVAL_MS));
        else if (medium_mode)
            vTaskDelay(pdMS_TO_TICKS(HCSR04_FASTMODE_INTERVAL_MS * 2));
        else
            vTaskDelay(pdMS_TO_TICKS(HCSR04_SLOWMODE_INTERVAL_MS));
    }
}

void hcsr04_start_task(float *parameter)
{
    xTaskCreate(hcsr04_task, "HSRC04 task", 8192, parameter, 10, NULL);
}
