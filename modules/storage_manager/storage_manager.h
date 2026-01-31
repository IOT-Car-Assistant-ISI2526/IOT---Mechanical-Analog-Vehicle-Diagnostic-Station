#pragma once

#include <stddef.h>
#include <stdbool.h>

void storage_init(void);

size_t storage_get_free_space(void);

void storage_clear_all(void);

bool storage_write_line(const char* text);

char* storage_read_all(void);