#include "buzzer.h"

static gpio_num_t s_buzzer_pin = GPIO_NUM_NC;
static TaskHandle_t s_buzzer_task = NULL;

static _Atomic bool s_park_enabled = false;
static _Atomic uint32_t s_park_distance_cm = 150;

static uint32_t buzzer_calc_delay_ms(uint32_t distance_cm);

static inline void buzzer_hw_on(void) { gpio_set_level(s_buzzer_pin, 1); }
static inline void buzzer_hw_off(void) { gpio_set_level(s_buzzer_pin, 0); }

static uint32_t buzzer_calc_delay_ms(uint32_t distance_cm)
{
    if (distance_cm == 200)
        return 0; // no beeping
    if (distance_cm > 120)
        return 800;
    if (distance_cm > 90)
        return 500;
    if (distance_cm > 70)
        return 400;
    if (distance_cm > 50)
        return 300;
    if (distance_cm > 30)
        return 200;
    if (distance_cm > 20)
        return 120;
    if (distance_cm > 12)
        return 60;
    return 25;
}

static void buzzer_task(void *arg)
{
    const TickType_t idle_delay = pdMS_TO_TICKS(50);

    while (1)
    {
        if (!atomic_load(&s_park_enabled))
        {
            buzzer_hw_off();
            vTaskDelay(idle_delay);
            continue;
        }

        uint32_t dist = atomic_load(&s_park_distance_cm);
        uint32_t delay_ms = buzzer_calc_delay_ms(dist);

        if (delay_ms == 0)
        {
            buzzer_hw_off();
            vTaskDelay(idle_delay);
            continue;
        }

        buzzer_hw_on();
        vTaskDelay(pdMS_TO_TICKS(20));
        buzzer_hw_off();

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    vTaskDelete(NULL);
}

void buzzer_init(gpio_num_t pin)
{
    s_buzzer_pin = pin;

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << s_buzzer_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    buzzer_hw_off();

    // if (s_buzzer_task == NULL) {
    //     xTaskCreate(buzzer_task, "buzzer_task", 2048, NULL, 2, &s_buzzer_task);
    // }
}

void buzzer_enable_park(bool enable)
{
    atomic_store(&s_park_enabled, enable);
    if (!enable)
    {
        buzzer_hw_off();
    }
}

void buzzer_set_distance(uint32_t distance_cm)
{
    atomic_store(&s_park_distance_cm, distance_cm);
}

void buzzer_on(void)
{
    buzzer_hw_on();
}

void buzzer_off(void)
{
    buzzer_hw_off();
}

void buzzer_beep(uint32_t duration_ms)
{
    buzzer_hw_on();
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    buzzer_hw_off();
}
