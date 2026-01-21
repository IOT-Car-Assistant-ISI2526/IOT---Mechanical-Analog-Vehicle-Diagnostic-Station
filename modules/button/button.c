#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "ble_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// External button on GPIO4 (recommended wiring: button shorts GPIO4 to GND, use internal pull-up)
#define BUTTON_GPIO GPIO_NUM_4
#define LONG_PRESS_MS 3000  // 3 sekundy (hold)

static volatile bool button_down = false;
static volatile int64_t press_start_ms = 0;
static volatile bool long_press_released = false;

// Guard against re-initializing BLE multiple times (ble_server_init() is not meant to be called repeatedly).
static bool ble_started = false;

static void IRAM_ATTR button_isr(void* arg)
{
    int level = gpio_get_level(BUTTON_GPIO);

    if (level == 0) {
        // wciśnięty
        button_down = true;
        press_start_ms = esp_timer_get_time() / 1000;
    } else {
        // puszczony
        button_down = false;
        int64_t now_ms = esp_timer_get_time() / 1000;
        if ((now_ms - press_start_ms) >= LONG_PRESS_MS) {
            // Evaluate long-press on release (unclicking)
            long_press_released = true;
        }
    }
}


static void button_task(void *arg)
{
    while (1)
    {
        if (long_press_released) {
            long_press_released = false;

            if (!ble_started) {
                ble_started = true;
                ESP_LOGI("BUTTON", "WYKRYTO długie przytrzymanie (>=3s) i puszczenie → start BLE provisioning");
                ble_server_init();
            } else {
                ESP_LOGI("BUTTON", "BLE provisioning już uruchomione — restartowanie BLE");
                ble_server_restart();
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
