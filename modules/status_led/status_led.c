#include "status_led.h"
#include "project_config.h" 
#include "wifi_station.h"   

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG_LED = "status_led";

static void led_status_task(void* arg)
{
    gpio_reset_pin(CONFIG_BLINK_GPIO); // -> resetuje pin do stanu domyslego
    gpio_set_direction(CONFIG_BLINK_GPIO, GPIO_MODE_OUTPUT); // -> ustawia pin jako wyjscie
    ESP_LOGI(TAG_LED, "Task LED uruchomiony na GPIO %d", CONFIG_BLINK_GPIO);

    while (1) {
        if (wifi_station_is_connected()) { 
            gpio_set_level(CONFIG_BLINK_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(100)); // -> zatrzymuje wykonywanie watku na 100ms
        } else {
            gpio_set_level(CONFIG_BLINK_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_set_level(CONFIG_BLINK_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

// towrzenie nowego watku 
void status_led_start_task(void)
{
    xTaskCreate(led_status_task, "led_status_task", 2048, NULL, 5, NULL);
}