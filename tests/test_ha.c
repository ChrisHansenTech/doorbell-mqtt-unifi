#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "mocks/mock_mqtt.h"
#include "support/test_config_mock.h"
#include "support/test_helpers.h"
#include "cJSON.h"
#include "config.h"
#include "ha_discovery.h"
#include "ha_entities.h"
#include "ha_mqtt.h"
#include "ha_status.h"
#include "ha_topics.h"
#include "state.h"

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static config_t g_cfg;
static char g_tmp_dir[128];

static void setup_config(const char *prefix, const char *instance, const char *instance_human) {
    memset(&g_cfg, 0, sizeof(g_cfg));
    make_mock_config(&g_cfg);

    snprintf(g_cfg.mqtt_cfg.prefix, sizeof(g_cfg.mqtt_cfg.prefix), "%s", prefix);
    snprintf(g_cfg.mqtt_cfg.instance, sizeof(g_cfg.mqtt_cfg.instance), "%s", instance);
    snprintf(g_cfg.mqtt_cfg.instance_human, sizeof(g_cfg.mqtt_cfg.instance_human), "%s", instance_human);
    g_cfg.mqtt_cfg.qos = 1;
    g_cfg.mqtt_cfg.retained_online = 1;

    g_cfg.sfx_preset_cfg.count = 1;
    g_cfg.sfx_preset_cfg.items = calloc(1, sizeof(*g_cfg.sfx_preset_cfg.items));
    g_cfg.sfx_preset_cfg.items[0].name = strdup("Chime");
    g_cfg.sfx_preset_cfg.items[0].key_name = strdup("chime");
    g_cfg.sfx_preset_cfg.items[0].file = strdup("chime.ogg");
    g_cfg.sfx_preset_cfg.items[0].volume = 80;
}

void setUp(void) {
    mock_mqtt_reset();
    setup_config("chrishansentech", "default", "Default");
    ha_topics_init(&g_cfg);
    TEST_ASSERT_TRUE(test_make_temp_dir(g_tmp_dir, sizeof(g_tmp_dir)));
}

void tearDown(void) {
    config_free(&g_cfg);
    test_remove_dir_recursive(g_tmp_dir);
}

void test_ha_topics_build_and_subscribe_commands(void) {
    TEST_ASSERT_EQUAL_STRING("chrishansentech/doorbell-mqtt/default", ha_get_base_topic());
    TEST_ASSERT_EQUAL_STRING("chrishansentech/doorbell-mqtt/default/availability", ha_availability_topic());
    TEST_ASSERT_EQUAL_STRING("chrishansentech/doorbell-mqtt/default/status", ha_status_topic());

    char out[256] = "untouched";
    ha_build_topic(NULL, sizeof(out), "status");
    ha_build_topic(out, 0, "status");
    ha_build_topic(out, sizeof(out), NULL);
    TEST_ASSERT_EQUAL_STRING("untouched", out);

    ha_build_topic(out, sizeof(out), "cmd/preset_set");
    TEST_ASSERT_EQUAL_STRING("chrishansentech/doorbell-mqtt/default/cmd/preset_set", out);

    ha_topic_subscribe_commands();
    TEST_ASSERT_EQUAL_UINT(7, g_mock_mqtt.subscribe_count);
    TEST_ASSERT_TRUE(mock_mqtt_has_subscribe("chrishansentech/doorbell-mqtt/default/cmd/preset_set"));
    TEST_ASSERT_TRUE(mock_mqtt_has_subscribe("chrishansentech/doorbell-mqtt/default/cmd/apply_custom"));
    TEST_ASSERT_TRUE(mock_mqtt_has_subscribe("chrishansentech/doorbell-mqtt/default/cmd/test_config"));
    TEST_ASSERT_TRUE(mock_mqtt_has_subscribe("chrishansentech/doorbell-mqtt/default/cmd/download_assets"));
    TEST_ASSERT_TRUE(mock_mqtt_has_subscribe("chrishansentech/doorbell-mqtt/default/cmd/validate_profile"));
    TEST_ASSERT_TRUE(mock_mqtt_has_subscribe("chrishansentech/doorbell-mqtt/default/cmd/reapply_last_profile"));
    TEST_ASSERT_TRUE(mock_mqtt_has_subscribe("chrishansentech/doorbell-mqtt/default/cmd/sfx_preset_play"));
}

void test_ha_entities_metadata_consistency(void) {
    TEST_ASSERT_EQUAL_UINT(12, HA_ENTITIES_COUNT);

    size_t command_entities = 0;
    for (size_t i = 0; i < HA_ENTITIES_COUNT; i++) {
        TEST_ASSERT_NOT_NULL(HA_ENTITIES[i].component);
        TEST_ASSERT_NOT_NULL(HA_ENTITIES[i].object_id);
        TEST_ASSERT_NOT_NULL(HA_ENTITIES[i].name);
        TEST_ASSERT_NOT_NULL(HA_ENTITIES[i].availability_topic);
        TEST_ASSERT_NOT_NULL(HA_ENTITIES[i].icon);
        if (HA_ENTITIES[i].command_topic) {
            TEST_ASSERT_NOT_NULL(HA_ENTITIES[i].handle_command);
            command_entities++;
        }
    }
    TEST_ASSERT_EQUAL_UINT(7, command_entities);
}

void test_ha_discovery_default_instance_publishes_all_entities(void) {
    TEST_ASSERT_FALSE(ha_publish_discovery(NULL));

    TEST_ASSERT_TRUE(ha_publish_discovery(&g_cfg));
    TEST_ASSERT_EQUAL_UINT(HA_ENTITIES_COUNT, g_mock_mqtt.publish_count);

    /* Validate preset select discovery payload */
    const mock_mqtt_publish_t *preset_pub = mock_mqtt_find_last_publish("_preset/config");
    TEST_ASSERT_NOT_NULL(preset_pub);
    TEST_ASSERT_EQUAL_STRING("homeassistant/select/chrishansentech_doorbell_mqtt_default_preset/config", preset_pub->topic);
    TEST_ASSERT_EQUAL_INT(1, preset_pub->qos);
    TEST_ASSERT_EQUAL_INT(1, preset_pub->retained);

    cJSON *root = cJSON_Parse(preset_pub->payload);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_STRING("Presets", cJSON_GetObjectItem(root, "name")->valuestring);
    TEST_ASSERT_EQUAL_STRING("doorbell_mqtt_unifi_preset", cJSON_GetObjectItem(root, "unique_id")->valuestring);
    TEST_ASSERT_EQUAL_STRING("select.doorbell_mqtt_unifi_preset", cJSON_GetObjectItem(root, "default_entity_id")->valuestring);

    cJSON *opts = cJSON_GetObjectItem(root, "options");
    TEST_ASSERT_TRUE(cJSON_IsArray(opts));
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetArraySize(opts));
    TEST_ASSERT_EQUAL_STRING("Christmas", cJSON_GetArrayItem(opts, 0)->valuestring);
    TEST_ASSERT_EQUAL_STRING("Labor Day", cJSON_GetArrayItem(opts, 1)->valuestring);

    cJSON *dev = cJSON_GetObjectItem(root, "device");
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_STRING("UniFi Doorbell MQTT Service", cJSON_GetObjectItem(dev, "name")->valuestring);

    cJSON *origin = cJSON_GetObjectItem(root, "origin");
    TEST_ASSERT_NOT_NULL(origin);
    TEST_ASSERT_EQUAL_STRING("UniFi Doorbell MQTT Service", cJSON_GetObjectItem(origin, "name")->valuestring);
    cJSON_Delete(root);

    /* Validate SFX preset select options */
    const mock_mqtt_publish_t *sfx_pub = mock_mqtt_find_last_publish("_sfxpreset/config");
    TEST_ASSERT_NOT_NULL(sfx_pub);
    root = cJSON_Parse(sfx_pub->payload);
    TEST_ASSERT_NOT_NULL(root);
    opts = cJSON_GetObjectItem(root, "options");
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(opts));
    TEST_ASSERT_EQUAL_STRING("Chime", cJSON_GetArrayItem(opts, 0)->valuestring);
    cJSON_Delete(root);
}

void test_ha_discovery_custom_instance_namespacing(void) {
    config_free(&g_cfg);
    setup_config("myhome", "front_door", "Front Door");
    ha_topics_init(&g_cfg);

    TEST_ASSERT_TRUE(ha_publish_discovery(&g_cfg));
    TEST_ASSERT_EQUAL_UINT(HA_ENTITIES_COUNT, g_mock_mqtt.publish_count);

    const mock_mqtt_publish_t *status_pub = mock_mqtt_find_last_publish("_status/config");
    TEST_ASSERT_NOT_NULL(status_pub);
    TEST_ASSERT_EQUAL_STRING("homeassistant/sensor/myhome_doorbell_mqtt_front_door_status/config", status_pub->topic);

    cJSON *root = cJSON_Parse(status_pub->payload);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_STRING("Status (Front Door)", cJSON_GetObjectItem(root, "name")->valuestring);
    TEST_ASSERT_EQUAL_STRING("doorbell_mqtt_unifi_front_door_status", cJSON_GetObjectItem(root, "unique_id")->valuestring);
    TEST_ASSERT_EQUAL_STRING("sensor.doorbell_mqtt_unifi_status_front_door", cJSON_GetObjectItem(root, "default_entity_id")->valuestring);
    TEST_ASSERT_EQUAL_STRING("UniFi Doorbell MQTT Service (Front Door)", cJSON_GetObjectItem(cJSON_GetObjectItem(root, "device"), "name")->valuestring);
    cJSON_Delete(root);
}

void test_ha_status_publishers_and_error_dedup(void) {
    /* NULL guards */
    status_set_state(NULL);
    status_set_status_message(NULL, "msg");
    status_set_status_message("sub", NULL);
    status_set_last_download(NULL, "/p", "ts");
    status_set_validate_profile(NULL, "p", "h1", "h2", "ts");
    TEST_ASSERT_EQUAL_UINT(0, g_mock_mqtt.publish_count);

    status_set_state("uploading");
    TEST_ASSERT_EQUAL_STRING("uploading", mock_mqtt_find_last_publish("/status")->payload);

    status_set_status_message("custom/sub", "hello");
    TEST_ASSERT_EQUAL_STRING("hello", mock_mqtt_find_last_publish("/custom/sub")->payload);

    status_set_last_applied_profile("Christmas");
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/last_applied_profile")->payload);

    status_set_preset_selected("Christmas");
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/preset/selected")->payload);

    status_set_sfx_preset_selected("Chime");
    TEST_ASSERT_EQUAL_STRING("Chime", mock_mqtt_find_last_publish("/sfxpreset/selected")->payload);

    status_set_custom_directory("my_custom");
    TEST_ASSERT_EQUAL_STRING("my_custom", mock_mqtt_find_last_publish("/custom/directory")->payload);

    status_set_last_download("20260301_120000", "/profiles/downloads/20260301_120000", "2026-03-01T12:00:00Z");
    TEST_ASSERT_EQUAL_STRING("2026-03-01T12:00:00Z", mock_mqtt_find_last_publish("/last_download/time/state")->payload);
    cJSON *dl = cJSON_Parse(mock_mqtt_find_last_publish("/last_download/time/attributes")->payload);
    TEST_ASSERT_NOT_NULL(dl);
    TEST_ASSERT_EQUAL_STRING("20260301_120000", cJSON_GetObjectItem(dl, "directory")->valuestring);
    TEST_ASSERT_EQUAL_STRING("/profiles/downloads/20260301_120000", cJSON_GetObjectItem(dl, "full_path")->valuestring);
    cJSON_Delete(dl);

    status_set_validate_profile("match", "Christmas", "aaa", "aaa", "2026-03-01T12:00:00Z");
    TEST_ASSERT_EQUAL_STRING("match", mock_mqtt_find_last_publish("/profile_validation")->payload);
    cJSON *val = cJSON_Parse(mock_mqtt_find_last_publish("/profile_validation/attributes")->payload);
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("Christmas", cJSON_GetObjectItem(val, "profile")->valuestring);
    TEST_ASSERT_EQUAL_STRING("aaa", cJSON_GetObjectItem(val, "expected_hash")->valuestring);
    TEST_ASSERT_EQUAL_STRING("aaa", cJSON_GetObjectItem(val, "applied_hash")->valuestring);
    cJSON_Delete(val);

    status_set_availability(true);
    TEST_ASSERT_EQUAL_STRING("online", mock_mqtt_find_last_publish("/availability")->payload);
    status_set_availability(false);
    TEST_ASSERT_EQUAL_STRING("offline", mock_mqtt_find_last_publish("/availability")->payload);

    /* Error deduplication, set_error, clear_error, HA_ERR, HA_ERRF */
    status_clear_error();
    TEST_ASSERT_FALSE(status_error_changed(ERROR_SSH_AUTH_FAILED, NULL));
    TEST_ASSERT_TRUE(status_error_changed(ERROR_SSH_AUTH_FAILED, "bad pass"));
    TEST_ASSERT_FALSE(status_error_changed(ERROR_SSH_AUTH_FAILED, "bad pass"));
    TEST_ASSERT_TRUE(status_error_changed(ERROR_SSH_AUTH_FAILED, "other msg"));

    status_set_error(ERROR_SSH_AUTH_FAILED, NULL); /* uses default msg */
    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_AUTH_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    TEST_ASSERT_EQUAL_STRING("ERROR_SSH_AUTH_FAILED", cJSON_GetObjectItem(err, "name")->valuestring);
    TEST_ASSERT_EQUAL_STRING("SSH authentication failed", cJSON_GetObjectItem(err, "message")->valuestring);
    cJSON_Delete(err);

    HA_ERR(ERROR_PROFILE_NOT_FOUND, "missing folder");
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_STRING("Profile folder not found: missing folder", cJSON_GetObjectItem(err, "message")->valuestring);
    cJSON_Delete(err);

    HA_ERRF(ERROR_PROFILE_INVALID, "bad profile %s", "xmas");
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_STRING("bad profile xmas", cJSON_GetObjectItem(err, "message")->valuestring);
    cJSON_Delete(err);

    status_clear_error();
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, cJSON_GetObjectItem(err, "code")->valueint);
    TEST_ASSERT_EQUAL_STRING("", cJSON_GetObjectItem(err, "message")->valuestring);
    cJSON_Delete(err);
}

void test_ha_mqtt_bind_connect_restore_and_disconnect(void) {
    TEST_ASSERT_TRUE(ha_mqtt_bind(&g_cfg, g_tmp_dir));

    TEST_ASSERT_TRUE(g_mock_mqtt.lwt_set);
    TEST_ASSERT_EQUAL_STRING("chrishansentech/doorbell-mqtt/default/availability", g_mock_mqtt.lwt_topic);
    TEST_ASSERT_EQUAL_STRING("offline", g_mock_mqtt.lwt_payload);
    TEST_ASSERT_NOT_NULL(g_mock_mqtt.on_connect_fn);
    TEST_ASSERT_NOT_NULL(g_mock_mqtt.on_disconnect_fn);

    /* Connect with no last_applied.json -> publishes discovery, subscribes, availability online */
    g_mock_mqtt.on_connect_fn(false, g_mock_mqtt.on_connect_user);
    TEST_ASSERT_EQUAL_UINT(7, g_mock_mqtt.subscribe_count);
    TEST_ASSERT_EQUAL_STRING("online", mock_mqtt_find_last_publish("/availability")->payload);
    TEST_ASSERT_NULL(mock_mqtt_find_last_publish("/last_applied_profile"));

    /* Save preset state and reconnect -> restores last_applied_profile & preset/selected */
    profile_state_t ps = {0};
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, state_save(&ps, "Christmas", true, UNIFI_APPLY_IPC, g_tmp_dir));
    mock_mqtt_reset();
    TEST_ASSERT_TRUE(ha_mqtt_bind(&g_cfg, g_tmp_dir));
    g_mock_mqtt.on_connect_fn(true, g_mock_mqtt.on_connect_user);
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/preset/selected")->payload);
    TEST_ASSERT_EQUAL_STRING("", mock_mqtt_find_last_publish("/custom/directory")->payload);

    /* Save non-preset state and reconnect -> sets preset/selected to "none" */
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, state_save(&ps, "custom_theme", false, UNIFI_APPLY_IPC, g_tmp_dir));
    mock_mqtt_reset();
    TEST_ASSERT_TRUE(ha_mqtt_bind(&g_cfg, g_tmp_dir));
    g_mock_mqtt.on_connect_fn(true, g_mock_mqtt.on_connect_user);
    TEST_ASSERT_EQUAL_STRING("custom_theme", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
    TEST_ASSERT_EQUAL_STRING("none", mock_mqtt_find_last_publish("/preset/selected")->payload);

    /* Disconnect publishes offline */
    g_mock_mqtt.on_disconnect_fn(g_mock_mqtt.on_disconnect_user);
    TEST_ASSERT_EQUAL_STRING("offline", mock_mqtt_find_last_publish("/availability")->payload);
}

void test_ha_discovery_virtual_default_preset_option_placement_and_visibility(void) {
    /* 1. Enabled with both default assets -> Default placed first before user presets */
    g_cfg.default_assets_cfg.enabled = true;
    g_cfg.default_assets_cfg.welcome.enabled = true;
    g_cfg.default_assets_cfg.ring_button.enabled = true;

    mock_mqtt_reset();
    TEST_ASSERT_TRUE(ha_publish_discovery(&g_cfg));
    const mock_mqtt_publish_t *preset_pub = mock_mqtt_find_last_publish("_preset/config");
    TEST_ASSERT_NOT_NULL(preset_pub);
    cJSON *root = cJSON_Parse(preset_pub->payload);
    TEST_ASSERT_NOT_NULL(root);
    cJSON *opts = cJSON_GetObjectItem(root, "options");
    TEST_ASSERT_EQUAL_INT(3, cJSON_GetArraySize(opts));
    TEST_ASSERT_EQUAL_STRING("Default", cJSON_GetArrayItem(opts, 0)->valuestring);
    TEST_ASSERT_EQUAL_STRING("Christmas", cJSON_GetArrayItem(opts, 1)->valuestring);
    TEST_ASSERT_EQUAL_STRING("Labor Day", cJSON_GetArrayItem(opts, 2)->valuestring);
    cJSON_Delete(root);

    /* 2. Enabled with only welcome asset -> Default still advertised */
    g_cfg.default_assets_cfg.ring_button.enabled = false;
    mock_mqtt_reset();
    TEST_ASSERT_TRUE(ha_publish_discovery(&g_cfg));
    root = cJSON_Parse(mock_mqtt_find_last_publish("_preset/config")->payload);
    opts = cJSON_GetObjectItem(root, "options");
    TEST_ASSERT_EQUAL_INT(3, cJSON_GetArraySize(opts));
    TEST_ASSERT_EQUAL_STRING("Default", cJSON_GetArrayItem(opts, 0)->valuestring);
    cJSON_Delete(root);

    /* 3. Enabled globally but neither asset enabled -> Default NOT advertised */
    g_cfg.default_assets_cfg.welcome.enabled = false;
    mock_mqtt_reset();
    TEST_ASSERT_TRUE(ha_publish_discovery(&g_cfg));
    root = cJSON_Parse(mock_mqtt_find_last_publish("_preset/config")->payload);
    opts = cJSON_GetObjectItem(root, "options");
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetArraySize(opts));
    TEST_ASSERT_EQUAL_STRING("Christmas", cJSON_GetArrayItem(opts, 0)->valuestring);
    cJSON_Delete(root);

    /* 4. No user-defined presets + default ring enabled -> options has ["Default"] */
    for (size_t i = 0; i < g_cfg.preset_cfg.count; i++) {
        free(g_cfg.preset_cfg.items[i].display_name);
        free(g_cfg.preset_cfg.items[i].key_name);
        free(g_cfg.preset_cfg.items[i].directory);
    }
    free(g_cfg.preset_cfg.items);
    g_cfg.preset_cfg.items = NULL;
    g_cfg.preset_cfg.count = 0;
    g_cfg.default_assets_cfg.ring_button.enabled = true;

    mock_mqtt_reset();
    TEST_ASSERT_TRUE(ha_publish_discovery(&g_cfg));
    root = cJSON_Parse(mock_mqtt_find_last_publish("_preset/config")->payload);
    opts = cJSON_GetObjectItem(root, "options");
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(opts));
    TEST_ASSERT_EQUAL_STRING("Default", cJSON_GetArrayItem(opts, 0)->valuestring);
    cJSON_Delete(root);

    /* 5. If "none" is first option in presets, Default is placed immediately after "none" */
    g_cfg.preset_cfg.count = 2;
    g_cfg.preset_cfg.items = calloc(2, sizeof(*g_cfg.preset_cfg.items));
    g_cfg.preset_cfg.items[0].display_name = strdup("none");
    g_cfg.preset_cfg.items[0].key_name = strdup("none");
    g_cfg.preset_cfg.items[0].directory = strdup("none");
    g_cfg.preset_cfg.items[1].display_name = strdup("Christmas");
    g_cfg.preset_cfg.items[1].key_name = strdup("christmas");
    g_cfg.preset_cfg.items[1].directory = strdup("christmas");

    mock_mqtt_reset();
    TEST_ASSERT_TRUE(ha_publish_discovery(&g_cfg));
    root = cJSON_Parse(mock_mqtt_find_last_publish("_preset/config")->payload);
    opts = cJSON_GetObjectItem(root, "options");
    TEST_ASSERT_EQUAL_INT(3, cJSON_GetArraySize(opts));
    TEST_ASSERT_EQUAL_STRING("none", cJSON_GetArrayItem(opts, 0)->valuestring);
    TEST_ASSERT_EQUAL_STRING("Default", cJSON_GetArrayItem(opts, 1)->valuestring);
    TEST_ASSERT_EQUAL_STRING("Christmas", cJSON_GetArrayItem(opts, 2)->valuestring);
    cJSON_Delete(root);

    /* 6. Restart/reconnect after applying virtual Default restores Default state cleanly */
    profile_state_t ps = {0};
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, state_save_ex(&ps, "Default", true, true, UNIFI_APPLY_IPC, g_tmp_dir));
    mock_mqtt_reset();
    TEST_ASSERT_TRUE(ha_mqtt_bind(&g_cfg, g_tmp_dir));
    g_mock_mqtt.on_connect_fn(false, g_mock_mqtt.on_connect_user);
    TEST_ASSERT_EQUAL_STRING("Default", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
    TEST_ASSERT_EQUAL_STRING("Default", mock_mqtt_find_last_publish("/preset/selected")->payload);
    TEST_ASSERT_EQUAL_STRING("", mock_mqtt_find_last_publish("/custom/directory")->payload);
}

static void replace_presets_with_generated(config_t *cfg, size_t count, const char *prefix) {
    for (size_t i = 0; i < cfg->preset_cfg.count; i++) {
        free(cfg->preset_cfg.items[i].display_name);
        free(cfg->preset_cfg.items[i].key_name);
        free(cfg->preset_cfg.items[i].directory);
    }
    free(cfg->preset_cfg.items);
    cfg->preset_cfg.items = calloc(count, sizeof(*cfg->preset_cfg.items));
    TEST_ASSERT_NOT_NULL(cfg->preset_cfg.items);
    cfg->preset_cfg.count = count;

    for (size_t i = 0; i < count; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s_%03zu", prefix, i);
        cfg->preset_cfg.items[i].display_name = strdup(buf);
        cfg->preset_cfg.items[i].key_name = strdup(buf);
        cfg->preset_cfg.items[i].directory = strdup(buf);
    }
}

void test_ha_discovery_large_payload_over_1024_and_overflow_detection(void) {
    /* 1. Payload exceeding the original 1024-byte buffer succeeds within 4096 bytes */
    g_cfg.default_assets_cfg.enabled = true;
    g_cfg.default_assets_cfg.welcome.enabled = true;
    replace_presets_with_generated(&g_cfg, 45, "Seasonal_Holiday_Theme");

    mock_mqtt_reset();
    TEST_ASSERT_TRUE(ha_publish_discovery(&g_cfg));
    const mock_mqtt_publish_t *preset_pub = mock_mqtt_find_last_publish("_preset/config");
    TEST_ASSERT_NOT_NULL(preset_pub);
    TEST_ASSERT_TRUE(strlen(preset_pub->payload) > 1024);

    cJSON *root = cJSON_Parse(preset_pub->payload);
    TEST_ASSERT_NOT_NULL(root);
    cJSON *opts = cJSON_GetObjectItem(root, "options");
    TEST_ASSERT_TRUE(cJSON_IsArray(opts));
    TEST_ASSERT_EQUAL_INT(46, cJSON_GetArraySize(opts));
    TEST_ASSERT_EQUAL_STRING("Default", cJSON_GetArrayItem(opts, 0)->valuestring);
    TEST_ASSERT_EQUAL_STRING("Seasonal_Holiday_Theme_000", cJSON_GetArrayItem(opts, 1)->valuestring);
    TEST_ASSERT_EQUAL_STRING("Seasonal_Holiday_Theme_044", cJSON_GetArrayItem(opts, 45)->valuestring);
    cJSON_Delete(root);

    /* 2. Payload exceeding the 4096-byte buffer is detected, returns false, and is NOT published truncated */
    replace_presets_with_generated(&g_cfg, 200, "Very_Long_Seasonal_Preset_Display_Name");

    mock_mqtt_reset();
    TEST_ASSERT_FALSE(ha_publish_discovery(&g_cfg));
    TEST_ASSERT_NULL(mock_mqtt_find_last_publish("_preset/config"));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_ha_topics_build_and_subscribe_commands);
    RUN_TEST(test_ha_entities_metadata_consistency);
    RUN_TEST(test_ha_discovery_default_instance_publishes_all_entities);
    RUN_TEST(test_ha_discovery_custom_instance_namespacing);
    RUN_TEST(test_ha_status_publishers_and_error_dedup);
    RUN_TEST(test_ha_mqtt_bind_connect_restore_and_disconnect);
    RUN_TEST(test_ha_discovery_virtual_default_preset_option_placement_and_visibility);
    RUN_TEST(test_ha_discovery_large_payload_over_1024_and_overflow_detection);

    return UNITY_END();
}
