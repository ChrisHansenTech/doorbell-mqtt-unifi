#pragma once

#include "config_types.h"
#include "mqtt_router_types.h"

/**
 * @brief Resolves the given SFX preset name to a config_sfx_preset_item_t structure. The returned pointer is owned by the sfx_ctx and should not be freed by the caller.
 * 
 * @param sfx_name 
 * @param sfx_ctx 
 * @return config_sfx_preset_item_t* 
 */
config_sfx_preset_item_t *unifi_sfx_resolve_sfx(const char *sfx_name, const sfx_ctx_t *sfx_ctx);