#include "wifi_station.h"
#include "project_config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

// tag do logow
static const char *TAG_WIFI = "wifi_station";

// grupa zdarzen, pozwala czekac na polaczenie
static EventGroupHandle_t s_wifi_event_group;

// bit w grupie zdarzen oznaczajacy „wifi polaczone”
static const int WIFI_CONNECTED_BIT = BIT0;

// flaga informujaca czy mamy polaczenie wifi
static volatile bool s_is_wifi_connected = false;


// handler zdarzen systemowych WiFi/IP
// funkcja wywolywana automatycznie, gdy zmieni się stan sieci
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    // 1. start modułu wifi
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    
    // 2. rozlaczenie / brak polaczenia
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_is_wifi_connected = false; 
        ESP_LOGI(TAG_WIFI, "Rozłączono z Wi-Fi. Próba ponownego połączenia...");
        esp_wifi_connect(); 
    } 
    
    // 3. uzyskano adres IP → ustaw flage i powiadom grupe zdarzen
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG_WIFI, "Uzyskano adres IP: " IPSTR, IP2STR(&event->ip_info.ip));

        s_is_wifi_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


void wifi_station_init(void)
{
    // tworzenie grupy zdarzen do sygnalizacji stanu wifi
    s_wifi_event_group = xEventGroupCreate();

    // inicjalizacja stosu sieciowego i systemu zdarzen
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // tworzenie interfejsu WiFi STA
    esp_netif_create_default_wifi_sta();

    // konfiguracja sterownika wifi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // rejestracja handlerow eventow wifi i uzyskania IP
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // konfiguracja sieci wifi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    // ustawienie trybu pracy na STATION i zapis konfiguracji
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // start modulu wifi
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_WIFI, "Inicjalizacja wifi_init_sta zakończona. Oczekiwanie na pierwsze połączenie...");

    // czekanie, az handler ustawi WIFI_CONNECTED_BIT
    xEventGroupWaitBits(s_wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        pdFALSE,
                        pdFALSE,
                        portMAX_DELAY);

    ESP_LOGI(TAG_WIFI, "Połączono z AP: %s", CONFIG_ESP_WIFI_SSID);
}


// funkcja do sprawdzenia, czy jestesmy polaczeni
bool wifi_station_is_connected(void)
{
    return s_is_wifi_connected;
}
