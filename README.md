# ESP32 – WiFi Station + prosty klient HTTP

Układ:
- łączy się z wybraną siecią WiFi,
- reaguje na utratę połączenia (migająca dioda LED),
- próbuje ponownie połączyć się z WiFi,
- po uzyskaniu połączenia pobiera dane z serwera http.

---

iot_project/
│
├── CMakeLists.txt
├── sdkconfig
├── README.md
│
├── main/
│   ├── main.c
│   ├── app_main.c            → logika główna modułu diagnostycznego
│   ├── app_main.h
│   ├── CMakeLists.txt
│
├── modules/
│   │
│   ├── wifi_station/
│   │   ├── wifi_station.c
│   │   ├── wifi_station.h
│   │   └── CMakeLists.txt
│   │
│   ├── http_client/          → wysyłanie danych REST (jeśli używasz)
│   │   ├── http_client.c
│   │   ├── http_client.h
│   │   └── CMakeLists.txt
│   │
│   ├── mqtt_client/          → MQTT do wysyłania buforów
│   │   ├── mqtt_client_app.c
│   │   ├── mqtt_client_app.h
│   │   └── CMakeLists.txt
│   │
│   ├── sensors/
│   │   ├── max6675.c
│   │   ├── max6675.h
│   │   ├── bme280.c
│   │   ├── bme280.h
│   │   ├── hcsr04.c
│   │   ├── hcsr04.h
│   │   ├── adxl345.c
│   │   ├── adxl345.h
│   │   ├── light_veml7700.c
│   │   ├── light_veml7700.h
│   │   └── CMakeLists.txt
│   │
│   ├── storage/              → buforowanie danych: NVS / FATFS / SPIFFS
│   │   ├── storage.c
│   │   ├── storage.h
│   │   └── CMakeLists.txt
│   │
│   ├── ble_service/
│   │   ├── ble_service.c
│   │   ├── ble_service.h
│   │   └── CMakeLists.txt
│   │
│   ├── ui_led/
│   │   ├── status_led.c
│   │   ├── status_led.h
│   │   └── CMakeLists.txt
│
└── build/
