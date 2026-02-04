#include "sd_card.h"

#define MOUNT_POINT "/sdcard"

static const char *TAG = "SD_CARD";
static bool s_card_mounted = false;
static sdmmc_card_t *s_card = NULL;

esp_err_t sd_card_init(spi_host_device_t host_id, int pin_mosi, int pin_miso, int pin_clk, int pin_cs)
{
    if (s_card_mounted)
    {
        ESP_LOGW(TAG, "SD card already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SD card...");

    // Configuration for the SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = pin_mosi,
        .miso_io_num = pin_miso,
        .sclk_io_num = pin_clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    // Initialize the SPI bus if it's not already initialized
    esp_err_t ret = spi_bus_initialize(host_id, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ret;
    }

    // Configuration for SD SPI interface
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = host_id;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; // Increased frequency for better performance

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = pin_cs;
    slot_config.host_id = host_id;

    // Mount filesystem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    ret = esp_vfs_fat_sdspi_mount(SD_CARD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ret;
    }

    s_card_mounted = true;

    // Print SD card info
    sdmmc_card_print_info(stdout, s_card);
    ESP_LOGI(TAG, "SD card mounted successfully at %s", SD_CARD_MOUNT_POINT);

    return ESP_OK;
}

esp_err_t sd_card_deinit(void)
{
    if (!s_card_mounted)
    {
        ESP_LOGW(TAG, "SD card not mounted");
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_CARD_MOUNT_POINT, s_card);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        return ret;
    }

    s_card_mounted = false;
    s_card = NULL;
    ESP_LOGI(TAG, "SD card unmounted");

    return ESP_OK;
}

static esp_err_t sd_card_write(const char *path, char *data)
{
    ESP_LOGI(TAG, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}

static esp_err_t sd_card_read(const char *path)
{
    ESP_LOGI(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char *pos = strchr(line, '\n');
    if (pos)
    {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    return ESP_OK;
}

esp_err_t sd_card_write_file(const char *path, const char *data)
{
    if (!s_card_mounted)
    {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_FAIL;
    }

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s", SD_CARD_MOUNT_POINT, path);

    ESP_LOGI(TAG, "Writing to file: %s", full_path);
    FILE *f = fopen(full_path, "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
        return ESP_FAIL;
    }

    size_t written = fprintf(f, "%s", data);
    fclose(f);

    if (written <= 0)
    {
        ESP_LOGE(TAG, "Failed to write data to file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "File written successfully: %d bytes", written);
    return ESP_OK;
}

esp_err_t sd_card_append_file(const char *path, const char *data)
{
    if (!s_card_mounted)
    {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_FAIL;
    }

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s", SD_CARD_MOUNT_POINT, path);

    ESP_LOGI(TAG, "Appending to file: %s", full_path);
    FILE *f = fopen(full_path, "a");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for appending: %s", path);
        return ESP_FAIL;
    }

    size_t written = fprintf(f, "%s", data);
    fclose(f);

    if (written <= 0)
    {
        ESP_LOGE(TAG, "Failed to append data to file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Data appended successfully: %d bytes", written);
    return ESP_OK;
}

char *sd_card_read_file(const char *path)
{
    if (!s_card_mounted)
    {
        ESP_LOGE(TAG, "SD card not mounted");
        return NULL;
    }

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s", SD_CARD_MOUNT_POINT, path);

    ESP_LOGI(TAG, "Reading file: %s", full_path);
    FILE *f = fopen(full_path, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", path);
        return NULL;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0)
    {
        ESP_LOGW(TAG, "File is empty or error reading size");
        fclose(f);
        return NULL;
    }

    // Allocate buffer
    char *buffer = (char *)malloc(file_size + 1);
    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for file contents");
        fclose(f);
        return NULL;
    }

    // Read file
    size_t read = fread(buffer, 1, file_size, f);
    fclose(f);

    buffer[read] = '\0';
    ESP_LOGI(TAG, "File read successfully: %d bytes", read);

    return buffer;
}

esp_err_t sd_card_delete_file(const char *path)
{
    if (!s_card_mounted)
    {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_FAIL;
    }

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s", SD_CARD_MOUNT_POINT, path);

    ESP_LOGI(TAG, "Deleting file: %s", full_path);
    int ret = unlink(full_path);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to delete file: %s", path);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "File deleted successfully");
    return ESP_OK;
}

bool sd_card_is_mounted(void)
{
    return s_card_mounted;
}

uint64_t sd_card_get_free_space(void)
{
    if (!s_card_mounted || s_card == NULL)
    {
        return 0;
    }

    // Get card size in sectors
    uint64_t card_size = (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;

    // For simplicity, return total size. For accurate free space, use filesystem APIs
    // This is a basic implementation - for real free space use statvfs
    return card_size;
}