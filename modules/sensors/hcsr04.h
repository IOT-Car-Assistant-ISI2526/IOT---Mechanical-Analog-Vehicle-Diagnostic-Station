#ifndef HCSR04
#define HCSR04

#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include "project_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "hcsr04_driver.h"
#include "esp_timer.h"
#include "buzzer.h"
#include "driver/gpio.h"
#include "esp_timer.h"

void hcsr04_task(void *arg);
void hcsr04_start_task(float *parameter);

void buzzer_init(gpio_num_t pin);
void enable_park(bool enable);
void set_distance(uint32_t distance_cm);

void buzzer_on(void);
void buzzer_off(void);
void buzzer_beep(uint32_t duration_ms);

#endif // HCSR04