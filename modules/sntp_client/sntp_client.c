#include "sntp_client.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include <sys/time.h>

static const char *TAG = "SNTP_CLIENT";
static bool g_time_synced = false;
static uint32_t g_last_known_timestamp = 0;
static int64_t g_boot_time_us = 0;

#define NVS_NAMESPACE "sntp_client"
#define NVS_TIMESTAMP_KEY "last_timestamp"


static void sntp_timestamp_persist_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        uint32_t current_timestamp = sntp_client_get_timestamp();
        if (current_timestamp > 0) {
            nvs_handle_t handle;
            esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
            if (err == ESP_OK) {
                err = nvs_set_u32(handle, NVS_TIMESTAMP_KEY, current_timestamp);
                nvs_close(handle);
            }
        }
    }
}

void sntp_client_init(void) {
    g_boot_time_us = esp_timer_get_time();
    
    // Odczytaj ostatni zapisany timestamp z NVS
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_u32(handle, NVS_TIMESTAMP_KEY, &g_last_known_timestamp);
        if (err == ESP_OK && g_last_known_timestamp > 0) {
            ESP_LOGI(TAG, "Restored last known timestamp from NVS: %lu", g_last_known_timestamp);
            
            // Ustaw systemowy czas na ostatni znany + uptime
            struct timeval tv;
            tv.tv_sec = g_last_known_timestamp;
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);
        } else {
            ESP_LOGW(TAG, "No timestamp found in NVS");
        }
        nvs_close(handle);
    } else {
        ESP_LOGW(TAG, "Failed to open NVS for reading timestamp");
    }
    
    // Uruchom task do periodycznego zapisywania timestampu
    xTaskCreate(sntp_timestamp_persist_task, "sntp_persist", 2048, NULL, 5, NULL);
}

void sntp_client_set_timestamp(uint32_t unix_timestamp) {
    struct timeval tv;
    tv.tv_sec = unix_timestamp;
    tv.tv_usec = 0;
    
    settimeofday(&tv, NULL);
    g_time_synced = true;
    g_last_known_timestamp = unix_timestamp;
    g_boot_time_us = esp_timer_get_time();
    
    // Zapisz do NVS dla późniejszego użycia po resecie
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, NVS_TIMESTAMP_KEY, unix_timestamp);
        if (err == ESP_OK) {
            nvs_commit(handle);
            ESP_LOGI(TAG, "Timestamp synced via BLE and saved to NVS: %lu", unix_timestamp);
        } else {
            ESP_LOGW(TAG, "Failed to save timestamp to NVS");
        }
        nvs_close(handle);
    } else {
        ESP_LOGW(TAG, "Failed to open NVS for writing timestamp");
    }
}

uint32_t sntp_client_get_timestamp(void) {
    if (g_time_synced) {
        // Jeśli BLE zsynchronizował czas w tej sesji, użyj time()
        return (uint32_t)time(NULL);
    } else if (g_last_known_timestamp > 0) {
        // Jeśli mamy ostatni znany timestamp z NVS, dodaj uptime
        int64_t uptime_sec = (esp_timer_get_time() - g_boot_time_us) / 1000000;
        return g_last_known_timestamp + (uint32_t)uptime_sec;
    }
    
    // Fallback: nie mamy żadnego czasu
    return 0;
}

bool sntp_client_is_synced(void) {
    return g_time_synced || (g_last_known_timestamp > 0);
}
