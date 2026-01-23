#include "max6675_task.h"
#include "utils.h"
#include "ble_server.h"
#include "esp_timer.h"


#define MAX6675_PROFILE_TEMP_TRIGGER  50.0f
#define MAX6675_PROFILE_DURATION_MS   (4 * 60 * 1000) // 4 minutes


void max6675_task(void *arg)
{
    while (1)
    {
        float engine_temp = max6675_read_celsius();
        if (engine_temp != -1.0f)
        {
            *(float *)arg = engine_temp;
            vTaskDelay(MAX6675_MEASUREMENT_INTERVAL_MS); // 5 minutes

            char alert_msg[32];
            snprintf(alert_msg, sizeof(alert_msg), "%.1f", engine_temp);
            ble_send_alert("MAX6675", alert_msg);
            save_sensor_to_storage("MAX6675_NORMAL", engine_temp);
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

void max6675_profile_task(void *arg)
{
    float *shared_temp = (float *)arg;

 
    bool threshold_reached = false;
    int64_t threshold_time_us = 0;

    while (1)
    {
        if (!ble_max6675_profile_requested())
        {
  
            threshold_reached = false;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

  

        float temp = max6675_read_celsius();
        if (temp < 0)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        *shared_temp = temp;

        save_sensor_to_storage("MAX6675_PROFILE", temp);
        ble_notify_max6675_profile(temp);


        if (!threshold_reached && temp >= MAX6675_PROFILE_TEMP_TRIGGER)
        {
            threshold_reached = true;
            threshold_time_us = esp_timer_get_time();

            ESP_LOGI("MAX6675_PROFILE",
                     "Threshold reached (%.1f°C), 4-minute timer started",
                     temp);
        }

        if (threshold_reached)
        {
            int64_t elapsed_ms =
                (esp_timer_get_time() - threshold_time_us) / 1000;

            if (elapsed_ms >= MAX6675_PROFILE_DURATION_MS)
            {
                ESP_LOGI("MAX6675_PROFILE", "Profiling finished (4 minutes)");

                ble_max6675_clear_profile_request(); // require new BLE '1'
                threshold_reached = false;

                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
        }

        vTaskDelay(MAX6675_PROFILE_INTERVAL_MS);
    }
}


void max6675_start_profile_task(float *engine_temp)
{
    xTaskCreate(
        max6675_profile_task,
        "max6675_profile_task",
        2048,
        engine_temp,
        5,
        NULL);
}
