#include "adxl345_task.h"

int adxl_counter = 0;

void adxl345_task(void *arg)
{
    while (1)
    {
        float acceleration = adxl345_read_data();
        if (acceleration != -1.0f)
        {
            *(float *)arg = acceleration;

            // char measurement_str[512];
            // snprintf(measurement_str, sizeof(measurement_str), "Measurement %d: %.2f\n", adxl_counter++, acceleration);

            // esp_err_t append_result = sd_card_append_file("adxl.txt", measurement_str);
            // if (append_result == ESP_OK)
            // {
            //     // Read and print all contents of the file
            //     char *file_contents = sd_card_read_file("adxl.txt");
            //     if (file_contents != NULL)
            //     {
            //         printf("ADXL345 file contents:\n%s\n", file_contents);
            //         free(file_contents);
            //     }
            // }

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