#pragma once
#include <stdint.h>
#include <stdbool.h>

// Inicjalizacja modułu - odczytuje ostatni znany timestamp z NVS
void sntp_client_init(void);

// Ustawia czas z telefonu (wywoływane przez BLE) i zapisuje do NVS
void sntp_client_set_timestamp(uint32_t unix_timestamp);

// Pobiera aktualny czas (zwraca timestamp lub 0 jeśli nie zsynchronizowany)
uint32_t sntp_client_get_timestamp(void);

// Sprawdza czy czas został już zsynchronizowany (BLE lub z NVS)
bool sntp_client_is_synced(void);
