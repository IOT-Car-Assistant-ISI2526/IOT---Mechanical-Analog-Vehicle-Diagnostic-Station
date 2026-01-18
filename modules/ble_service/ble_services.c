#include "ble_internal.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_wifi.h"
#include "storage_manager.h"
#include "wifi_station.h"

// Zmienne statyczne na uchwyty (Handles)
static uint16_t s_service_handle;
static uint16_t s_char_notes_handle;
static uint16_t s_char_ssid_handle;
static uint16_t s_char_pass_handle;
static uint16_t s_char_wifi_switch_handle;

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// --- HELPER: ZAPIS DO NVS ---
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

// --- HELPER: DODAWANIE OPISU ---
static void add_user_description(uint16_t service_handle, const char* name) {
    esp_bt_uuid_t descr_uuid;
    descr_uuid.len = ESP_UUID_LEN_16;
    descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_DESCRIPTION;

    esp_attr_value_t char_descr_val;
    char_descr_val.attr_max_len = strlen(name);
    char_descr_val.attr_len = strlen(name);
    char_descr_val.attr_value = (uint8_t *)name;

    // WAŻNE: auto_rsp = ESP_GATT_AUTO_RSP naprawia błąd odczytu (Error 133)
    esp_attr_control_t control;
    control.auto_rsp = ESP_GATT_AUTO_RSP;

    esp_ble_gatts_add_char_descr(service_handle, &descr_uuid,
                                 ESP_GATT_PERM_READ,
                                 &char_descr_val, 
                                 &control);
}

// --- GŁÓWNY HANDLER ---
void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        
        // 1. REJESTRACJA -> TWORZYMY SERWIS
        case ESP_GATTS_REG_EVT: {
            ESP_LOGI(TAG, "Tworzenie serwisu...");
            esp_gatt_srvc_id_t service_id;
            service_id.is_primary = true;
            service_id.id.inst_id = 0x00;
            service_id.id.uuid.len = ESP_UUID_LEN_16;
            service_id.id.uuid.uuid.uuid16 = SERVICE_UUID; // Zakładam, że SERVICE_UUID jest w ble_internal.h

            // Zwiększamy liczbę uchwytów (1 serwis + 4 chary * 2 uchwyty = 9 -> dajemy z zapasem 16)
            esp_ble_gatts_create_service(gatts_if, &service_id, 16);
            break;
        }

        // 2. SERWIS UTWORZONY -> DODAJEMY PIERWSZĄ CHARAKTERYSTYKĘ (NOTES)
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

        // 3. OBSŁUGA DODANIA CHARAKTERYSTYKI -> TUTAJ DODAJEMY JEJ OPIS
        case ESP_GATTS_ADD_CHAR_EVT: {
            uint16_t added_uuid = param->add_char.char_uuid.uuid.uuid16;
            
            if (added_uuid == CHAR_NOTES_UUID) {
                s_char_notes_handle = param->add_char.attr_handle;
                add_user_description(s_service_handle, "Notatki");
            } 
            else if (added_uuid == CHAR_SSID_UUID) {
                s_char_ssid_handle = param->add_char.attr_handle;
                add_user_description(s_service_handle, "WiFi SSID");
            } 
            else if (added_uuid == CHAR_PASS_UUID) {
                s_char_pass_handle = param->add_char.attr_handle;
                add_user_description(s_service_handle, "WiFi Password");
            }
            else if (added_uuid == CHAR_WIFI_SWITCH_UUID) {
                s_char_wifi_switch_handle = param->add_char.attr_handle;
                add_user_description(s_service_handle, "WiFi ON/OFF (1=ON, 0=OFF)");
            }
            break;
        }

        // 4. ŁAŃCUCH TWORZENIA: OPIS DODANY -> DODAJ NASTĘPNĄ CHARAKTERYSTYKĘ
        case ESP_GATTS_ADD_CHAR_DESCR_EVT: {
            // Mamy Notatki -> Dodaj SSID
            if (s_char_notes_handle != 0 && s_char_ssid_handle == 0) {
                esp_bt_uuid_t uuid = { .len = ESP_UUID_LEN_16, .uuid.uuid16 = CHAR_SSID_UUID };
                esp_ble_gatts_add_char(s_service_handle, &uuid, ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE, NULL, NULL);
            }
            // Mamy SSID -> Dodaj PASS
            else if (s_char_ssid_handle != 0 && s_char_pass_handle == 0) {
                esp_bt_uuid_t uuid = { .len = ESP_UUID_LEN_16, .uuid.uuid16 = CHAR_PASS_UUID };
                esp_ble_gatts_add_char(s_service_handle, &uuid, ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE, NULL, NULL);
            }
            // Mamy PASS -> Dodaj SWITCH
            else if (s_char_pass_handle != 0 && s_char_wifi_switch_handle == 0) {
                esp_bt_uuid_t uuid = { .len = ESP_UUID_LEN_16, .uuid.uuid16 = CHAR_WIFI_SWITCH_UUID };
                // Switch ma WRITE (sterowanie) i opcjonalnie READ
                esp_ble_gatts_add_char(s_service_handle, &uuid, 
                                       ESP_GATT_PERM_WRITE, 
                                       ESP_GATT_CHAR_PROP_BIT_WRITE, NULL, NULL);
            }
            // Mamy wszystko -> Start Serwisu
            else if (s_char_wifi_switch_handle != 0) {
                esp_ble_gatts_start_service(s_service_handle);
            }
            break;
        }

        case ESP_GATTS_START_EVT:
            ESP_LOGI(TAG, "Serwis gotowy i wystartowany.");
            break;

        // --- OBSŁUGA ZAPISU ---
        case ESP_GATTS_WRITE_EVT: {
            
            // 1. Potwierdzenie (ACK) - ważne dla stabilności
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }

            // 2. Przetwarzanie danych
            if (param->write.len > 0) {
                // Kopiowanie danych do bufora (bezpieczne zakończenie stringa)
                char buffer[65];
                int len = (param->write.len > 64) ? 64 : param->write.len;
                memcpy(buffer, param->write.value, len);
                buffer[len] = '\0';

                // --- Logika dla poszczególnych charakterystyk ---
                
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
                else if (param->write.handle == s_char_wifi_switch_handle) {
                    // Sprawdzamy pierwszy znak: '1' lub '0'
                    char command = buffer[0];
                    ESP_LOGI(TAG, "Komenda Switch: %c", command);

                    if (command == '1') {
                        // WŁĄCZ
                        if (wifi_check_credentials()) {
                            ESP_LOGI(TAG, "Odebrano komendę START. Czekam na wysłanie ACK...");
                            
                            // 1. FIX NA ERROR 133:
                            // Daj czas na wysłanie odpowiedzi do telefonu ZANIM obciążysz procesor WiFi
                            vTaskDelay(100 / portTICK_PERIOD_MS);

                            char ssid[33];
                            char pass[65];

                            if (!read_credentials_from_nvs(ssid, sizeof(ssid), pass, sizeof(pass))) {
                                ESP_LOGW(TAG, "Nie udało się wczytać WiFi z NVS");
                                break;
                            }
                            wifi_config_t cfg = {0};

                            strncpy((char*)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
                            strncpy((char*)cfg.sta.password, pass, sizeof(cfg.sta.password));

                            // 2. PRÓBA STARTU (Lepsze niż ciągłe init)
                            // Najpierw próbujemy po prostu wystartować radio (jeśli było tylko zatrzymane)
                            esp_err_t err = esp_wifi_start();
                            if (err == ESP_OK) {
                                ESP_LOGI(TAG, "WiFi wystartowane (esp_wifi_start). Łączę...");
                                esp_wifi_connect(); // Konieczne po starcie
                            } 
                            else if (err == ESP_ERR_WIFI_NOT_INIT) {
                                // Jeśli zwróci błąd, że nie zainicjalizowane - dopiero wtedy robimy pełny init
                                ESP_LOGI(TAG, "WiFi nie było zainicjalizowane. Robię pełny init...");
                                wifi_station_init(); 
                            }
                            else {
                                // Inny błąd (np. już działa)
                                ESP_LOGW(TAG, "Błąd startu WiFi (może już działa?): %s", esp_err_to_name(err));
                            }
                            
                            esp_wifi_disconnect(); // Reset połączenia
                            vTaskDelay(pdMS_TO_TICKS(200));

                            esp_wifi_set_mode(WIFI_MODE_STA);
                            esp_wifi_set_config(WIFI_IF_STA, &cfg);

                            esp_wifi_connect();    // Ponowne łączenie

                        } else {
                            ESP_LOGW(TAG, "Brak danych WiFi w NVS!");
                        }
                    }
                    else if (command == '0') {
                        ESP_LOGI(TAG, "Odebrano komendę STOP. Czekam na wysłanie ACK...");
                        
                        vTaskDelay(100 / portTICK_PERIOD_MS); 

                        ESP_LOGI(TAG, "Zatrzymywanie WiFi...");
                        esp_wifi_stop(); 
                        
                        ESP_LOGI(TAG, "WiFi zatrzymane.");
                    }
                }
            }
            break;
        }
        
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "Połączono.");
            break;
            
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "Rozłączono. Wznawiam rozgłaszanie...");
            esp_ble_gap_start_advertising(&adv_params); 
            break;

        default: break;
    }
}