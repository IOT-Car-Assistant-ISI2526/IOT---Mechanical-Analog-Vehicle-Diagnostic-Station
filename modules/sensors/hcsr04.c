#include "hcsr04.h"

static gpio_num_t s_buzzer_pin = GPIO_NUM_NC;
static _Atomic bool s_park_enabled = false;
static _Atomic uint32_t s_park_distance_cm = 150;

static bool s_buzzer_is_on = false;
static int64_t s_next_buzzer_event_time = 0; // Time in microseconds

static inline void buzzer_hw_on(void);
static inline void buzzer_hw_off(void);
static uint32_t buzzer_calc_delay_ms(uint32_t distance_cm);
static void buzzer_update_tick(void);
static void vTaskDelay_with_buzzer(uint32_t ms);

static inline void buzzer_hw_on(void)
{
    if (s_buzzer_pin != GPIO_NUM_NC)
        gpio_set_level(s_buzzer_pin, 1);
}

static inline void buzzer_hw_off(void)
{
    if (s_buzzer_pin != GPIO_NUM_NC)
        gpio_set_level(s_buzzer_pin, 0);
}

static uint32_t buzzer_calc_delay_ms(uint32_t distance_cm)
{
    if (distance_cm >= 200)
        return 0;
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

static void buzzer_update_tick(void)
{
    // 1. Check if Parking Mode is enabled
    if (!atomic_load(&s_park_enabled))
    {
        if (s_buzzer_is_on)
        {
            buzzer_hw_off();
            s_buzzer_is_on = false;
        }
        return;
    }

    // 2. Get current time in microseconds
    int64_t now = esp_timer_get_time();

    // 3. Check if we reached the next event time
    if (now >= s_next_buzzer_event_time)
    {

        uint32_t dist = atomic_load(&s_park_distance_cm);
        uint32_t interval_ms = buzzer_calc_delay_ms(dist);

        if (interval_ms == 0)
        {
            // Distance too far, ensure off
            buzzer_hw_off();
            s_buzzer_is_on = false;
            // Check again in 50ms
            s_next_buzzer_event_time = now + (50 * 1000);
            return;
        }

        if (s_buzzer_is_on)
        {
            // -- WAS ON, TURN OFF --
            buzzer_hw_off();
            s_buzzer_is_on = false;
            // Schedule next BEEP start based on distance interval
            s_next_buzzer_event_time = now + ((int64_t)interval_ms * 1000);
        }
        else
        {
            // -- WAS OFF, TURN ON --
            buzzer_hw_on();
            s_buzzer_is_on = true;
            // Beep duration is fixed at 20ms
            s_next_buzzer_event_time = now + (20 * 1000);
        }
    }
}

static void vTaskDelay_with_buzzer(uint32_t ms)
{
    const uint32_t step_ms = 10;
    uint32_t elapsed = 0;

    while (elapsed < ms)
    {
        buzzer_update_tick(); // Check buzzer state
        vTaskDelay(pdMS_TO_TICKS(step_ms));
        elapsed += step_ms;
    }
}

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
        // CRITICAL: Update buzzer immediately after measure (which blocks slightly)
        buzzer_update_tick();
        // Wait 40ms, but keep updating buzzer
        vTaskDelay_with_buzzer(30);

        return_value2 = UltrasonicMeasure(200, &distance2);
        buzzer_update_tick();
        vTaskDelay_with_buzzer(30);
        return_value3 = UltrasonicMeasure(200, &distance3);
        buzzer_update_tick();
        vTaskDelay_with_buzzer(30);

        if (return_value1 == ESP_OK && return_value2 == ESP_OK && return_value3 == ESP_OK)
        {
            if (!medium_mode)
            {
                medium_mode = true;
            }

            *shared_value = (float)(distance1 + distance2 + distance3) / 3.0f;
            success_count++;

            buzzer_set_distance((uint32_t)*shared_value);

            if (!fast_mode && success_count >= HCSR04_TRIGGER_COUNT)
            {
                medium_mode = false;
                fast_mode = true;
                buzzer_enable_park(true);
                fast_mode_start_time = esp_timer_get_time();
            }
            else if (fast_mode)
            {
                fast_mode_start_time = esp_timer_get_time();
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

        // Handle the variable delays at end of loop using the buzzer-aware delay
        if (fast_mode)
            vTaskDelay_with_buzzer(HCSR04_FASTMODE_INTERVAL_MS);
        else if (medium_mode)
            vTaskDelay_with_buzzer(HCSR04_FASTMODE_INTERVAL_MS * 2);
        else
            vTaskDelay_with_buzzer(HCSR04_SLOWMODE_INTERVAL_MS);
    }
}

void hcsr04_start_task(float *parameter)
{
    xTaskCreate(hcsr04_task, "HSRC04_Buzzer", 8192, parameter, 10, NULL);
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
}

void buzzer_enable_park(bool enable)
{
    atomic_store(&s_park_enabled, enable);
    if (!enable)
    {
        buzzer_hw_off();
        s_buzzer_is_on = false;
    }
}

void buzzer_set_distance(uint32_t distance_cm)
{
    atomic_store(&s_park_distance_cm, distance_cm);
}

void buzzer_on(void) { buzzer_hw_on(); }
void buzzer_off(void) { buzzer_hw_off(); }

void buzzer_beep(uint32_t duration_ms)
{
    buzzer_hw_on();
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    buzzer_hw_off();
}