#include "storage_manager.h"
#include "project_config.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

static const char *TAG = "STORAGE_MGR";

/* Mount point and file */
#define MOUNT_POINT "/sdcard"
#define FILE_PATH   "/sdcard/data.txt"

/* SPI pins – ADJUST TO YOUR WIRING */
#define PIN_NUM_MISO  SPI_MISO_PIN
#define PIN_NUM_MOSI  SPI_MOSI_PIN
#define PIN_NUM_CLK   SPI_SCK_PIN
#define PIN_NUM_CS    CS_SD_CARD_PIN

#define SD_INIT_RETRIES 3
#define SD_INIT_RETRY_DELAY_MS 500

static sdmmc_card_t *card;
static bool sd_card_mounted = false;


void storage_init(void)
{
    esp_err_t ret;
    int retry_count = 0;

    // SPI bus jest już zainicjalizowany w main.c przez spi_initialize_master()
    // Tylko konfigurujemy host dla SD karty
    
    ESP_LOGI(TAG, "Configuring SD card on existing SPI3_HOST bus...");
    
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI_HOST_USED;  // SPI3_HOST
    host.max_freq_khz = 1000;   // Niska prędkość dla stabilności podczas init
    
    // Stabilizacja przed montowaniem SD
    vTaskDelay(pdMS_TO_TICKS(100));

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = SPI_HOST_USED;  // Musi zgadzać się z zainicjalizowanym hostem

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Retry logika - timeout SD init jest intermittentny
    for (retry_count = 0; retry_count < SD_INIT_RETRIES; retry_count++) {
        ESP_LOGI(TAG, "SD card mount attempt %d/%d", retry_count + 1, SD_INIT_RETRIES);
        
        ret = esp_vfs_fat_sdspi_mount(
            MOUNT_POINT,
            &host,
            &slot_config,
            &mount_config,
            &card
        );

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "SD card mounted successfully");
            ESP_LOGI(TAG, "Free space: %u bytes", storage_get_free_space());
            if (card != NULL) {
                sdmmc_card_print_info(stdout, card);
            }
            sd_card_mounted = true;
            return;
        }

        ESP_LOGW(TAG, "SD card mount failed (%s), retry in %dms", 
                 esp_err_to_name(ret), SD_INIT_RETRY_DELAY_MS);
        
        // Delay przed ponowną próbą
        vTaskDelay(pdMS_TO_TICKS(SD_INIT_RETRY_DELAY_MS));
    }

    // Jeśli wszystkie próby nie powiodły się
    ESP_LOGE(TAG, "Failed to mount SD card after %d attempts", SD_INIT_RETRIES);
    sd_card_mounted = false;
    ESP_LOGW(TAG, "HINT: Please verify SD card is inserted. Continuing without SD card storage.");
}


size_t storage_get_free_space(void)
{
    if (!sd_card_mounted) {
        ESP_LOGW(TAG, "SD card not mounted, cannot get free space");
        return 0;
    }

    FATFS *fs;
    DWORD free_clusters;

    FRESULT res = f_getfree("0:", &free_clusters, &fs);
    if (res != FR_OK) {
        ESP_LOGE(TAG, "Failed to get free space (err=%d)", res);
        return 0;
    }

    return (size_t)free_clusters * fs->csize * 512;
}


void storage_clear_all(void)
{
    if (!sd_card_mounted) {
        ESP_LOGW(TAG, "SD card not mounted, cannot clear all");
        return;
    }

    struct stat st;
    if (stat(FILE_PATH, &st) == 0) {
        unlink(FILE_PATH);
        ESP_LOGI(TAG, "File deleted");
    } else {
        ESP_LOGW(TAG, "File does not exist");
    }
}

bool storage_write_line(const char *text)
{
    if (!sd_card_mounted) {
        ESP_LOGW(TAG, "SD card not mounted, cannot write line");
        return false;
    }

    if (storage_get_free_space() < 512) {
        ESP_LOGW(TAG, "Not enough space on SD card");
        return false;
    }

    FILE *f = fopen(FILE_PATH, "a");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file");
        return false;
    }

    int res = fprintf(f, "%s\n", text);
    fclose(f);

    return (res >= 0);
}

char *storage_read_all(void)
{
    if (!sd_card_mounted) {
        ESP_LOGW(TAG, "SD card not mounted, cannot read all");
        return NULL;
    }

    FILE *f = fopen(FILE_PATH, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size <= 0) {
        fclose(f);
        return NULL;
    }

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    return buf;
}
