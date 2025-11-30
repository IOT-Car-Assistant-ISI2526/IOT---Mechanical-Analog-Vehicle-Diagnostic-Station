#include "ble_server.h"
#include "ble_internal.h"
#include <string.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"

// Konfiguracja Advertisingu
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006, 
    .max_interval = 0x0010, 
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// Obsługa zdarzeń GAP (Połączenia / Rozgłaszanie)
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Błąd startu advertisingu");
            }
            break;
        // Obsługa rozłączenia - restart advertisingu
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            break;
        default: break;
    }
}

// Globalny handler GATT - przekierowuje do pliku services
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    
    // 1. Obsługa rejestracji (tylko tutaj ustawiamy nazwę)
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            esp_ble_gap_set_device_name(DEVICE_NAME);
            esp_ble_gap_config_adv_data(&adv_data);
        }
    }
    
    // 2. Restart advertisingu po rozłączeniu
    if (event == ESP_GATTS_DISCONNECT_EVT) {
        ESP_LOGI(TAG, "Rozłączono - wznawiam advertising");
        esp_ble_gap_start_advertising(&adv_params);
    }

    // 3. PRZEKAZANIE DO USŁUG (BEZ WARUNKU!)
    // Usuwamy if (gatts_if == ...), bo blokował on wykonanie kodu.
    gatts_profile_event_handler(event, gatts_if, param);
}

void ble_server_init(void) {
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    
    esp_bluedroid_init();
    esp_bluedroid_enable();
    
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_app_register(PROFILE_APP_ID);
    
    ESP_LOGI(TAG, "BLE Core zainicjowane.");
}