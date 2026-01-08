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
#define ESP_GATT_UUID_CHAR_DESCRIPTION  0x2901

#define PROFILE_APP_ID      0

// Funkcja z pliku ble_services.c, którą wywołuje core
void gatts_profile_event_handler(esp_gatts_cb_event_t event, 
                                 esp_gatt_if_t gatts_if, 
                                 esp_ble_gatts_cb_param_t *param);