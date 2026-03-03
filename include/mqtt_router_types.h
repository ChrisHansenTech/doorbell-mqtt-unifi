#pragma once

#include "config_types.h"
#include <linux/limits.h>
#include <stddef.h>

typedef struct sfx_ctx {
    const config_sfx_preset_t *sfx_preset_cfg;
    const char *sounds_dir;
} sfx_ctx_t;

typedef struct mqtt_router_ctx {
    const config_ssh_t *ssh_cfg;
    const config_preset_t *preset_cfg;
    const config_unifi_t *unifi_cfg;
    const sfx_ctx_t *sfx_ctx;
} mqtt_router_ctx_t;

typedef void (*mqtt_handler_fn)(const mqtt_router_ctx_t *ctx, const char *payload, size_t len);