#include "nvs_flash.h" // pamiec flash
#include "esp_log.h"   // logowanie wiadomosci

// importy
#include "wifi_station.h"
#include "status_led.h"
#include "http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#include "hcsr04.h"
#include "storage_manager.h"

// tag do logowania w konsoli
static const char *TAG = "app_main";


// Funkcja pomocnicza do konsoli (zostaje tutaj, bo to UI)
void get_line_from_console(char* buffer, size_t max_len) {
    size_t index = 0;
    int c;
    while (1) {
        c = fgetc(stdin);
        if (c == EOF) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }
        if (c == '\n' || c == '\r') {
            if (index > 0) {
                buffer[index] = 0;
                printf("\n");
                return;
            }
        } else {
            if (index < max_len - 1) {
                buffer[index++] = (char)c;
                printf("%c", c);
            }
        }
    }
}

void app_main(void) {
    // 1. Inicjalizacja NVS (Systemowa)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    // 2. Inicjalizacja naszego modułu STORAGE
    storage_init();
    
    // --- CZYSZCZENIE BUFORA WEJŚCIOWEGO NA STARCIE ---
    int c;
    while ((c = fgetc(stdin)) != EOF) {
        // Po prostu ignorujemy znaki, które przyszły przed startem pętli
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }

    printf("\n\n===================================\n");
    printf(" SYSTEM GOTOWY (Modularny)\n");
    printf(" 1. [tekst] -> Zapisz\n");
    printf(" 2. 'read'  -> Odczytaj\n");
    printf(" 3. 'free'  -> Info o pamięci\n");
    printf(" 4. 'clear' -> Wyczyść\n");
    printf("===================================\n");

    char input_line[128];

    while (1) {
        get_line_from_console(input_line, sizeof(input_line));

        if (strcmp(input_line, "read") == 0) {
            // Używamy funkcji z modułu
            char* content = storage_read_all();
            if (content) {
                printf("\n--- NOTATKI ---\n%s\n---------------\n", content);
                free(content);
            } else {
                printf(">> Pusto.\n");
            }
        } 
        else if (strcmp(input_line, "free") == 0) {
            printf(">> Wolne: %zu bajtów\n", storage_get_free_space());
        }
        else if (strcmp(input_line, "clear") == 0) {
            storage_clear_all();
            printf(">> Wyczyszczono.\n");
        } 
        else {
            if (storage_write_line(input_line)) {
                printf(">> Zapisano.\n");
            } else {
                printf(">> Błąd zapisu/brak miejsca!\n");
            }
        }
    }
}