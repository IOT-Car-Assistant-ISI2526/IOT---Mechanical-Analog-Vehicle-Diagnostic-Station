#include "veml7700.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "project_config.h"

void veml7700_task(void *arg);

void veml7700_start_task(float *parameter);