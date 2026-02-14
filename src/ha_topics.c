#include "ha_topics.h"
#include "mqtt.h"
#include "ha_entities.h"
#include "mqtt_router.h"

#include <stdio.h>
#include <string.h>

static char g_base_topic[128];
static char g_availability_topic[256];
static char g_status_topic[256];

void ha_topics_init(const config_t *cfg) {
    snprintf(g_base_topic, sizeof(g_base_topic), "%s/doorbell-mqtt/%s", cfg->mqtt_cfg.prefix, cfg->mqtt_cfg.instance);
    snprintf(g_availability_topic, sizeof(g_availability_topic), "%s/availability", g_base_topic);
    snprintf(g_status_topic, sizeof(g_status_topic), "%s/status", g_base_topic);
}

const char *ha_get_base_topic(void) {
    return g_base_topic;
}

void ha_build_topic(char *out, size_t out_size, const char *subtopic) {
    if (!out || out_size == 0 || !subtopic) {
        return;
    }

    snprintf(out, out_size, "%s/%s", g_base_topic, subtopic);
}

const char *ha_availability_topic(void) {
    return g_availability_topic;
}

const char *ha_status_topic(void) {
    return g_status_topic;
}

void ha_topic_subscribe_commands(void) {
    for (size_t i = 0; i < HA_ENTITIES_COUNT; i++) {
        const entity_t *ent = &HA_ENTITIES[i];

        if(ent->command_topic == NULL || ent->command_topic[0] == '\0') {
            continue;
        }

        char buffer[256];
        ha_build_topic(buffer, sizeof(buffer), ent->command_topic);
        mqtt_subscribe(buffer);
        mqtt_routes_add(buffer, ent->handle_command);
    }
}