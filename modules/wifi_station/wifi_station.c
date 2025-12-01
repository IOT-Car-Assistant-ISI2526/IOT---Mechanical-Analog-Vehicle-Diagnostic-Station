#include "wifi_station.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

// Tag do logów
static const char *TAG_WIFI = "wifi_station";

// Flaga informująca czy mamy połączenie
static volatile bool s_is_wifi_connected = false;

// Handler zdarzeń
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    // 1. Start modułu wifi -> Próba łączenia
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    // 2. Rozłączenie / Brak połączenia
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_is_wifi_connected = false; 
        ESP_LOGW(TAG_WIFI, "Brak połączenia. Ponawiam próbę...");
        esp_wifi_connect(); 
    } 
    // 3. Uzyskano adres IP -> SUKCES
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG_WIFI, "Uzyskano adres IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_is_wifi_connected = true;
    }
}

void wifi_station_init(void)
{
    // 1. Odczyt danych z NVS (zapisanych przez Bluetooth)
    nvs_handle_t my_handle;
    char ssid[33] = {0};
    char pass[64] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(pass);

    // Próbujemy otworzyć NVS
    if (nvs_open("storage", NVS_READONLY, &my_handle) == ESP_OK) {
        nvs_get_str(my_handle, "wifi_ssid", ssid, &ssid_len);
        nvs_get_str(my_handle, "wifi_pass", pass, &pass_len);
        nvs_close(my_handle);
    }

    // Jeśli brak SSID, nie blokujemy programu! Pozwalamy działać BLE.
    if (strlen(ssid) == 0) {
        ESP_LOGW(TAG_WIFI, "Brak konfiguracji WiFi w NVS! Użyj Bluetooth, aby ustawić sieć.");
    } else {
        ESP_LOGI(TAG_WIFI, "Wczytano konfigurację: SSID=%s", ssid);
    }

    // 2. Inicjalizacja stosu sieciowego
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 3. Rejestracja handlerów
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    // 4. Konfiguracja i Start (tylko jeśli mamy dane)
    if (strlen(ssid) > 0) {
        wifi_config_t wifi_config = {
            .sta = {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
        };
        // Kopiujemy dane wczytane z NVS do struktury WiFi
        strlcpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        strlcpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
    } else {
        // Jeśli nie mamy danych, ustawiamy tylko tryb, ale nie startujemy łączenia
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    }
}

// Funkcja wywoływana przez BLE po wpisaniu nowego hasła/komendy
void wifi_force_reconnect(void) {
    ESP_LOGI(TAG_WIFI, "Otrzymano rozkaz rekonfiguracji z BLE...");

    // 1. Pobierz nowe dane z NVS
    nvs_handle_t my_handle;
    char ssid[33] = {0};
    char pass[64] = {0};
    size_t len = sizeof(ssid);

    if (nvs_open("storage", NVS_READONLY, &my_handle) == ESP_OK) {
        nvs_get_str(my_handle, "wifi_ssid", ssid, &len);
        len = sizeof(pass);
        nvs_get_str(my_handle, "wifi_pass", pass, &len);
        nvs_close(my_handle);
    }

    if (strlen(ssid) == 0) {
        ESP_LOGE(TAG_WIFI, "Nadal brak SSID w NVS!");
        return;
    }

    // 2. Rozłącz obecne i załaduj nowe
    esp_wifi_stop(); // Zatrzymaj WiFi, żeby przeładować config bezpiecznie
    
    wifi_config_t wifi_config = {
        .sta = { .threshold.authmode = WIFI_AUTH_WPA2_PSK },
    };
    strlcpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 3. Startuj ponownie
    ESP_LOGI(TAG_WIFI, "Łączenie z nową siecią: %s", ssid);
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool wifi_station_is_connected(void)
{
    return s_is_wifi_connected;
}