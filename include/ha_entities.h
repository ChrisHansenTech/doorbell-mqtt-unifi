#pragma once

#include "mqtt_router_types.h"
#include "cJSON.h"
#include <stddef.h>

typedef struct entity_t entity_t;

typedef enum {
    OPTIONS_OK = 0,
    OPTIONS_ERR_NO_CONFIG,
    OPTIONS_ERR_EMPTY_LIST,
    OPTIONS_ERR_UNKNOWN
} options_result_t;

typedef options_result_t (*add_options_fn)(cJSON *root, const config_t *cfg, const entity_t *def);

struct entity_t {
    const char *component;
    const char *object_id;
    const char *name;
    const char *category;
    const char *state_topic;
    const char *availability_topic;
    const char *command_topic;
    const char *icon;
    const char *value_template;
    const char *json_attributes_topic;
    const char *json_attributes_template;
    add_options_fn add_options;
    mqtt_handler_fn handle_command;
};

extern const entity_t HA_ENTITIES[];
extern const size_t HA_ENTITIES_COUNT;