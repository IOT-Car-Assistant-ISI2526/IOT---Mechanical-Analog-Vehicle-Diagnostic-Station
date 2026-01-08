#include "adxl345.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "project_config.h"

void adxl345_task(void *arg);

void adxl345_start_task(float *parameter);