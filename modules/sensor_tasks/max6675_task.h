#include "max6675.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "project_config.h"

void max6675_task(void *arg);

void max6675_start_task(float *engine_temp);
void max6675_profile_task(void *arg);
void max6675_start_profile_task(float *engine_temp);