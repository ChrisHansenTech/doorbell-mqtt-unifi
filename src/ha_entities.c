#include "ha_entities.h"
#include "command.h"
#include "config.h"

#include "cJSON.h"



static options_result_t add_holiday_options(cJSON *root, const config_t *cfg, const entity_t *def) {
    (void)def;

    if (cfg->holiday_cfg.count == 0) {
        return OPTIONS_ERR_EMPTY_LIST;
    }

    cJSON *holidays = cJSON_AddArrayToObject(root, "options");

    if (!holidays) {
        return OPTIONS_ERR_UNKNOWN;
    }

    for(size_t i = 0; i < cfg->holiday_cfg.count; i++) {
        cJSON_AddItemToArray(holidays, cJSON_CreateString(cfg->holiday_cfg.items[i].holiday));
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
        .value_template = "{{ value_json.message }}",
        .json_attributes_topic = "last_error",
        .json_attributes_template = NULL,
        .add_options = NULL,
        .handle_command = NULL
    }, {
        .component = "sensor",
        .object_id = "active_profile",
        .name = "Active Profile",
        .category = NULL,
        .state_topic = "active_profile",
        .availability_topic ="availability",
        .command_topic = NULL,
        .icon = "mdi:badge-account",
        .add_options = NULL,
        .handle_command = NULL
    }, {
        .component = "select",
        .object_id = "holiday",
        .name = "Holiday",
        .category = "config",
        .state_topic = "holiday/selected",
        .availability_topic ="availability",
        .command_topic = "cmd/holiday_set",
        .icon = "mdi:calendar-star",
        .add_options = add_holiday_options,
        .handle_command = command_set_holiday
    }, {
        .component = "text",
        .object_id = "custom_directory",
        .name = "Custom directory",
        .category = "config",
        .state_topic = "custom/directory",
        .availability_topic ="availability",
        .command_topic = "cmd/apply_custom",
        .icon = "mdi:folder",
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
        .add_options = NULL,
        .handle_command = command_download_assets
    }, {
        .component = "sensor",
        .object_id = "last_download_path",
        .name = "Last Asset Download",
        .category = "diagnostic",
        .state_topic = "download/last_path",
        .availability_topic = "availability",
        .command_topic = NULL,
        .icon = "mdi:folder-arrow-down",
        .add_options = NULL,
        .handle_command = NULL
    }
};

const size_t HA_ENTITIES_COUNT = 8;