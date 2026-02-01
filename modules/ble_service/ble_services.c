#include "ble_internal.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdatomic.h>
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
#include "sntp_client.h"
#include "esp_mac.h"

static uint16_t s_service_handle;
static uint16_t s_char_notes_handle;
static uint16_t s_char_ssid_handle;
static uint16_t s_char_pass_handle;
static uint16_t s_char_wifi_switch_handle;
static uint16_t s_char_hcsr04_ctrl_handle;
static uint16_t s_char_hcsr04_data_handle;
static uint16_t s_char_hcsr04_cccd_handle;
static uint16_t s_char_alert_handle;
static uint16_t s_char_alert_cccd_handle;
static uint16_t s_char_max6675_profile_ctrl_handle;
static uint16_t s_char_max6675_profile_data_handle;
static uint16_t s_char_max6675_profile_data_cccd_handle;


static _Atomic bool s_hcsr04_streaming_enabled = false;
static _Atomic bool s_hcsr04_ctrl_wants_stream = false;
static _Atomic bool s_hcsr04_notifications_enabled = false;
static _Atomic bool s_alert_notifications_enabled = false;
static _Atomic bool s_max6675_profile_requested = false;

static bool s_is_connected = false;
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_conn_id = 0;

void ble_notify_max6675_profile(float temperature);
int ble_send_alert(const char* sensor_name, const char* message);

typedef enum
{
    STAGE_NONE = 0,
    STAGE_NOTES_DESC_ADDED,
    STAGE_SSID_DESC_ADDED,
    STAGE_PASS_DESC_ADDED,
    STAGE_WIFI_SWITCH_DESC_ADDED,
    STAGE_HCSR04_CTRL_DESC_ADDED,
    STAGE_HCSR04_DATA_DESC_ADDED,
    STAGE_HCSR04_DATA_CCCD_ADDED,
    STAGE_ALERT_DESC_ADDED,
    STAGE_ALERT_CCCD_ADDED,
    STAGE_MAX6675_PROFILE_CTRL_DESC_ADDED,
    STAGE_MAX6675_PROFILE_DATA_ADDED,
    STAGE_MAX6675_PROFILE_DATA_CCCD_ADDED,

} ble_gatt_build_stage_t;

static ble_gatt_build_stage_t s_build_stage = STAGE_NONE;

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

typedef struct {
    char ssid[33];
    char pass[65];
} wifi_start_arg_t;

static void ble_wifi_start_task(void* arg)
{
    wifi_start_arg_t* creds = (wifi_start_arg_t*)arg;

    ESP_LOGI(TAG, "Starting WiFi to SSID: %s", creds->ssid);

    wifi_config_t cfg = {0};
    strncpy((char*)cfg.sta.ssid, creds->ssid, sizeof(cfg.sta.ssid));
    strncpy((char*)cfg.sta.password, creds->pass, sizeof(cfg.sta.password));

    esp_err_t err = esp_wifi_start();
    if (err == ESP_OK) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi started and connecting...");
    } else if (err == ESP_ERR_WIFI_NOT_INIT) {
        wifi_station_init();
    } else {
        ESP_LOGW(TAG, "WiFi start returned %s", esp_err_to_name(err));
    }

    vPortFree(creds);
    vTaskDelete(NULL);
}

typedef struct {
    float temperature;
} max6675_task_arg_t;

static void ble_max6675_notify_task(void* arg)
{
    max6675_task_arg_t* data = (max6675_task_arg_t*)arg;
    ble_notify_max6675_profile(data->temperature);
    vPortFree(data);
    vTaskDelete(NULL);
}



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

static void add_user_description(uint16_t service_handle, const char* name) {
    esp_bt_uuid_t descr_uuid;
    descr_uuid.len = ESP_UUID_LEN_16;
    descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_DESCRIPTION;

    esp_attr_value_t char_descr_val;
    char_descr_val.attr_max_len = strlen(name);
    char_descr_val.attr_len = strlen(name);
    char_descr_val.attr_value = (uint8_t *)name;

    esp_attr_control_t control;
    control.auto_rsp = ESP_GATT_AUTO_RSP;

    esp_ble_gatts_add_char_descr(service_handle, &descr_uuid,
                                 ESP_GATT_PERM_READ,
                                 &char_descr_val, 
                                 &control);
}

static void add_cccd(uint16_t service_handle)
{
    esp_bt_uuid_t descr_uuid;
    descr_uuid.len = ESP_UUID_LEN_16;
    descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

    static uint8_t cccd_val[2] = {0x00, 0x00};
    esp_attr_value_t descr_val = {
        .attr_max_len = sizeof(cccd_val),
        .attr_len = sizeof(cccd_val),
        .attr_value = cccd_val,
    };

    esp_attr_control_t control;
    control.auto_rsp = ESP_GATT_AUTO_RSP;

    esp_ble_gatts_add_char_descr(service_handle,
                                 &descr_uuid,
                                 ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                 &descr_val,
                                 &control);
}

void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        
        case ESP_GATTS_REG_EVT: {
            ESP_LOGI(TAG, "Tworzenie serwisu...");
            esp_gatt_srvc_id_t service_id;
            service_id.is_primary = true;
            service_id.id.inst_id = 0x00;
            service_id.id.uuid.len = ESP_UUID_LEN_16;
            service_id.id.uuid.uuid.uuid16 = SERVICE_UUID;

            esp_ble_gatts_create_service(gatts_if, &service_id, 32);
            break;
        }

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

        case ESP_GATTS_ADD_CHAR_EVT: {
            uint16_t added_uuid = param->add_char.char_uuid.uuid.uuid16;
            
            if (added_uuid == CHAR_NOTES_UUID) {
                s_char_notes_handle = param->add_char.attr_handle;
                add_user_description(s_service_handle, "Notes/Timestamp (4 bytes=timestamp, text=note)");
                s_build_stage = STAGE_NOTES_DESC_ADDED;
            } 
            else if (added_uuid == CHAR_SSID_UUID) {
                s_char_ssid_handle = param->add_char.attr_handle;
                add_user_description(s_service_handle, "WiFi SSID");
                s_build_stage = STAGE_SSID_DESC_ADDED;
            } 
            else if (added_uuid == CHAR_PASS_UUID) {
                s_char_pass_handle = param->add_char.attr_handle;
                add_user_description(s_service_handle, "WiFi Password");
                s_build_stage = STAGE_PASS_DESC_ADDED;
            }
            else if (added_uuid == CHAR_WIFI_SWITCH_UUID) {
                s_char_wifi_switch_handle = param->add_char.attr_handle;
                add_user_description(s_service_handle, "WiFi ON/OFF (1=ON, 0=OFF)");
                s_build_stage = STAGE_WIFI_SWITCH_DESC_ADDED;
            }
            else if (added_uuid == CHAR_HCSR04_CTRL_UUID) {
                s_char_hcsr04_ctrl_handle = param->add_char.attr_handle;
                add_user_description(s_service_handle, "HCSR04 stream control (1=start, 0=stop)");
                s_build_stage = STAGE_HCSR04_CTRL_DESC_ADDED;
            }
            else if (added_uuid == CHAR_HCSR04_DATA_UUID) {
                s_char_hcsr04_data_handle = param->add_char.attr_handle;
                add_user_description(s_service_handle, "HCSR04 distance_cm (uint16, notify)");
                s_build_stage = STAGE_HCSR04_DATA_DESC_ADDED;
            }
            else if (added_uuid == CHAR_ALERT_UUID) {
                s_char_alert_handle = param->add_char.attr_handle;
                add_user_description(s_service_handle, "Sensor Alerts (notify)");
                s_build_stage = STAGE_ALERT_DESC_ADDED;
            }
            else if (added_uuid == CHAR_MAX6675_PROFILE_CTRL_UUID)
            {
                s_char_max6675_profile_ctrl_handle = param->add_char.attr_handle;
                add_user_description(s_service_handle, "MAX6675 profile control (1=start)");
                s_build_stage = STAGE_MAX6675_PROFILE_CTRL_DESC_ADDED;
            }
            else if (added_uuid == CHAR_MAX6675_PROFILE_DATA_UUID)
            {
                s_char_max6675_profile_data_handle = param->add_char.attr_handle;
                add_user_description(s_service_handle, "MAX6675 profile temperature (float, notify)");
                s_build_stage = STAGE_MAX6675_PROFILE_DATA_ADDED;
            }

            break;
        }

        case ESP_GATTS_ADD_CHAR_DESCR_EVT: {
            const uint16_t descr_uuid = param->add_char_descr.descr_uuid.uuid.uuid16;
            const uint16_t descr_handle = param->add_char_descr.attr_handle;

            if (descr_uuid == ESP_GATT_UUID_CHAR_CLIENT_CONFIG)
            {
                if (s_build_stage == STAGE_HCSR04_DATA_DESC_ADDED)
                {
                    s_char_hcsr04_cccd_handle = descr_handle;
                    s_build_stage = STAGE_HCSR04_DATA_CCCD_ADDED;
                }
                else if (s_build_stage == STAGE_ALERT_DESC_ADDED)
                {
                    s_char_alert_cccd_handle = descr_handle;
                    s_build_stage = STAGE_ALERT_CCCD_ADDED;
                }
                else if (s_build_stage == STAGE_MAX6675_PROFILE_DATA_ADDED)
                {
                    s_char_max6675_profile_data_cccd_handle = descr_handle;
                    s_build_stage = STAGE_MAX6675_PROFILE_DATA_CCCD_ADDED;
                }
            }

            if (s_build_stage == STAGE_NOTES_DESC_ADDED)
            {
                esp_bt_uuid_t uuid = {.len = ESP_UUID_LEN_16, .uuid.uuid16 = CHAR_SSID_UUID};
                esp_ble_gatts_add_char(s_service_handle, &uuid, ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE, NULL, NULL);
            }
            else if (s_build_stage == STAGE_SSID_DESC_ADDED)
            {
                esp_bt_uuid_t uuid = {.len = ESP_UUID_LEN_16, .uuid.uuid16 = CHAR_PASS_UUID};
                esp_ble_gatts_add_char(s_service_handle, &uuid, ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE, NULL, NULL);
            }
            else if (s_build_stage == STAGE_PASS_DESC_ADDED)
            {
                esp_bt_uuid_t uuid = {.len = ESP_UUID_LEN_16, .uuid.uuid16 = CHAR_WIFI_SWITCH_UUID};
                esp_ble_gatts_add_char(s_service_handle, &uuid,
                                       ESP_GATT_PERM_WRITE,
                                       ESP_GATT_CHAR_PROP_BIT_WRITE,
                                       NULL, NULL);
            }
            else if (s_build_stage == STAGE_WIFI_SWITCH_DESC_ADDED)
            {
                esp_bt_uuid_t uuid = {.len = ESP_UUID_LEN_16, .uuid.uuid16 = CHAR_HCSR04_CTRL_UUID};
                esp_ble_gatts_add_char(s_service_handle, &uuid,
                                       ESP_GATT_PERM_WRITE,
                                       ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
                                       NULL, NULL);
            }
            else if (s_build_stage == STAGE_HCSR04_CTRL_DESC_ADDED)
            {
                esp_bt_uuid_t uuid = {.len = ESP_UUID_LEN_16, .uuid.uuid16 = CHAR_HCSR04_DATA_UUID};
                esp_ble_gatts_add_char(s_service_handle, &uuid,
                                       ESP_GATT_PERM_READ,
                                       ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                       NULL, NULL);
            }
            else if (s_build_stage == STAGE_HCSR04_DATA_DESC_ADDED)
            {
                add_cccd(s_service_handle);
            }
            else if (s_build_stage == STAGE_HCSR04_DATA_CCCD_ADDED)
            {
                esp_bt_uuid_t uuid = {.len = ESP_UUID_LEN_16, .uuid.uuid16 = CHAR_ALERT_UUID};
                esp_ble_gatts_add_char(s_service_handle, &uuid,
                                       ESP_GATT_PERM_READ,
                                       ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                       NULL, NULL);
            }
            else if (s_build_stage == STAGE_ALERT_DESC_ADDED)
            {
                add_cccd(s_service_handle);
            }
            else if (s_build_stage == STAGE_ALERT_CCCD_ADDED)
            {
                esp_bt_uuid_t uuid = {
                    .len = ESP_UUID_LEN_16,
                    .uuid.uuid16 = CHAR_MAX6675_PROFILE_CTRL_UUID};

                esp_ble_gatts_add_char(
                    s_service_handle,
                    &uuid,
                    ESP_GATT_PERM_WRITE,
                    ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
                    NULL,
                    NULL);
            }
            else if (s_build_stage == STAGE_MAX6675_PROFILE_CTRL_DESC_ADDED)
            {
                esp_bt_uuid_t uuid = {
                    .len = ESP_UUID_LEN_16,
                    .uuid.uuid16 = CHAR_MAX6675_PROFILE_DATA_UUID};

                esp_ble_gatts_add_char(
                    s_service_handle,
                    &uuid,
                    ESP_GATT_PERM_READ,
                    ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_READ,
                    NULL,
                    NULL);
            }
            else if (s_build_stage == STAGE_MAX6675_PROFILE_DATA_ADDED)
            {
                add_cccd(s_service_handle);
            }
            else if (s_build_stage == STAGE_MAX6675_PROFILE_DATA_CCCD_ADDED)
            {
                esp_ble_gatts_start_service(s_service_handle);
            }

            break;
        }

        case ESP_GATTS_START_EVT:
            ESP_LOGI(TAG, "Service ready.");
            break;

        case ESP_GATTS_WRITE_EVT: {
            
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }

            if (param->write.len > 0) {
                char buffer[65];
                int len = (param->write.len > 64) ? 64 : param->write.len;
                memcpy(buffer, param->write.value, len);
                buffer[len] = '\0';

                
                if (param->write.handle == s_char_notes_handle) {
                    ESP_LOGI(TAG, "FF01 write received: len=%d, hex: %02X %02X %02X %02X %02X", 
                             param->write.len,
                             param->write.len > 0 ? param->write.value[0] : 0,
                             param->write.len > 1 ? param->write.value[1] : 0,
                             param->write.len > 2 ? param->write.value[2] : 0,
                             param->write.len > 3 ? param->write.value[3] : 0,
                             param->write.len > 4 ? param->write.value[4] : 0);
                    
                    if (param->write.len >= 4) {
                        uint32_t timestamp = 0;
                        memcpy(&timestamp, param->write.value, 4);
                        sntp_client_set_timestamp(timestamp);
                        ESP_LOGI(TAG, "Timestamp synced via BLE (FF01): %lu", timestamp);
                    } else {
                        ESP_LOGI(TAG, "Notatka: %s", buffer);
                        storage_write_line(buffer);
                    }
                } 
                else if (param->write.handle == s_char_ssid_handle) {
                    ESP_LOGI(TAG, "SSID: %s", buffer);
                    save_wifi_cred_to_nvs("wifi_ssid", buffer);
                } 
                else if (param->write.handle == s_char_pass_handle) {
                    ESP_LOGI(TAG, "Pass: %s", buffer);
                    save_wifi_cred_to_nvs("wifi_pass", buffer);
                }
                else if (param->write.handle == s_char_max6675_profile_ctrl_handle)
                {
                    char command = buffer[0];
                    ESP_LOGI(TAG, "MAX6675 PROFILE ctrl: %c", command);

                    if (command == '1')
                    {
                        atomic_store(&s_max6675_profile_requested, true);
                    }
                }

else if (param->write.handle == s_char_wifi_switch_handle)
{
    char command = buffer[0];
    if (command == '1') {
        char ssid[33], pass[65];
        if (!read_credentials_from_nvs(ssid, sizeof(ssid), pass, sizeof(pass))) {
            ESP_LOGW(TAG, "WiFi credentials missing in NVS");
            break;
        }

        wifi_start_arg_t* arg = pvPortMalloc(sizeof(wifi_start_arg_t));
        strncpy(arg->ssid, ssid, sizeof(arg->ssid));
        strncpy(arg->pass, pass, sizeof(arg->pass));

        xTaskCreate(ble_wifi_start_task, "ble_wifi_start", 4096, arg, 5, NULL);
    } else if (command == '0') {
        ESP_LOGI(TAG, "Stopping WiFi...");
        esp_wifi_stop();
    }
}

                else if (param->write.handle == s_char_hcsr04_ctrl_handle) {
                    char command = buffer[0];
                    ESP_LOGI(TAG, "HCSR04 stream ctrl: %c", command);
                    
                    bool ctrl_wants_stream = (command == '1');
                    atomic_store(&s_hcsr04_ctrl_wants_stream, ctrl_wants_stream);
                    
                    bool notif_enabled = atomic_load(&s_hcsr04_notifications_enabled);
                    atomic_store(&s_hcsr04_streaming_enabled, ctrl_wants_stream && notif_enabled);
                    
                    ESP_LOGI(TAG, "HCSR04 CTRL: command=%c, ctrl_wants=%d, notif=%d, streaming=%d", 
                             command, (int)ctrl_wants_stream, (int)notif_enabled, 
                             (int)atomic_load(&s_hcsr04_streaming_enabled));
                }

                if (param->write.handle == s_char_hcsr04_cccd_handle && param->write.len >= 2) {
                    const uint16_t cccd = (uint16_t)param->write.value[0] | ((uint16_t)param->write.value[1] << 8);
                    const bool notif_enabled = (cccd & 0x0001) != 0;
                    
                    atomic_store(&s_hcsr04_notifications_enabled, notif_enabled);
                    ESP_LOGI(TAG, "HCSR04 CCCD written: 0x%04X, notifications=%d", cccd, (int)notif_enabled);
                    
                
                    bool ctrl_wants_stream = atomic_load(&s_hcsr04_ctrl_wants_stream);
                    atomic_store(&s_hcsr04_streaming_enabled, ctrl_wants_stream && notif_enabled);
                    
                    ESP_LOGI(TAG, "HCSR04 CCCD update: ctrl_wants=%d, notif=%d, streaming=%d",
                             (int)ctrl_wants_stream, (int)notif_enabled,
                             (int)atomic_load(&s_hcsr04_streaming_enabled));
                }
                else if (param->write.handle == s_char_alert_cccd_handle && param->write.len >= 2) {
                    const uint16_t cccd = (uint16_t)param->write.value[0] | ((uint16_t)param->write.value[1] << 8);
                    const bool notif_enabled = (cccd & 0x0001) != 0;
                    
                    atomic_store(&s_alert_notifications_enabled, notif_enabled);
                    ESP_LOGI(TAG, "Alert CCCD written: 0x%04X, notifications=%d", cccd, (int)notif_enabled);

                    uint8_t mac[6];
                    char mac_alert[80];
                    esp_read_mac(mac, ESP_MAC_WIFI_STA);
                    snprintf(mac_alert, sizeof(mac_alert), "%02X%02X%02X%02X%02X%02X",
                             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                    ble_send_alert("MAC", mac_alert);
                    ESP_LOGI(TAG, "MAC: %s", mac_alert);
                }
            }
            break;
        }
        
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "Połączono.");
            s_is_connected = true;
            s_gatts_if = gatts_if;
            s_conn_id = param->connect.conn_id;
            break;
            
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "Rozłączono. Wznawiam rozgłaszanie...");
            s_is_connected = false;
            s_gatts_if = ESP_GATT_IF_NONE;
            atomic_store(&s_hcsr04_streaming_enabled, false);
            atomic_store(&s_hcsr04_ctrl_wants_stream, false);
            atomic_store(&s_hcsr04_notifications_enabled, false);
            atomic_store(&s_alert_notifications_enabled, false);
            esp_ble_gap_start_advertising(&adv_params); 
            break;

        default: break;
    }
}

bool ble_hcsr04_streaming_enabled(void)
{
    return atomic_load(&s_hcsr04_streaming_enabled);
}

void ble_hcsr04_set_streaming(bool enable)
{
    atomic_store(&s_hcsr04_streaming_enabled, enable);
}

int ble_hcsr04_notify_distance_cm(uint16_t distance_cm)
{
    if (!s_is_connected || s_gatts_if == ESP_GATT_IF_NONE || s_char_hcsr04_data_handle == 0) {
        ESP_LOGW(TAG, "Cannot notify: BLE not ready");
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t payload[2] = {(uint8_t)(distance_cm & 0xFF), (uint8_t)((distance_cm >> 8) & 0xFF)};

    esp_err_t err = esp_ble_gatts_send_indicate(s_gatts_if,
                                               s_conn_id,
                                               s_char_hcsr04_data_handle,
                                               sizeof(payload),
                                               payload,
                                               false);
    return (int)err;
}

int ble_send_alert(const char* sensor_name, const char* message)
{
    if (!s_is_connected || s_gatts_if == ESP_GATT_IF_NONE || s_char_alert_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!atomic_load(&s_alert_notifications_enabled)) {
        return -2;
    }
    
    char alert_msg[65];
    int len = snprintf(alert_msg, sizeof(alert_msg), "%s: %s", sensor_name, message);
    if (len < 0 || len >= (int)sizeof(alert_msg)) {
        len = sizeof(alert_msg) - 1;
        alert_msg[len] = '\0';
    }
    
    ESP_LOGI(TAG, "Sending BLE alert: %s", alert_msg);
    
    esp_err_t err = esp_ble_gatts_send_indicate(s_gatts_if,
                                               s_conn_id,
                                               s_char_alert_handle,
                                               len,
                                               (uint8_t*)alert_msg,
                                               false);
    return (int)err;
}

bool ble_max6675_profile_requested(void)
{
    return atomic_load(&s_max6675_profile_requested);
}

void ble_max6675_clear_profile_request(void)
{
    atomic_store(&s_max6675_profile_requested, false);
}

void ble_notify_max6675_profile(float temperature)
{
    if (!s_is_connected || s_char_max6675_profile_data_handle == 0)
        return;

    uint8_t payload[4];
    memcpy(payload, &temperature, sizeof(float));

    esp_ble_gatts_send_indicate(
        s_gatts_if,
        s_conn_id,
        s_char_max6675_profile_data_handle,
        sizeof(payload),
        payload,
        false
    );
}
