#pragma once

#include "config_types.h"

#include <stddef.h>
#include <stdbool.h>


bool config_load(const char *filename, config_t *cfg);
void config_free(config_t *cfg);
