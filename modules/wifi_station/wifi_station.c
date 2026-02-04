#include "wifi_station.h"
#include <string.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

volatile bool g_wifi_reconnect_request = false;

static const char *TAG_WIFI = "wifi_station";

static volatile bool s_is_wifi_connected = false;
static esp_netif_t *s_sta_netif = NULL;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        s_is_wifi_connected = false;
        ESP_LOGW(TAG_WIFI, "Brak połączenia. Ponawiam próbę...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_WIFI, "Uzyskano adres IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_is_wifi_connected = true;
    }
}

bool read_credentials_from_nvs(char *ssid, size_t ssid_size, char *pass, size_t pass_size)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    if (ssid && ssid_size > 0)
        ssid[0] = '\0';
    if (pass && pass_size > 0)
        pass[0] = '\0';

    if (nvs_open("storage", NVS_READONLY, &my_handle) != ESP_OK)
    {
        ESP_LOGW(TAG_WIFI, "Nie można otworzyć NVS");
        return false;
    }

    size_t len = ssid_size;
    err = nvs_get_str(my_handle, "wifi_ssid", ssid, &ssid_size);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG_WIFI, "Nie można odczytać SSID z NVS");
        ssid[0] = '\0';
    }

    len = pass_size;
    err = nvs_get_str(my_handle, "wifi_pass", pass, &pass_size);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG_WIFI, "Nie można odczytać hasła z NVS");
        pass[0] = '\0';
    }

    nvs_close(my_handle);

    while (strlen(ssid) && (ssid[strlen(ssid) - 1] == '\n' || ssid[strlen(ssid) - 1] == '\r'))
        ssid[strlen(ssid) - 1] = '\0';

    while (strlen(pass) && (pass[strlen(pass) - 1] == '\n' || pass[strlen(pass) - 1] == '\r'))
        pass[strlen(pass) - 1] = '\0';

    return (strlen(ssid) > 0);
}

void wifi_station_init(void)
{
    nvs_handle_t my_handle;
    char ssid[33] = {0};
    char pass[64] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(pass);

    esp_netif_init();
    esp_event_loop_create_default();
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

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

    if (read_credentials_from_nvs(ssid, sizeof(ssid), pass, sizeof(pass)))
    {
        wifi_config_t wifi_config = {
            .sta = {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
        };
        strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
    else
    {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    }
}

void wifi_station_deinit(void)
{
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    esp_wifi_stop();
    esp_wifi_deinit();
    if (s_sta_netif)
    {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
    }
    s_is_wifi_connected = false;
}

bool wifi_check_credentials(void)
{
    nvs_handle_t my_handle;
    char ssid[33] = {0};
    size_t len = sizeof(ssid);

    if (nvs_open("storage", NVS_READONLY, &my_handle) == ESP_OK)
    {
        nvs_get_str(my_handle, "wifi_ssid", ssid, &len);
        nvs_close(my_handle);
    }

    if (strlen(ssid) == 0)
    {
        ESP_LOGW(TAG_WIFI, "Brak konfiguracji WiFi w NVS!");
        return false;
    }
    else
    {
        ESP_LOGI(TAG_WIFI, "Konfiguracja WiFi dostępna: SSID=%s", ssid);
        return true;
    }
}

bool wifi_station_is_connected(void)
{
    return s_is_wifi_connected;
}