#pragma once

#pragma once
#include <stdbool.h>
#include <stdint.h>

// Funkcja uruchamiająca serwer BLE (rozgłaszanie i obsługę usług)
void ble_server_init(void);
void ble_server_stop(void);
void ble_server_restart(void);

// --- HCSR04 BLE streaming ---
// Phone controls streaming by writing '1'/'0' to CHAR_HCSR04_CTRL_UUID.
bool ble_hcsr04_streaming_enabled(void);
void ble_hcsr04_set_streaming(bool enable);
// Notifies the current distance in centimeters (uint16 LE) if connected & data char is ready.
// Returns ESP_OK if notification was queued, otherwise an error (e.g. ESP_FAIL / ESP_ERR_INVALID_STATE).
int ble_hcsr04_notify_distance_cm(uint16_t distance_cm);

// --- BLE Alert Notifications ---
// Sends a sensor alert notification to the phone if connected and notifications are enabled.
// Format: "SENSOR: message" (max 64 bytes total)
// Returns ESP_OK if notification was queued, -1 if not connected/ready, -2 if notifications disabled.
int ble_send_alert(const char* sensor_name, const char* message);