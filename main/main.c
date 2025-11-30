#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

// --- DOŁĄCZAMY NASZE MODUŁY ---
#include "storage_manager.h" // Obsługa plików (SPIFFS)
#include "ble_server.h"      // Bluetooth (Konfiguracja)
#include "wifi_station.h"    // WiFi (Łączenie)

// --- Funkcja pomocnicza: Pobieranie linii z konsoli ---
void get_line_from_console(char* buffer, size_t max_len) {
    size_t index = 0;
    int c;
    while (1) {
        c = fgetc(stdin);
        // Jeśli brak znaku, czekamy chwilę (zapobiega obciążeniu CPU na 100%)
        if (c == EOF) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }
        // Zatwierdzenie Enterem
        if (c == '\n' || c == '\r') {
            if (index > 0) {
                buffer[index] = 0; // Null-terminator
                printf("\n");      // Nowa linia wizualnie
                return;
            }
        } else {
            // Zwykły znak - dodaj do bufora
            if (index < max_len - 1) {
                buffer[index++] = (char)c;
                printf("%c", c);   // Echo (pokaż co piszę)
            }
        }
    }
}

// --- MAIN ---
void app_main(void) {
    // 1. Inicjalizacja NVS (Kluczowe dla WiFi i BLE!)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    // 2. Start Modułów
    printf("\n--- START SYSTEMU ---\n");
    
    storage_init();       // Montowanie plików
    ble_server_init();    // Uruchomienie Bluetooth (możesz już łączyć się telefonem)
    wifi_station_init();

    printf("\n===================================\n");
    printf(" SYSTEM GOTOWY.\n");
    printf(" * Bluetooth: 'ESP32_CONFIG' (użyj nRF Connect)\n");
    printf(" * Konsola: Pisz tekst aby zapisać notatkę.\n");
    printf(" * Komendy: 'read', 'free', 'clear'\n");
    printf("===================================\n");

    // Czyszczenie bufora wejściowego (usuwa śmieci po restarcie)
    int c;
    while ((c = fgetc(stdin)) != EOF) { vTaskDelay(10 / portTICK_PERIOD_MS); }

    char input_line[128];

    // 3. Pętla Główna (Obsługa konsoli szeregowej)
    while (1) {
        get_line_from_console(input_line, sizeof(input_line));

        // --- Komenda: READ ---
        if (strcmp(input_line, "read") == 0) {
            char* content = storage_read_all();
            if (content) {
                printf("\n--- TWOJE NOTATKI ---\n%s\n---------------------\n", content);
                free(content); // WAŻNE: Zwalniamy pamięć!
            } else {
                printf(">> Brak notatek.\n");
            }
        } 
        // --- Komenda: FREE ---
        else if (strcmp(input_line, "free") == 0) {
            printf(">> Wolne miejsce: %zu bajtów\n", storage_get_free_space());
        }
        // --- Komenda: CLEAR ---
        else if (strcmp(input_line, "clear") == 0) {
            storage_clear_all();
            printf(">> Notatki usunięte.\n");
        } 
        // --- Zapisz wszystko inne ---
        else {
            if (storage_write_line(input_line)) {
                printf(">> Zapisano notatkę (Konsola).\n");
            } else {
                printf(">> Błąd zapisu!\n");
            }
        }
    }
}