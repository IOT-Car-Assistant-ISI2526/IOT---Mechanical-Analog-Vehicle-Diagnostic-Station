#include "http_client.h"
#include "project_config.h" 
#include "wifi_station.h"   

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h" 


static const char *TAG_HTTP = "http_client";

static const char *REQUEST = "GET " CONFIG_WEB_PATH " HTTP/1.1\r\n"
    "Host: " CONFIG_WEB_SERVER "\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "Connection: close\r\n"
    "\r\n";


/* Zadanie zostało przeniesione tutaj i jest statyczne (prywatne) */
static void http_get_task(void* arg)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[256];

    while(1) {
        // Używamy funkcji publicznej z modułu wifi_station
        if (!wifi_station_is_connected()) {
            ESP_LOGW(TAG_HTTP, "Oczekiwanie na połączenie Wi-Fi...");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        ESP_LOGI(TAG_HTTP, "Rozpoczynam pobieranie danych z %s...", CONFIG_WEB_SERVER);

        // --- 1. Rozwiązywanie nazwy DNS ---
        int err = getaddrinfo(CONFIG_WEB_SERVER, CONFIG_WEB_PORT, &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(TAG_HTTP, "Błąd DNS. Błąd: %d", err);
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG_HTTP, "Znaleziono adres IP: %s", inet_ntoa(*addr));

        // --- 2. Utworzenie gniazda (socketu) ---
        s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ESP_LOGE(TAG_HTTP, "Nie można utworzyć gniazda. Błąd: %d", errno);
            freeaddrinfo(res);
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }
        ESP_LOGI(TAG_HTTP, "Gniazdo utworzone.");

        // --- 3. Połączenie z serwerem (Connect) ---
        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG_HTTP, "Nie można połączyć się z serwerem. Błąd: %d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }
        ESP_LOGI(TAG_HTTP, "Połączono z serwerem.");
        freeaddrinfo(res);

        // --- 4. Wysłanie zapytania HTTP GET ---
        if (send(s, REQUEST, strlen(REQUEST), 0) < 0) {
            ESP_LOGE(TAG_HTTP, "Błąd podczas wysyłania zapytania. Błąd: %d", errno);
            close(s);
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }
        ESP_LOGI(TAG_HTTP, "Zapytanie HTTP GET wysłane.");

        // --- 5. Odbieranie odpowiedzi ---
        ESP_LOGI(TAG_HTTP, "Odbieranie odpowiedzi...");
        printf("\n--- POCZĄTEK ODPOWIEDZI HTTP ---\n");
        
        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG_HTTP, "Błąd przy ustawianiu timeoutu gniazda: %d", errno);
            close(s);
            continue;
        }

        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = recv(s, recv_buf, sizeof(recv_buf)-1, 0);
            if (r > 0) {
                printf("%.*s", r, recv_buf);
            }
        } while(r > 0);

        printf("\n--- KONIEC ODPOWIEDZI HTTP ---\n");

        if (r < 0 && errno != EAGAIN) {
            ESP_LOGE(TAG_HTTP, "Błąd podczas odbierania danych. Błąd: %d", errno);
        } else {
            ESP_LOGI(TAG_HTTP, "Odpowiedź odebrana pomyślnie.");
        }

        // --- 6. Zamknięcie gniazda ---
        close(s);
        ESP_LOGI(TAG_HTTP, "Gniazdo zamknięte.");

        ESP_LOGI(TAG_HTTP, "Następne zapytanie za 30 sekund...");
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void http_client_start_task(void)
{
    xTaskCreate(http_get_task, "http_get_task", 4096, NULL, 5, NULL);
}
