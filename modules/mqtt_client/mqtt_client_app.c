#include "mqtt_client_app.h"
#include "mqtt_client.h"
#include "project_config.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "storage_manager.h"
#include "wifi_station.h"
#include "ble_internal.h"
#include "buzzer.h"

#include <string.h>
#include <stdlib.h>


static volatile bool mqtt_exit_requested = false;
static const char *user = "user";
static volatile bool mqtt_connected = false;

static void get_mac_str(char *out, size_t len)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, len, "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                              int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    char mac_str[13];
    get_mac_str(mac_str, sizeof(mac_str));

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT connected");
        {
            char topic[128];
            snprintf(topic, sizeof(topic), "%s/%s/alerts", user, mac_str);
            esp_mqtt_client_subscribe(event->client, topic, 0);
            ESP_LOGI(TAG, "Subscribed to %s", topic);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected = false;
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_DATA:
        if (event->topic_len && event->data_len) {
            char topic[128];
            snprintf(topic, sizeof(topic), "%s/%s/alerts", user, mac_str);

            if (strncmp(event->topic, topic, event->topic_len) == 0) {
                char payload[64];
                int len = event->data_len < sizeof(payload)-1 ? event->data_len : sizeof(payload)-1;
                memcpy(payload, event->data, len);
                payload[len] = '\0';

                ESP_LOGI(TAG, "Received on %s: %s", topic, payload);

                if (strcmp(payload, "BUZZ") == 0) {
                    buzzer_beep(500);
                }
                if (strcmp(payload, "EXIT") == 0) {
                        mqtt_exit_requested = true;
                }
            }
        }
        break;

    default:
        break;
    }
}

static void publish_hello(esp_mqtt_client_handle_t client,
                          const char *user,
                          const char *mac,
                          const char *sensor)
{
    char topic[128];
    snprintf(topic, sizeof(topic),
             "%s/%s/sensor/%s", user, mac, sensor);

    esp_mqtt_client_publish(client, topic, "hello", 0, 0, 0);
    ESP_LOGI(TAG, "Sent hello to %s", topic);
}

static void publish_storage_via_mqtt(esp_mqtt_client_handle_t client,
                                     const char *user,
                                     const char *mac)
{
    char *data = storage_read_all();
    if (data == NULL) {
        ESP_LOGI(TAG, "No stored data to send");
        return;
    }

    ESP_LOGI(TAG, "Sending stored data via MQTT...");

    char *line_ctx = NULL;
    char *line = strtok_r(data, "\n", &line_ctx);
    
    // For each line: TYPE;TIMESTAMP;VALUE
    while (line) { 
        char *tok_ctx = NULL;
        char *type = strtok_r(line, ";", &tok_ctx);
        char *ts   = strtok_r(NULL, ";", &tok_ctx);
        char *val  = strtok_r(NULL, ";", &tok_ctx);

        if (type && ts && val) {
            char topic[128];
            snprintf(topic, sizeof(topic),
                     "%s/%s/sensor/%s", user, mac, type);

            char payload[64];
            snprintf(payload, sizeof(payload),
                     "%s;%s", ts, val);

            //send via MQTT
            esp_mqtt_client_publish( 
                client,
                topic,
                payload,
                0, // auto length
                0, // QoS 0
                0  // no retain
            );

            ESP_LOGI(TAG, "MQTT -> %s : %s", topic, payload);

            vTaskDelay(pdMS_TO_TICKS(20));
        }

        line = strtok_r(NULL, "\n", &line_ctx);
    }

    free(data);

    ESP_LOGI(TAG, "All stored data sent, clearing SPIFFS");
    storage_clear_all();
}


static void mqtt_task(void *arg)
{
    while (!wifi_station_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    char mac[32];
    get_mac_str(mac, sizeof(mac));


    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    while (!mqtt_connected) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    publish_hello(client, user, mac, "adxl");
    publish_hello(client, user, mac, "max_normal");
    publish_hello(client, user, mac, "max_profile");

    publish_storage_via_mqtt(client, user, mac);

    ESP_LOGI(TAG, "All MQTT data sent");

    while (!mqtt_exit_requested) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    ESP_LOGI(TAG, "EXIT received, stopping MQTT...");

    esp_mqtt_client_stop(client);
    esp_mqtt_client_destroy(client);
    vTaskDelete(NULL);
}


void mqtt_client_start(void)
{
    xTaskCreate(mqtt_task, "mqtt_hello", 4096, NULL, 5, NULL);
}
