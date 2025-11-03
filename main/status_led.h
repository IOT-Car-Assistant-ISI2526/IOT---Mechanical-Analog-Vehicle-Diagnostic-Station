#ifndef STATUS_LED_H
#define STATUS_LED_H

/**
 * @brief Uruchamia zadanie (task) obsługujące diodę LED statusu.
 * * Dioda mruga inaczej w zależności od stanu połączenia Wi-Fi.
 */
void status_led_start_task(void);

#endif // STATUS_LED_H