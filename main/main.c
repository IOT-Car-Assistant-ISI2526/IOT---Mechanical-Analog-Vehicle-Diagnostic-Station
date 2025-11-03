#include "nvs_flash.h"
#include "esp_log.h"

#include "wifi_station.h"
#include "status_led.h"
#include "http_client.h"

static const char *TAG = "app_main";

void app_main(void)
{
    // Inicjalizacja NVS (niezbędna dla Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Start aplikacji...");

    // Uruchom zadanie obsługujące diodę LED
    // Od razu zacznie mrugać w trybie "brak połączenia"
    status_led_start_task();

    // Uruchom inicjalizację Wi-Fi
    // Ta funkcja zablokuje app_main do czasu uzyskania pierwszego połączenia
    wifi_station_init();

    // Ten kod wykona się dopiero po pierwszym udanym połączeniu Wi-Fi.
    ESP_LOGI(TAG, "Pierwsze połączenie Wi-Fi nawiązane. Uruchamiam klienta HTTP...");
    
    // Uruchom zadanie klienta HTTP w tle.
    http_client_start_task();
    
    // app_main może teraz zakończyć działanie.
    ESP_LOGI(TAG, "app_main zakończone. Zadania działają w tle.");
}