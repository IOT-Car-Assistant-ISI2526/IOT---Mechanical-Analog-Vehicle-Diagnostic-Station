#include "storage_manager.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

// --- Konfiguracja prywatna modułu ---
static const char *TAG = "STORAGE_MGR";
#define FILE_PATH "/spiffs/notatki.txt"
#define PARTITION_NAME "storage"

void storage_init(void) {
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = PARTITION_NAME,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Błąd montowania SPIFFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Nie znaleziono partycji SPIFFS");
        } else {
            ESP_LOGE(TAG, "Błąd inicjalizacji (%s)", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGI(TAG, "System plików zamontowany.");
    }
}

size_t storage_get_free_space(void) {
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(PARTITION_NAME, &total, &used);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Błąd odczytu info partycji");
        return 0;
    }
    return total - used;
}

void storage_clear_all(void) {
    struct stat st;
    if (stat(FILE_PATH, &st) == 0) {
        unlink(FILE_PATH);
        ESP_LOGI(TAG, "Plik usunięty.");
    } else {
        ESP_LOGW(TAG, "Plik nie istnieje.");
    }
}

bool storage_write_line(const char* text) {
    size_t free_space = storage_get_free_space();
    
    // Zabezpieczenie: 512 bajtów
    if (free_space < 512) {
        ESP_LOGW(TAG, "BRAK MIEJSCA! Anulowano zapis.");
        return false;
    }

    FILE* f = fopen(FILE_PATH, "a");
    if (f == NULL) return false;
    
    int result = fprintf(f, "%s\n", text);
    fclose(f);

    return (result >= 0);
}

char* storage_read_all(void) {
    FILE* f = fopen(FILE_PATH, "r");
    if (f == NULL) return NULL;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        return NULL;
    }

    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL) {
        fclose(f);
        return NULL;
    }

    fread(buffer, 1, file_size, f);
    buffer[file_size] = '\0';
    fclose(f);
    
    return buffer;
}