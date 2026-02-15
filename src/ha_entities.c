#include "ha_entities.h"
#include "command.h"
#include "config.h"

#include "cJSON.h"



static options_result_t add_preset_options(cJSON *root, const config_t *cfg, const entity_t *def) {
    (void)def;

    if (cfg->preset_cfg.count == 0) {
        return OPTIONS_ERR_EMPTY_LIST;
    }

    cJSON *presets = cJSON_AddArrayToObject(root, "options");

    if (!presets) {
        return OPTIONS_ERR_UNKNOWN;
    }

    for(size_t i = 0; i < cfg->preset_cfg.count; i++) {
        cJSON_AddItemToArray(presets, cJSON_CreateString(cfg->preset_cfg.items[i].display_name));
    }

    return OPTIONS_OK;
} 

const entity_t HA_ENTITIES[]  = {
    {
        .component = "sensor",
        .object_id = "status",
        .name = "Status",
        .category = "diagnostic",
        .state_topic = "status",
        .availability_topic = "availability",
        .command_topic = NULL,
        .icon = "mdi:information-outline",
        .device_class = NULL,
        .add_options = NULL,
        .handle_command = NULL
    }, {
        .component = "sensor",
        .object_id = "last_error",
        .name = "Last Error",
        .category = "diagnostic",
        .state_topic = "last_error",
        .availability_topic = "availability",
        .command_topic = NULL,
        .icon = "mdi:alert-circle-outline",
        .device_class = NULL,
        .value_template = "{{ value_json.message }}",
        .json_attributes_topic = "last_error",
        .json_attributes_template = NULL,
        .add_options = NULL,
        .handle_command = NULL
    }, {
        .component = "sensor",
        .object_id = "last_applied_profile",
        .name = "Last Applied Profile",
        .category = NULL,
        .state_topic = "last_applied_profile",
        .availability_topic ="availability",
        .command_topic = NULL,
        .icon = "mdi:badge-account",
        .device_class = NULL,
        .add_options = NULL,
        .handle_command = NULL
    }, {
        .component = "select",
        .object_id = "preset",
        .name = "Presets",
        .category = "config",
        .state_topic = "preset/selected",
        .availability_topic ="availability",
        .command_topic = "cmd/preset_set",
        .icon = "mdi:tune-variant",
        .device_class = NULL,
        .add_options = add_preset_options,
        .handle_command = command_set_preset
    }, {
        .component = "text",
        .object_id = "custom_directory",
        .name = "Custom directory",
        .category = "config",
        .state_topic = "custom/directory",
        .availability_topic ="availability",
        .command_topic = "cmd/apply_custom",
        .icon = "mdi:folder",
        .device_class = NULL,
        .add_options = NULL,
        .handle_command = command_apply_custom
    }, {
        .component = "button",
        .object_id = "test_config",
        .name = "Test Config",
        .category = "config",
        .state_topic = NULL,
        .availability_topic = "availability",
        .command_topic = "cmd/test_config",
        .icon = "mdi:test-tube",
        .device_class = NULL,
        .add_options = NULL,
        .handle_command = command_test_config
    }, {
        .component = "button",
        .object_id = "download_assets",
        .name = "Asset Download",
        .category = NULL,
        .state_topic = NULL,
        .availability_topic = "availability",
        .command_topic = "cmd/download_assets",
        .icon = "mdi:download",
        .device_class = NULL,
        .add_options = NULL,
        .handle_command = command_download_assets
    }, {
        .component = "sensor",
        .object_id = "last_download",
        .name = "Last Asset Download",
        .category = "diagnostic",
        .state_topic = "last_download/time/state",
        .availability_topic = "availability",
        .command_topic = NULL,
        .icon = "mdi:folder-arrow-down",
        .device_class = "timestamp",
        .value_template = NULL,
        .json_attributes_topic = "last_download/time/attributes",
        .json_attributes_template = NULL,
        .add_options = NULL,
        .handle_command = NULL
    }
};

const size_t HA_ENTITIES_COUNT = 8;