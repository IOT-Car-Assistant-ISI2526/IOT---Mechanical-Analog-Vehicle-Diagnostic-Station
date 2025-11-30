#include "nvs_flash.h" // pamiec flash
#include "esp_log.h"   // logowanie wiadomosci

// importy
#include "wifi_station.h"
#include "status_led.h"
#include "http_client.h"

#include "hcsr04.h"

// tag do logowania w konsoli
static const char *TAG = "app_main";

void app_main(void)
{
  //----Test HC-SR04----
  //hcsr04_regular_measurments();
  //--------------------

  esp_err_t ret = nvs_flash_init(); // inicjalizacja NVS
  // sprawdzanie czy NVS jest pelny lub ma niekompatybilna wersje
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  ESP_LOGI(TAG, "Start aplikacji...");

  status_led_start_task(); // watek obslugujacy diode LED
  wifi_station_init();     // watek obslugujacy wi-fi

  ESP_LOGI(TAG, "Pierwsze połączenie Wi-Fi nawiązane. Uruchamiam klienta HTTP...");

  http_client_start_task(); // watek klienta HTTP
}