#include "unifi_sfx.h"
#include "logger.h"
#include "mqtt_router_types.h"
#include <strings.h>

config_sfx_preset_item_t *unifi_sfx_resolve_sfx(const char *sfx_name, const sfx_ctx_t *sfx_ctx) {
    if (!sfx_name || !sfx_ctx) {
        LOG_ERROR("Invalid parameters: sfx_name=%p, sfx_preset_cfg=%p",(void*)sfx_name, (void*)sfx_ctx);
        return NULL;
    }

    for (size_t i = 0; i < sfx_ctx->sfx_preset_cfg->count; i++) {
        if (strcasecmp(sfx_ctx->sfx_preset_cfg->items[i].name, sfx_name) == 0) {
            return &sfx_ctx->sfx_preset_cfg->items[i];
        }
    }

    return NULL;
}

