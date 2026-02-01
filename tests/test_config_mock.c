#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include "test_config_mock.h"
#include <stdlib.h>
#include <string.h>

void make_mock_config(config_t *cfg) {
    cfg->count = 2;

    cfg->holiday_names = malloc(sizeof(char*) * cfg->count);
    cfg->directory_names = malloc(sizeof(char*) * cfg->count);

    cfg->holiday_names[0] = strdup("Christmas");
    cfg->directory_names[0] = strdup("christmas");

    cfg->holiday_names[1] = strdup("Labor Day");
    cfg->directory_names[1] = strdup("labor_day");
}