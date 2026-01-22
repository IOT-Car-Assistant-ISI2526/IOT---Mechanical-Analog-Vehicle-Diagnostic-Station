#include "storage_manager.h"
#include "project_config.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"

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

static sdmmc_card_t *card;


void storage_init(void)
{
    esp_err_t ret;

   sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
};

ret = spi_bus_initialize(SPI_HOST_USED, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus init failed");
        return;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ret = esp_vfs_fat_sdspi_mount(
        MOUNT_POINT,
        &host,
        &slot_config,
        &mount_config,
        &card
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card (%s)", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "SD card mounted");
    ESP_LOGI(TAG, "Free space: %u bytes", storage_get_free_space());
    if (card == NULL)
    {
        ESP_LOGE(TAG, "Card pointer is NULL!");
    }
    else
    {
        sdmmc_card_print_info(stdout, card);
    }

    sdmmc_card_print_info(stdout, card);
}


size_t storage_get_free_space(void)
{
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
