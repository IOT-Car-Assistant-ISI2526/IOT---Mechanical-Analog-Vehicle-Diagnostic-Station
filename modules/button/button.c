#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "ble_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define BUTTON_GPIO GPIO_NUM_0
#define LONG_PRESS_MS 5000  // 5 sekund

static bool button_down = false;
static int64_t press_start = 0;

static void IRAM_ATTR button_isr(void* arg)
{
    int level = gpio_get_level(BUTTON_GPIO);

    if (level == 0) {
        // wciśnięty
        button_down = true;
        press_start = esp_timer_get_time() / 1000;
    } else {
        // puszczony
        button_down = false;
    }
}


static void button_task(void *arg)
{
    while (1)
    {
        if (button_down)
        {
            int64_t now = esp_timer_get_time() / 1000;
            if (now - press_start >= LONG_PRESS_MS)
            {
                ESP_LOGI("BUTTON", "WYKRYTO długie przytrzymanie → start BLE provisioning");
                ble_server_init();
                vTaskDelay(pdMS_TO_TICKS(1000)); // zapobiegamy wielokrotnemu wywołaniu
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void button_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&io);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr, NULL);

    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
}
