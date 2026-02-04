#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include "esp_err.h"

#define SD_CARD_MOUNT_POINT "/sdcard"

esp_err_t sd_card_init(spi_host_device_t host_id, int pin_mosi, int pin_miso, int pin_clk, int pin_cs);

esp_err_t sd_card_deinit(void);

esp_err_t sd_card_write_file(const char *path, const char *data);

esp_err_t sd_card_append_file(const char *path, const char *data);

char *sd_card_read_file(const char *path);

esp_err_t sd_card_delete_file(const char *path);

bool sd_card_is_mounted(void);

uint64_t sd_card_get_free_space(void);
