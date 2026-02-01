#pragma once

#include "config_types.h"

#include <stddef.h>
#include <stdbool.h>


bool config_load(const char *filename, config_t *cfg);
const char *config_get_holiday_directory(const config_t *config, const char *holiday_name);
void config_free(config_t *cfg);
