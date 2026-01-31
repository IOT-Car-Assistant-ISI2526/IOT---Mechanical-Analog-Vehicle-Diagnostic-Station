#pragma once
#include <stdint.h>
#include <stdbool.h>

void sntp_client_init(void);

void sntp_client_set_timestamp(uint32_t unix_timestamp);

uint32_t sntp_client_get_timestamp(void);

bool sntp_client_is_synced(void);
