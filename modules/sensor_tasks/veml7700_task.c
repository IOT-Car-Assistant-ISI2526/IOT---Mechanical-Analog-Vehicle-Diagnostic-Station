#include "veml7700_task.h"
#include "ble_server.h"
#include "esp_log.h"
#include "utils.h"

#define VEML7700_LUX_THRESHOLD 10.0f

void veml7700_task(void *arg)
{
    while (1)
    {
        float lux = veml7700_read_lux();
        if (lux >= 0.0f)
        {
            *(float *)arg = lux;

            if (lux < VEML7700_LUX_THRESHOLD)
            {
                char alert_msg[32];
                snprintf(alert_msg, sizeof(alert_msg), "%.1f", lux);
                ble_send_alert("VEML7700", alert_msg);
            }

            save_sensor_to_storage("VEML7700", lux);

            vTaskDelay(VEML7700_MEASUREMENT_INTERVAL_MS);
            printf("VEML7700: Lux = %.2f\n", lux);
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
    xTaskCreate(veml7700_task, "VEML7700_Task", 2048, parameter, 5, NULL);
}