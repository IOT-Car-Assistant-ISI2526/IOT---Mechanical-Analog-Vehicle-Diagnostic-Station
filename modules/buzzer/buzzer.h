#ifndef BUZZER_H
#define BUZZER_H

#include "driver/gpio.h"
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <stdatomic.h>

void buzzer_init(gpio_num_t pin);
void buzzer_beep(uint32_t duration_ms);
void buzzer_enable_park(bool enable);
void buzzer_set_distance(uint32_t distance_cm);

void buzzer_on(void);
void buzzer_off(void);

#endif
