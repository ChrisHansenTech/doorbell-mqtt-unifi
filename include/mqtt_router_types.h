#pragma once

#include "config_types.h"
#include <stddef.h>

typedef struct mqtt_router_ctx {
    const config_ssh_t *ssh_cfg;
    const config_holiday_t *holiday_cfg;
} mqtt_router_ctx_t;

typedef void (*mqtt_handler_fn)(const mqtt_router_ctx_t *ctx, const char *payload, size_t len);