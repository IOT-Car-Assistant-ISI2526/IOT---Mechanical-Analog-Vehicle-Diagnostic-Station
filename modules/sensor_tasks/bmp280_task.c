#include "bmp280_task.h"

int bmp_counter = 0;

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

            // char measurement_str[512];

            // snprintf(measurement_str, sizeof(measurement_str), "Measurement %d: %.2f\n", bmp_counter++, temp);

            // esp_err_t append_result = sd_card_append_file("bmp.txt", measurement_str);
            // if (append_result == ESP_OK)
            // {
            //     // Read and print all contents of the file
            //     char *file_contents = sd_card_read_file("bmp.txt");
            //     if (file_contents != NULL)
            //     {
            //         printf("BMP280 file contents:\n%s\n", file_contents);
            //         free(file_contents);
            //     }
            // }

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