#include "hcsr04.h"

void hcsr04_task(void *arg)
{
    esp_err_t return_value1 = ESP_OK;
    esp_err_t return_value2 = ESP_OK;
    esp_err_t return_value3 = ESP_OK;
    UltrasonicInit();

    static uint32_t distance1 = 0;
    static uint32_t distance2 = 0;
    static uint32_t distance3 = 0;
    float *shared_value = (float *)arg;

    int success_count = 0;
    bool medium_mode = false;
    bool fast_mode = false;
    int64_t fast_mode_start_time = 0;

    while (1)
    {
        return_value1 = UltrasonicMeasure(200, &distance1);
        vTaskDelay(pdMS_TO_TICKS(40));
        return_value2 = UltrasonicMeasure(200, &distance2);
        vTaskDelay(pdMS_TO_TICKS(40));
        return_value3 = UltrasonicMeasure(200, &distance3);
        vTaskDelay(pdMS_TO_TICKS(40));

        if (return_value1 == ESP_OK && return_value2 == ESP_OK && return_value3 == ESP_OK)
        {
            if (!medium_mode)
            {
                medium_mode = true;
            }
            *shared_value = (float)(distance1 + distance2 + distance3) / 3.0f;
            success_count++;
            buzzer_set_distance(*shared_value);

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
