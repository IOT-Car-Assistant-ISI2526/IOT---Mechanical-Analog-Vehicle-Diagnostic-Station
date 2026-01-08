#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

/* --- Wi-Fi config --- */
#define CONFIG_ESP_WIFI_SSID      "yourssid"
#define CONFIG_ESP_WIFI_PASS      "yourpassword"

/* --- GPIO config --- */
#define CONFIG_BLINK_GPIO 2

/* --- HTTP client config --- */
#define CONFIG_WEB_SERVER "example.com"
#define CONFIG_WEB_PORT   "80" // Port HTTP
#define CONFIG_WEB_PATH   "/"  



/* --- I2C for BMP280 & ADXL345 --- */
#define PORT_0_SDA_PIN 21
#define PORT_0_SCL_PIN 22

/* --- I2C for VEML7700 --- */
#define PORT_1_SDA_PIN 25
#define PORT_1_SCL_PIN 26

/* --- SPI config --- */
#define PIN_CLK 14
#define PIN_SO 12 // MISO
#define PIN_MOSI -1

/* --- Measurement timing config --- */ //TODO INCONSISTENT NAMING
#define STARTUP_DELAY_MS 500
#define BMP280_MEASUREMENT_INTERVAL_MS 15*60000
#define VEML7700_MEASUREMENT_INTERVAL_MS 60*60000
#define MAX6675_MEASUREMENT_INTERVAL_MS 5*60000
#define FREQUENT_MEASUREMENT_INTERVAL_MS 1000
#define HCSR04_SLOWMODE_INTERVAL_MS 15000
#define HCSR04_FASTMODE_INTERVAL_MS 500
#define HCSR04_FASTMODE_TIMEOUT_MS   5000 
#define INCORRECT_MEASUREMENT_INTERVAL_MS 2000

#define MAX6675_PROFILE_SAMPLES_COUNT 100

// Experiment: save-first-N samples to persistent storage (drop-new policy)
#define ADXL_SAVE_LIMIT 20
#define MAX6675_SAVE_LIMIT 20

// MQTT broker used for flushing stored samples (change as needed)
#define MQTT_BROKER_URI "mqtt://10.219.44.41:1883"

#define HCSR04_TRIGGER_COUNT 3 


#endif // PROJECT_CONFIG_H