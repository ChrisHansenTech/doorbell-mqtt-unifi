#include "ha_discovery.h"
#include "config_types.h"
#include "logger.h"
#include "mqtt.h"
#include "ha_entities.h"
#include "ha_topics.h"
#include "version.h"

#include "cJSON.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define HA_DISCOVERY_PAYLOAD_SIZE 4096

static void build_entity_name(char *out, size_t out_len, const char *name, const char *instance) {
    if (strcasecmp(instance, "default") == 0) {
        snprintf(out, out_len, "%s", name);
    } else {
        snprintf(out, out_len, "%s (%s)", name, instance);
    }
}

static void build_unique_id(char *out, size_t out_len, const char *object_id, const char *instance) {
    const char *slug = "doorbell_mqtt_unifi"; 

    if (strcmp(instance, "default") == 0) {
        snprintf(out, out_len, "%s_%s", slug, object_id);
    } else {
        snprintf(out, out_len, "%s_%s_%s", slug, instance, object_id);
    }
}

static void build_default_entity_id(char *out, size_t out_len, const char *component, const char *object_id, const char *instance) {
    const char *slug = "doorbell_mqtt_unifi";

    if (strcmp(instance, "default") == 0) {
        snprintf(out, out_len, "%s.%s_%s", component, slug, object_id);
    } else {
        snprintf(out, out_len, "%s.%s_%s_%s", component, slug, object_id, instance);
    }
}

static bool create_device(cJSON *out, const char *prefix, const char *instance, const char *instance_human) {
    if (!out || !prefix || !instance || !instance_human) {
        return false;
    }

    cJSON *device = cJSON_AddObjectToObject(out, "device");
    if (!device) {
        return false;
    }

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s_doorbell_mqtt_%s", prefix, instance);
    cJSON_AddStringToObject(device, "identifiers", buffer);

    cJSON_AddStringToObject(device, "manufacturer", "ChrisHansenTech");
    cJSON_AddStringToObject(device, "model", "UniFi Doorbell MQTT Service");

    build_entity_name(buffer, sizeof(buffer), "UniFi Doorbell MQTT Service", instance_human);
    cJSON_AddStringToObject(device, "name", buffer);

    cJSON_AddStringToObject(device, "sw_version", APP_VERSION);

    return true;
}

static bool build_entity_payload(char *payload, size_t payload_size, const entity_t *d, const config_t *cfg) {
    if (!payload || payload_size == 0 || !d || !cfg) {
        return false;
    }

    cJSON *root = cJSON_CreateObject();

    if (!root) {
        LOG_ERROR("Failed to create entity [%s] payload", d->component);
        return false;
    }

    char buffer[256];
    build_entity_name(buffer, sizeof(buffer), d->name, cfg->mqtt_cfg.instance_human);
    cJSON_AddStringToObject(root, "name", buffer);

    if (d->category) {
        cJSON_AddStringToObject(root, "entity_category", d->category);
    }

    cJSON_AddStringToObject(root, "object_id", d->object_id);

    build_unique_id(buffer, sizeof(buffer), d->object_id, cfg->mqtt_cfg.instance);
    
    cJSON_AddStringToObject(root, "unique_id", buffer);

    build_default_entity_id(buffer, sizeof(buffer), d->component, d->object_id, cfg->mqtt_cfg.instance);
    cJSON_AddStringToObject(root,"default_entity_id", buffer);

    ha_build_topic(buffer, sizeof(buffer), d->state_topic);
    cJSON_AddStringToObject(root, "state_topic", buffer);

    ha_build_topic(buffer, sizeof(buffer), d->availability_topic);
    cJSON_AddStringToObject(root, "availability_topic", buffer);

    if (d->command_topic) {
        ha_build_topic(buffer, sizeof(buffer), d->command_topic);
        cJSON_AddStringToObject(root, "command_topic", buffer);
    }

    if (d->add_options) {
        options_result_t result = d->add_options(root, cfg, d);

        if(result != OPTIONS_OK) {
            cJSON_Delete(root);
            return false;
        }
    }

    cJSON_AddStringToObject(root, "icon", d->icon);

    if (d->device_class) {
        cJSON_AddStringToObject(root, "device_class", d->device_class);
    }

    if (d->value_template) {
        cJSON_AddStringToObject(root, "value_template", d->value_template);
    }

    if (d->json_attributes_topic) {
        ha_build_topic(buffer, sizeof(buffer), d->json_attributes_topic);
        cJSON_AddStringToObject(root, "json_attributes_topic", buffer);
    }

    if (!create_device(root, cfg->mqtt_cfg.prefix, cfg->mqtt_cfg.instance, cfg->mqtt_cfg.instance_human)) {
        cJSON_Delete(root);
        return false;
    }

    if (d->json_attributes_template) {
        cJSON_AddStringToObject(root, "json_attributes_template", d->json_attributes_template);
    }

    cJSON *origin = cJSON_AddObjectToObject(root, "origin");

    if (!origin) {
        cJSON_Delete(root);
        return false;
    }

    cJSON_AddStringToObject(origin, "name", "UniFi Doorbell MQTT Service");
    cJSON_AddStringToObject(origin, "sw_version", APP_VERSION);
    cJSON_AddStringToObject(origin, "url", "https://github.com/ChrisHansenTech/doorbell-mqtt-unifi");

    char *tmp = cJSON_PrintBuffered(root, (int)payload_size, 0);

    if (!tmp) {
        LOG_ERROR("Failed to serialize Home Assistant discovery payload for entity '%s'", d->object_id);
        cJSON_Delete(root);
        return false;
    }

    size_t tmp_len = strlen(tmp);
    if (tmp_len >= payload_size) {
        LOG_ERROR("Home Assistant discovery payload for entity '%s' (%zu bytes) exceeds buffer size (%zu bytes); payload truncated",
                  d->object_id, tmp_len, payload_size - 1);
        payload[0] = '\0';
        cJSON_Delete(root);
        free(tmp);
        return false;
    }

    memcpy(payload, tmp, tmp_len + 1);

    cJSON_Delete(root);
    free(tmp);

    return true;
}

bool ha_publish_discovery(const config_t *cfg) {
    if (!cfg) {
        return false;
    }

    for (size_t i = 0; i < HA_ENTITIES_COUNT; i++) {
        const entity_t *d = &HA_ENTITIES[i];

        char topic[256];
        char payload[HA_DISCOVERY_PAYLOAD_SIZE];

        int topic_len = snprintf(topic, sizeof(topic), "homeassistant/%s/%s_doorbell_mqtt_%s_%s/config",
                                 d->component, cfg->mqtt_cfg.prefix, cfg->mqtt_cfg.instance, d->object_id);
        if (topic_len < 0 || (size_t)topic_len >= sizeof(topic)) {
            LOG_ERROR("Home Assistant discovery topic for entity '%s' exceeds buffer size (%zu bytes)",
                      d->object_id, sizeof(topic) - 1);
            return false;
        }

        if (!build_entity_payload(payload, sizeof(payload), d, cfg)) {
            return false;
        }

        mqtt_publish(topic, payload, 1, 1);
    }

    return true;
}