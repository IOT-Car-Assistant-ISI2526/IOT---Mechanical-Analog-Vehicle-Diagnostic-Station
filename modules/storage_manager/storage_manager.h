#pragma once

#include <stddef.h>
#include <stdbool.h>

// Inicjalizuje system plików (montuje SPIFFS)
void storage_init(void);

// Zwraca ilość wolnego miejsca w bajtach
size_t storage_get_free_space(void);

// Usuwa plik z notatkami (zwalnia miejsce)
void storage_clear_all(void);

// Zapisuje linię tekstu (zwraca true jeśli się udało)
bool storage_write_line(const char* text);

// Odczytuje całą zawartość (zwraca wskaźnik, który trzeba zwolnić free!)
char* storage_read_all(void);