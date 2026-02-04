#pragma once

#pragma once
#include <stdbool.h>
#include <stdint.h>

void ble_server_init(void);
void ble_server_stop(void);
void ble_server_restart(void);


bool ble_hcsr04_streaming_enabled(void);
void ble_hcsr04_set_streaming(bool enable);

int ble_hcsr04_notify_distance_cm(uint16_t distance_cm);

int ble_send_alert(const char* sensor_name, const char* message);

bool ble_max6675_profile_requested(void);
void ble_max6675_clear_profile_request(void);
void ble_notify_max6675_profile(float temperature);
