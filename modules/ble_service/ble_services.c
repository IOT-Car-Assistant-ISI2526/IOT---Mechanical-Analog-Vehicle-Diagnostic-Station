#include "ble_internal.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_gap_ble_api.h"
#include "storage_manager.h"

// Zmienne statyczne na uchwyty
static uint16_t s_service_handle;
static uint16_t s_char_notes_handle;
static uint16_t s_char_ssid_handle;
static uint16_t s_char_pass_handle;

// Helper: Zapis do NVS
static void save_wifi_cred_to_nvs(const char* key, const char* value) {
    nvs_handle_t my_handle;
    if (nvs_open("storage", NVS_READWRITE, &my_handle) == ESP_OK) {
        nvs_set_str(my_handle, key, value);
        nvs_commit(my_handle);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "NVS Zapisano %s: %s", key, value);
    } else {
        ESP_LOGE(TAG, "Błąd zapisu do NVS");
    }
}

void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        // 1. REJESTRACJA -> TWORZYMY SERWIS
        case ESP_GATTS_REG_EVT: {
            ESP_LOGI(TAG, "Tworzenie serwisu...");
            esp_gatt_srvc_id_t service_id;
            service_id.is_primary = true;
            service_id.id.inst_id = 0x00;
            service_id.id.uuid.len = ESP_UUID_LEN_16;
            service_id.id.uuid.uuid.uuid16 = SERVICE_UUID;

            esp_ble_gatts_create_service(gatts_if, &service_id, 12);
            break;
        }

        // 2. SERWIS UTWORZONY -> DODAJEMY PIERWSZĄ CHARAKTERYSTYKĘ
        case ESP_GATTS_CREATE_EVT: {
            s_service_handle = param->create.service_handle;
            
            esp_bt_uuid_t uuid;
            uuid.len = ESP_UUID_LEN_16;
            uuid.uuid.uuid16 = CHAR_NOTES_UUID;
            
            esp_ble_gatts_add_char(s_service_handle, &uuid, 
                                   ESP_GATT_PERM_WRITE, 
                                   ESP_GATT_CHAR_PROP_BIT_WRITE, NULL, NULL);
            break;
        }

        // 3. ŁAŃCUCH DODAWANIA CHARAKTERYSTYK
        case ESP_GATTS_ADD_CHAR_EVT: {
            uint16_t added_uuid = param->add_char.char_uuid.uuid.uuid16;
            
            if (added_uuid == CHAR_NOTES_UUID) {
                s_char_notes_handle = param->add_char.attr_handle;
                
                // Dodaj SSID
                esp_bt_uuid_t uuid;
                uuid.len = ESP_UUID_LEN_16;
                uuid.uuid.uuid16 = CHAR_SSID_UUID;
                esp_ble_gatts_add_char(s_service_handle, &uuid, 
                                       ESP_GATT_PERM_WRITE, 
                                       ESP_GATT_CHAR_PROP_BIT_WRITE, NULL, NULL);
            } 
            else if (added_uuid == CHAR_SSID_UUID) {
                s_char_ssid_handle = param->add_char.attr_handle;
                
                // Dodaj HASŁO
                esp_bt_uuid_t uuid;
                uuid.len = ESP_UUID_LEN_16;
                uuid.uuid.uuid16 = CHAR_PASS_UUID;
                esp_ble_gatts_add_char(s_service_handle, &uuid, 
                                       ESP_GATT_PERM_WRITE, 
                                       ESP_GATT_CHAR_PROP_BIT_WRITE, NULL, NULL);
            } 
            else if (added_uuid == CHAR_PASS_UUID) {
                s_char_pass_handle = param->add_char.attr_handle;
                // Startujemy serwis
                esp_ble_gatts_start_service(s_service_handle);
            }
            break;
        }

        case ESP_GATTS_START_EVT:
            ESP_LOGI(TAG, "Serwis gotowy.");
            break;

        // --- OBSŁUGA ZAPISU (TUTAJ JEST FIX) ---
        case ESP_GATTS_WRITE_EVT: {
            
            // 1. WYSŁANIE ODPOWIEDZI (Fix na Error 133 / Error 22)
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }

            // 2. Przetwarzanie danych
            if (param->write.len > 0) {
                char buffer[65];
                int len = (param->write.len > 64) ? 64 : param->write.len;
                memcpy(buffer, param->write.value, len);
                buffer[len] = '\0';

                if (param->write.handle == s_char_notes_handle) {
                    ESP_LOGI(TAG, "Notatka: %s", buffer);
                    storage_write_line(buffer);
                } 
                else if (param->write.handle == s_char_ssid_handle) {
                    ESP_LOGI(TAG, "SSID: %s", buffer);
                    save_wifi_cred_to_nvs("wifi_ssid", buffer);
                } 
                else if (param->write.handle == s_char_pass_handle) {
                    ESP_LOGI(TAG, "Pass: %s", buffer);
                    save_wifi_cred_to_nvs("wifi_pass", buffer);
                }
            }
            break;
        }
        
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "Połączono.");
            break;
            
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "Rozłączono.");
            break;

        default: break;
    }
}