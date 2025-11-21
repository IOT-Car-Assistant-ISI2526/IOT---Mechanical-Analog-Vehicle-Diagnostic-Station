#ifndef WIFI_STATION_H
#define WIFI_STATION_H

#include <stdbool.h> 

/**
 * @brief Inicjuje i uruchamia połączenie Wi-Fi w trybie stacji.
 * * Funkcja blokuje działanie do czasu uzyskania pierwszego połączenia z siecią Wi-Fi.
 * Automatycznie obsługuje ponowne łączenie w tle.
 */
void wifi_station_init(void);

/**
 * @brief Sprawdza aktualny stan połączenia Wi-Fi.
 * * @return true jeśli urządzenie jest połączone z Wi-Fi i ma adres IP,
 * @return false w przeciwnym razie.
 */
bool wifi_station_is_connected(void);

#endif // WIFI_STATION_H