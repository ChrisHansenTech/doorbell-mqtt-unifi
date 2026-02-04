#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include "test_config_mock.h"
#include <stdlib.h>
#include <string.h>

void make_mock_config(config_t *cfg) {
    cfg->preset_cfg.count = 2;
    cfg->preset_cfg.items = calloc(cfg->preset_cfg.count, sizeof(*cfg->preset_cfg.items));

    cfg->preset_cfg.items[0].display_name = strdup("Christmas");
    cfg->preset_cfg.items[0].key_name = strdup("christmas");
    cfg->preset_cfg.items[0].directory = strdup("christmas");

    cfg->preset_cfg.items[1].display_name = strdup("Labor Day");
    cfg->preset_cfg.items[1].key_name = strdup("labor day");
    cfg->preset_cfg.items[1].directory = strdup("labor_day");
}