// #include "storage_manager.h"
// #include "project_config.h"
// #include <stdio.h>
// #include <string.h>
// #include <sys/stat.h>
// #include <unistd.h>

// #include "esp_err.h"
// #include "esp_log.h"
// #include "esp_vfs_fat.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

// #include "driver/gpio.h"
// #include "driver/spi_master.h"
// #include "driver/sdspi_host.h"
// #include "sdmmc_cmd.h"
// #include "spi_bus_mutex.h"

// static const char *TAG = "STORAGE_MGR";


// #define MOUNT_POINT "/sdcard"
// #define FILE_PATH   "/sdcard/data.txt"

// #define PIN_NUM_MISO  SPI_MISO_PIN
// #define PIN_NUM_MOSI  SPI_MOSI_PIN
// #define PIN_NUM_CLK   SPI_SCK_PIN
// #define PIN_NUM_CS    CS_SD_CARD_PIN

// #define SD_INIT_RETRIES 3
// #define SD_INIT_RETRY_DELAY_MS 500

// static sdmmc_card_t *card;
// static bool sd_card_mounted = false;

// static size_t storage_get_free_space_unlocked(void);


// void storage_init(void)
// {
//     esp_err_t ret;
//     int retry_count = 0;
    
//     ESP_LOGI(TAG, "Configuring SD card on existing SPI3_HOST bus...");
    
//     sdmmc_host_t host = SDSPI_HOST_DEFAULT();
//     host.slot = SPI_HOST_USED;
//     host.max_freq_khz = 1000;

//     gpio_set_pull_mode(PIN_NUM_MISO, GPIO_PULLUP_ONLY);
//     gpio_set_pull_mode(PIN_NUM_CS, GPIO_PULLUP_ONLY);
//     gpio_set_direction(PIN_NUM_CS, GPIO_MODE_OUTPUT);
//     gpio_set_level(PIN_NUM_CS, 1);
    
//     vTaskDelay(pdMS_TO_TICKS(100));

//     sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
//     slot_config.gpio_cs = PIN_NUM_CS;
//     slot_config.host_id = SPI_HOST_USED;

//     esp_vfs_fat_sdmmc_mount_config_t mount_config = {
//         .format_if_mount_failed = false,
//         .max_files = 5,
//         .allocation_unit_size = 16 * 1024
//     };

//     for (retry_count = 0; retry_count < SD_INIT_RETRIES; retry_count++) {
//         ESP_LOGI(TAG, "SD card mount attempt %d/%d", retry_count + 1, SD_INIT_RETRIES);
        
//         spi_bus_mutex_lock();
//         ret = esp_vfs_fat_sdspi_mount(
//             MOUNT_POINT,
//             &host,
//             &slot_config,
//             &mount_config,
//             &card
//         );

//         if (ret == ESP_OK) {
//             sd_card_mounted = true;
//             ESP_LOGI(TAG, "SD card mounted successfully");
//             size_t free_space = storage_get_free_space_unlocked();
//             spi_bus_mutex_unlock();
//             ESP_LOGI(TAG, "Free space: %zu bytes", free_space);
//             if (card != NULL) {
//                 sdmmc_card_print_info(stdout, card);
//             }
//             return;
//         }

//         spi_bus_mutex_unlock();

//         ESP_LOGW(TAG, "SD card mount failed (%s), retry in %dms", 
//                  esp_err_to_name(ret), SD_INIT_RETRY_DELAY_MS);
        
//         vTaskDelay(pdMS_TO_TICKS(SD_INIT_RETRY_DELAY_MS));
//     }

//     ESP_LOGE(TAG, "Failed to mount SD card after %d attempts", SD_INIT_RETRIES);
//     sd_card_mounted = false;
//     ESP_LOGW(TAG, "HINT: Please verify SD card is inserted. Continuing without SD card storage.");
// }


// static size_t storage_get_free_space_unlocked(void)
// {
//     if (!sd_card_mounted) {
//         ESP_LOGW(TAG, "SD card not mounted, cannot get free space");
//         return 0;
//     }

//     FATFS *fs;
//     DWORD free_clusters;

//     FRESULT res = f_getfree("0:", &free_clusters, &fs);
//     if (res != FR_OK) {
//         ESP_LOGE(TAG, "Failed to get free space (err=%d)", res);
//         return 0;
//     }

//     return (size_t)free_clusters * fs->csize * 512;
// }

// size_t storage_get_free_space(void)
// {
//     if (!sd_card_mounted) {
//         ESP_LOGW(TAG, "SD card not mounted, cannot get free space");
//         return 0;
//     }

//     spi_bus_mutex_lock();
//     size_t free_space = storage_get_free_space_unlocked();
//     spi_bus_mutex_unlock();

//     return free_space;
// }


// void storage_clear_all(void)
// {
//     if (!sd_card_mounted) {
//         ESP_LOGW(TAG, "SD card not mounted, cannot clear all");
//         return;
//     }

//     ESP_LOGI(TAG, "[CLEAR] Task %s requesting SPI mutex to clear SD", pcTaskGetName(NULL));
//     spi_bus_mutex_lock();
//     ESP_LOGI(TAG, "[CLEAR] Task %s acquired SPI mutex, deleting file", pcTaskGetName(NULL));
    
//     struct stat st;
//     if (stat(FILE_PATH, &st) == 0) {
//         unlink(FILE_PATH);
//         ESP_LOGI(TAG, "File deleted");
//     } else {
//         ESP_LOGW(TAG, "File does not exist");
//     }
    
//     ESP_LOGI(TAG, "[CLEAR] Task %s releasing SPI mutex after clear", pcTaskGetName(NULL));
//     spi_bus_mutex_unlock();
// }

// bool storage_write_line(const char *text)
// {
//     if (!sd_card_mounted) {
//         ESP_LOGW(TAG, "SD card not mounted, cannot write line");
//         return false;
//     }

//     ESP_LOGI(TAG, "[WRITE] Task %s requesting SPI mutex to write to SD", pcTaskGetName(NULL));
//     spi_bus_mutex_lock();
//     ESP_LOGI(TAG, "[WRITE] Task %s acquired SPI mutex, writing: %.20s...", pcTaskGetName(NULL), text);
    
//     if (storage_get_free_space_unlocked() < 512) {
//         ESP_LOGW(TAG, "Not enough space on SD card");
//         spi_bus_mutex_unlock();
//         return false;
//     }

//     FILE *f = fopen(FILE_PATH, "a");
//     if (!f) {
//         ESP_LOGE(TAG, "Failed to open file");
//         spi_bus_mutex_unlock();
//         return false;
//     }

//     int res = fprintf(f, "%s\n", text);
//     fclose(f);
    
//     ESP_LOGI(TAG, "[WRITE] Task %s releasing SPI mutex after write", pcTaskGetName(NULL));
//     spi_bus_mutex_unlock();

//     return (res >= 0);
// }

// char *storage_read_all(void)
// {
//     if (!sd_card_mounted) {
//         ESP_LOGW(TAG, "SD card not mounted, cannot read all");
//         return NULL;
//     }

//     ESP_LOGI(TAG, "[READ] Task %s requesting SPI mutex to read from SD", pcTaskGetName(NULL));
//     spi_bus_mutex_lock();
//     ESP_LOGI(TAG, "[READ] Task %s acquired SPI mutex, reading file", pcTaskGetName(NULL));
    
//     FILE *f = fopen(FILE_PATH, "r");
//     if (!f) {
//         ESP_LOGI(TAG, "[READ] Task %s releasing SPI mutex (file not found)", pcTaskGetName(NULL));
//         spi_bus_mutex_unlock();
//         return NULL;
//     }

//     fseek(f, 0, SEEK_END);
//     long size = ftell(f);
//     rewind(f);

//     if (size <= 0) {
//         fclose(f);
//         ESP_LOGI(TAG, "[READ] Task %s releasing SPI mutex (empty file)", pcTaskGetName(NULL));
//         spi_bus_mutex_unlock();
//         return NULL;
//     }

//     char *buf = malloc(size + 1);
//     if (!buf) {
//         fclose(f);
//         ESP_LOGI(TAG, "[READ] Task %s releasing SPI mutex (malloc failed)", pcTaskGetName(NULL));
//         spi_bus_mutex_unlock();
//         return NULL;
//     }

//     fread(buf, 1, size, f);
//     buf[size] = '\0';
//     fclose(f);
    
//     ESP_LOGI(TAG, "[READ] Task %s releasing SPI mutex after reading %ld bytes", pcTaskGetName(NULL), size);
//     spi_bus_mutex_unlock();

//     return buf;
// }



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