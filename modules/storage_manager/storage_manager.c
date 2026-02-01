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
#include "spi_bus_mutex.h"

static const char *TAG = "STORAGE_MGR";


#define MOUNT_POINT "/sdcard"
#define FILE_PATH   "/sdcard/data.txt"

#define PIN_NUM_MISO  SPI_MISO_PIN
#define PIN_NUM_MOSI  SPI_MOSI_PIN
#define PIN_NUM_CLK   SPI_SCK_PIN
#define PIN_NUM_CS    CS_SD_CARD_PIN

#define SD_INIT_RETRIES 3
#define SD_INIT_RETRY_DELAY_MS 500

static sdmmc_card_t *card;
static bool sd_card_mounted = false;

static size_t storage_get_free_space_unlocked(void);


void storage_init(void)
{
    esp_err_t ret;
    int retry_count = 0;
    
    ESP_LOGI(TAG, "Configuring SD card on existing SPI3_HOST bus...");
    
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI_HOST_USED;
    host.max_freq_khz = 1000;
    
    vTaskDelay(pdMS_TO_TICKS(100));

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = SPI_HOST_USED;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    for (retry_count = 0; retry_count < SD_INIT_RETRIES; retry_count++) {
        ESP_LOGI(TAG, "SD card mount attempt %d/%d", retry_count + 1, SD_INIT_RETRIES);
        
        spi_bus_mutex_lock();
        ret = esp_vfs_fat_sdspi_mount(
            MOUNT_POINT,
            &host,
            &slot_config,
            &mount_config,
            &card
        );

        if (ret == ESP_OK) {
            sd_card_mounted = true;
            ESP_LOGI(TAG, "SD card mounted successfully");
            size_t free_space = storage_get_free_space_unlocked();
            spi_bus_mutex_unlock();
            ESP_LOGI(TAG, "Free space: %zu bytes", free_space);
            if (card != NULL) {
                sdmmc_card_print_info(stdout, card);
            }
            return;
        }

        spi_bus_mutex_unlock();

        ESP_LOGW(TAG, "SD card mount failed (%s), retry in %dms", 
                 esp_err_to_name(ret), SD_INIT_RETRY_DELAY_MS);
        
        vTaskDelay(pdMS_TO_TICKS(SD_INIT_RETRY_DELAY_MS));
    }

    ESP_LOGE(TAG, "Failed to mount SD card after %d attempts", SD_INIT_RETRIES);
    sd_card_mounted = false;
    ESP_LOGW(TAG, "HINT: Please verify SD card is inserted. Continuing without SD card storage.");
}


static size_t storage_get_free_space_unlocked(void)
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

size_t storage_get_free_space(void)
{
    if (!sd_card_mounted) {
        ESP_LOGW(TAG, "SD card not mounted, cannot get free space");
        return 0;
    }

    spi_bus_mutex_lock();
    size_t free_space = storage_get_free_space_unlocked();
    spi_bus_mutex_unlock();

    return free_space;
}


void storage_clear_all(void)
{
    if (!sd_card_mounted) {
        ESP_LOGW(TAG, "SD card not mounted, cannot clear all");
        return;
    }

    spi_bus_mutex_lock();
    struct stat st;
    if (stat(FILE_PATH, &st) == 0) {
        unlink(FILE_PATH);
        ESP_LOGI(TAG, "File deleted");
    } else {
        ESP_LOGW(TAG, "File does not exist");
    }
    spi_bus_mutex_unlock();
}

bool storage_write_line(const char *text)
{
    if (!sd_card_mounted) {
        ESP_LOGW(TAG, "SD card not mounted, cannot write line");
        return false;
    }

    spi_bus_mutex_lock();
    if (storage_get_free_space_unlocked() < 512) {
        ESP_LOGW(TAG, "Not enough space on SD card");
        spi_bus_mutex_unlock();
        return false;
    }

    FILE *f = fopen(FILE_PATH, "a");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file");
        spi_bus_mutex_unlock();
        return false;
    }

    int res = fprintf(f, "%s\n", text);
    fclose(f);
    spi_bus_mutex_unlock();

    return (res >= 0);
}

char *storage_read_all(void)
{
    if (!sd_card_mounted) {
        ESP_LOGW(TAG, "SD card not mounted, cannot read all");
        return NULL;
    }

    spi_bus_mutex_lock();
    FILE *f = fopen(FILE_PATH, "r");
    if (!f) {
        spi_bus_mutex_unlock();
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size <= 0) {
        fclose(f);
        spi_bus_mutex_unlock();
        return NULL;
    }

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        spi_bus_mutex_unlock();
        return NULL;
    }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    spi_bus_mutex_unlock();

    return buf;
}
