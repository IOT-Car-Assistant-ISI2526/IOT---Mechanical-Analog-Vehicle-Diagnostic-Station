#include "project_config.h"

#include <stdio.h>
#include <string.h>
#include "storage_manager.h"
#include "button.h"
#include "ble_server.h"      // Bluetooth (Konfiguracja)

#include "esp_log.h"   // logowanie wiadomosci
#include "nvs_flash.h" // pamiec flash

#include "driver/i2c.h"        // Required for i2c_config_t, I2C_MODE_MASTER, i2c_driver_install
#include "driver/gpio.h"       // Required for GPIO_PULLUP_ENABLE
#include "driver/spi_master.h" // <--- Add this include at the top

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "storage_manager.h"
#include "bmp280.h"
#include "veml7700.h"
#include "max6675.h"
#include "adxl345.h"
#include "hcsr04.h"

#include "wifi_station.h"
#include "status_led.h"
#include "http_client.h"
#include "mqtt_client_app.h"

#include "buzzer.h"

// stałe do zastąpienia funkcjami i zmiennymi
#define BLE_PAIRED_SUCCESS true
#define MEMORY_USAGE_PERCENT 75

// Console tag
static const char *TAG = "app_main";

typedef struct
{
  float temperature;
  float illuminance;
  float engine_temp;
  float distance;
  float acceleration;
} sensor_data_t;

static esp_err_t init_i2c_bus(i2c_port_t port, int sda, int scl, uint32_t freq)
{
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = sda,
      .scl_io_num = scl,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = freq,
  };

  ESP_ERROR_CHECK(i2c_param_config(port, &conf));
  return i2c_driver_install(port, conf.mode, 0, 0, 0);
}

static void init_i2c(void)
{
  init_i2c_bus(I2C_NUM_0, PORT_0_SDA_PIN, PORT_0_SCL_PIN, 100000);
  init_i2c_bus(I2C_NUM_1, PORT_1_SDA_PIN, PORT_1_SCL_PIN, 100000);
  ESP_LOGI(TAG, "I2C initialized.");
}

static void init_spi(void)
{
  spi_bus_config_t buscfg = {
      .miso_io_num = PIN_SO,
      .mosi_io_num = PIN_MOSI,
      .sclk_io_num = PIN_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1};

  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
  ESP_LOGI(TAG, "SPI initialized.");
}

static void init_nvs(void)
{
  esp_err_t ret = nvs_flash_init();

  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }

  ESP_ERROR_CHECK(ret);
}

static sensor_data_t collect_sensor_data(
    float bmp, float lux, float eng, float dist, float accel)
{
  sensor_data_t data = {
      .temperature = bmp,
      .illuminance = lux,
      .engine_temp = eng,
      .distance = dist,
      .acceleration = accel};
  return data;
}

static void print_sensor_data(const sensor_data_t *data)
{
  printf("BMP280:    %.2f C\n", data->temperature);
  printf("VEML7700:  %.2f Lux\n", data->illuminance);
  printf("MAX6675:   %.2f C\n", data->engine_temp);
  printf("HC-SR04:   %.2f cm\n", data->distance);
  printf("ADXL345:   %.2f m/s2\n", data->acceleration);
}

static void format_sensor_line(char *out, size_t len, const sensor_data_t *d)
{
  snprintf(out, len,
           "BMP280: %.2f C\n"
           "VEML7700: %.2f Lux\n"
           "MAX6675: %.2f C\n"
           "HC-SR04: %.2f cm\n"
           "ADXL345: %.2f m/s2\n",
           d->temperature, d->illuminance, d->engine_temp, d->distance, d->acceleration);
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
  init_i2c();
  init_spi();
  init_nvs();

  storage_init();
  vTaskDelay(pdMS_TO_TICKS(STARTUP_DELAY_MS));

  float bmp280_temp = 0.0f, veml7700_illuminance = 0.0f, max6675_engine_temp = 0.0f, adxl345_acceleration = 0.0f, hcsr04_distance = 0.0f;

  bmp280_start_task(&bmp280_temp);
  // veml7700_start_task(&veml7700_illuminance);
  max6675_start_task(&max6675_engine_temp);
  adxl345_start_task(&adxl345_acceleration);
  hcsr04_start_task(&hcsr04_distance);


  mqtt_client_start();
  
  buzzer_init(GPIO_NUM_18); 
  buzzer_beep(500);
  button_init();

  int c;
  while ((c = fgetc(stdin)) != EOF)
  {
    // Ignore input during sensor startup
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

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

  
       char line[128];
       snprintf(line, sizeof(line), "BMP280 LM: %.2f C\nVEML7700 LM: %.2f Lux\nMAX6675 LM: %.2f C\nHC-SR04 LM: %.2f cm\nADXL345 LM: %.2f m/s2\n",
                bmp280_temp, veml7700_illuminance, max6675_engine_temp, hcsr04_distance, adxl345_acceleration);
       printf("BMP280 LM: %.2f C \nVEML7700 LM: %.2f Lux\nMAX6675 LM: %.2f C\nHC-SR04 LM: %.2f cm\nADXL345 LM: %.2f m/s2\n",
              bmp280_temp, veml7700_illuminance, max6675_engine_temp, hcsr04_distance, adxl345_acceleration);
       if (storage_write_line(line))
       {
         printf(">> Zapisano.\n");
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