#pragma once
#include <stdint.h>
#include "esp_gatts_api.h"


static const char *TAG = "BLE_SERVER";
// #define TAG "BLE_SERVER"
#define DEVICE_NAME "ESP32-NotesWiFi"

// --- UUID Definicje ---
#define SERVICE_UUID        0x00FF
#define CHAR_NOTES_UUID     0xFF01
#define CHAR_SSID_UUID      0xFF02
#define CHAR_PASS_UUID      0xFF03
#define CHAR_WIFI_SWITCH_UUID   0xFF04
#define CHAR_HCSR04_CTRL_UUID   0xFF05  // WRITE: '1' start stream, '0' stop stream
#define CHAR_HCSR04_DATA_UUID   0xFF06  // NOTIFY: uint16 distance_cm (LE)
#define CHAR_ALERT_UUID         0xFF07  // NOTIFY: Sensor alerts (string)
#define ESP_GATT_UUID_CHAR_DESCRIPTION  0x2901
#define ESP_GATT_UUID_CHAR_CLIENT_CONFIG 0x2902

#define PROFILE_APP_ID      0

// Funkcja z pliku ble_services.c, którą wywołuje core
void gatts_profile_event_handler(esp_gatts_cb_event_t event, 
                                 esp_gatt_if_t gatts_if, 
                                 esp_ble_gatts_cb_param_t *param);