#include "bmp280.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "project_config.h"

void bmp280_task(void *arg);

void bmp280_start_task(float *temperature);