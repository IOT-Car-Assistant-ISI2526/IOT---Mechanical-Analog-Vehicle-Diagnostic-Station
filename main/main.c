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
#include "bmp280.h"
#include "veml7700.h"
#include "max6675.h"
#include "adxl345.h"
#include "storage_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"        // Required for i2c_config_t, I2C_MODE_MASTER, i2c_driver_install
#include "driver/gpio.h"       // Required for GPIO_PULLUP_ENABLE
#include "driver/spi_master.h" // <--- Add this include at the top

// i2c bmp280 + adxl345
#define I2C_MASTER_NUM I2C_NUM_0
#define PORT_0_SDA_PIN 21
#define PORT_0_SCL_PIN 22

// i2c veml7700
#define I2C_MASTER_NUM I2C_NUM_1
#define PORT_1_SDA_PIN 25
#define PORT_1_SCL_PIN 26

// spi
#define PIN_CLK 14
#define PIN_SO 12 // MISO
// MOSI is not needed for MAX6675, but SPI driver might need a pin number or -1
#define PIN_MOSI -1

// stałe do zastąpienia funkcjami i zmiennymi
#define BLE_PAIRED_SUCCESS true
#define MEMORY_USAGE_PERCENT 75
#define WIFI_STATION_CHECK_CREDETIALS true
#define WIFI_CONNECTED_SUCCESS true

// tag do logowania w konsoli
static const char *TAG = "app_main";

void init_i2c_global(void)
{
  i2c_config_t conf0 = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = PORT_0_SDA_PIN,
      .scl_io_num = PORT_0_SCL_PIN,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = 100000,
  };
  // Install the driver ONCE.
  // If this errors, nothing else will work.
  ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf0));
  ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf0.mode, 0, 0, 0));
  printf(">>> I2C Driver Installed Globally <<<\n");

  i2c_config_t conf1 = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = PORT_1_SDA_PIN,
      .scl_io_num = PORT_1_SCL_PIN,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = 100000,
  };
  // Install the driver ONCE.
  // If this errors, nothing else will work.
  ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_1, &conf1));
  ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_1, conf1.mode, 0, 0, 0));
  printf(">>> I2C Driver Installed Globally <<<\n");
}

void init_spi_global(void)
{
  printf("Initializing SPI Bus...\n");

  spi_bus_config_t buscfg = {
      .miso_io_num = PIN_SO,
      .mosi_io_num = PIN_MOSI,
      .sclk_io_num = PIN_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 0, // Default
  };

  // Initialize SPI2_HOST (FSPI)
  // SPI_DMA_CH_AUTO lets ESP32 pick the best DMA settings
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

  printf(">>> SPI Bus Initialized <<<\n");
}

void get_line_from_console(char *buffer, size_t max_len)
{
  size_t index = 0;
  int c;
  while (1)
  {
    c = fgetc(stdin);
    if (c == EOF)
    {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }
    if (c == '\n' || c == '\r')
    {
      if (index > 0)
      {
        buffer[index] = 0;
        printf("\n");
        return;
      }
    }
    else
    {
      if (index < max_len - 1)
      {
        buffer[index++] = (char)c;
        printf("%c", c);
      }
    }
  }
}

void app_main(void)
{
  init_i2c_global();
  init_spi_global();
  vTaskDelay(pdMS_TO_TICKS(500));

  double bmp280_parameter = 0.0;
  double veml7700_parameters = 0.0;
  double max6675_parameters = 0.0;
  double adxl345_parameters = 0.0;
  double hcsr04_parameters = 0.0;

  bmp280_start_task(&bmp280_parameter);
  veml7700_start_task(&veml7700_parameters);
  max6675_start_task(&max6675_parameters);
  adxl345_start_task(&adxl345_parameters);
  hcsr04_start_task(&hcsr04_parameters);

  // 1. Inicjalizacja NVS (Systemowa)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }

  // 2. Inicjalizacja naszego modułu STORAGE
  storage_init();

  // --- CZYSZCZENIE BUFORA WEJŚCIOWEGO NA STARCIE ---
  int c;
  while ((c = fgetc(stdin)) != EOF)
  {
    // Po prostu ignorujemy znaki, które przyszły przed startem pętli
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  ESP_ERROR_CHECK(ret);
  ESP_LOGI(TAG, "Start aplikacji...");
  char input_line[128];

  while (1)
  {
    get_line_from_console(input_line, sizeof(input_line));

    if (strcmp(input_line, "read") == 0)
    {
      // Używamy funkcji z modułu
      char *content = storage_read_all();
      if (content)
      {
        printf("\n--- NOTATKI ---\n%s\n---------------\n", content);
        free(content);
      }
      else
      {
        printf(">> Pusto.\n");
      }
    }
    else if (strcmp(input_line, "free") == 0)
    {
      printf(">> Wolne: %zu bajtów\n", storage_get_free_space());
    }
    else if (strcmp(input_line, "clear") == 0)
    {
      storage_clear_all();
      printf(">> Wyczyszczono.\n");
    }
    else if (strcmp(input_line, "measurement") == 0)
    {

      bool valid = true;

      if (valid)
      {
        char line[128];
        snprintf(line, sizeof(line), "BMP280 LM: %.2f C\nVEML7700 LM: %.2f Lux\nMAX6675 LM: %.2f C\nHC-SR04 LM: %.2f cm\nADXL345 LM: %.2f m/s2\n",
                 bmp280_parameter, veml7700_parameters, max6675_parameters, hcsr04_parameters, adxl345_parameters);
        printf("BMP280 LM: %.2f C \n\nVEML7700 LM: %.2f Lux\nMAX6675 LM: %.2f C\nHC-SR04 LM: %.2f cm\nADXL345 LM: %.2f m/s2\n",
               bmp280_parameter, veml7700_parameters, max6675_parameters, hcsr04_parameters, adxl345_parameters);
        if (storage_write_line(line))
        {
          printf(">> Zapisano.\n");
        }
      }
      else
      {
        printf("Brak poprawnego pomiaru (czujnik jeszcze nie wykonał odczytu)\n");
      }
    }
    else
    {
      if (storage_write_line(input_line))
      {
        printf(">> Zapisano.\n");
      }
      else
      {
        printf(">> Błąd zapisu/brak miejsca!\n");
      }
    }
  }
}