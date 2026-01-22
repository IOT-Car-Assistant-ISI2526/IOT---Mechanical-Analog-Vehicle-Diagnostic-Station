#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdio.h>
#include "storage_manager.h"

uint32_t get_timestamp(void);

void print_sensor(const char *name, float value, const char *unit);

void save_sensor_to_storage(const char *name, float value);

void print_all_sensors(float bmp, float lux, float eng, float dist, float accel);

void save_all_sensors(float bmp, float lux, float eng, float dist, float accel);

#endif // UTILS_H
