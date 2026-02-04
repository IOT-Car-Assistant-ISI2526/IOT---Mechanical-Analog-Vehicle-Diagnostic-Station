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
#define FILE_PATH "/spiffs/data.txt"
#define PARTITION_NAME "storage"

void storage_init(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS...");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = PARTITION_NAME,
        .max_files = 5,
        .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format SPIFFS");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "SPIFFS partition not found");
        }
        else
        {
            ESP_LOGE(TAG, "SPIFFS init failed (%s)", esp_err_to_name(ret));
        }
    }
    else
    {
        ESP_LOGI(TAG, "SPIFFS mounted successfully.");
    }
}

size_t storage_get_free_space(void)
{
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(PARTITION_NAME, &total, &used);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS info");
        return 0;
    }
    return total - used;
}

void storage_clear_all(void)
{
    struct stat st;
    if (stat(FILE_PATH, &st) == 0)
    {
        unlink(FILE_PATH);
        ESP_LOGI(TAG, "Storage file deleted.");
    }
    else
    {
        ESP_LOGW(TAG, "Storage file does not exist.");
    }
}

bool storage_write_line(const char *text)
{
    size_t free_space = storage_get_free_space();

    // Safety check: 512 bytes
    if (free_space < 512)
    {
        ESP_LOGW(TAG, "OUT OF SPACE! Write cancelled.");
        return false;
    }

    FILE *f = fopen(FILE_PATH, "a");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open storage file for writing");
        return false;
    }

    int result = fprintf(f, "%s\n", text);
    fclose(f);

    return (result >= 0);
}

char *storage_read_all(void)
{
    FILE *f = fopen(FILE_PATH, "r");
    if (f == NULL)
    {
        ESP_LOGW(TAG, "Storage file not found for reading");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0)
    {
        fclose(f);
        return NULL;
    }

    char *buffer = (char *)malloc(file_size + 1);
    if (buffer == NULL)
    {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, file_size, f);
    buffer[read_bytes] = '\0';
    fclose(f);

    return buffer;
}
