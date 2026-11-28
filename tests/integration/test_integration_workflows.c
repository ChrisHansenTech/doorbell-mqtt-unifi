#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "mocks/mock_mqtt.h"
#include "mocks/mock_ssh.h"
#include "support/test_helpers.h"
#include "cJSON.h"
#include "config.h"
#include "errors.h"
#include "ha_mqtt.h"
#include "ha_topics.h"
#include "migration.h"
#include "mqtt_router.h"
#include "mqtt_router_types.h"
#include "unifi_profile_json.h"
#include "unifi_profiles_repo.h"
#include "utils.h"

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char g_orig_cwd[PATH_MAX];
static char g_tmp_dir[128];
static char g_config_path[256];
static char g_profiles_dir[256];
static char g_sounds_dir[256];
static char g_state_dir[256];

static config_t g_cfg;
static sfx_ctx_t g_sfx_ctx;
static mqtt_router_ctx_t g_router_ctx;
static bool g_routes_registered = false;

/* Simulated in-memory doorbell device state */
static char g_device_lcm_json[1024];
static char g_device_snd_json[1024];

static bool sim_doorbell_exec(
    const char *command,
    int *exit_status_out,
    char **stdout_data,
    size_t *stdout_len,
    char **stderr_data,
    size_t *stderr_len,
    size_t call_index
) {
    (void)stderr_data;
    (void)stderr_len;
    (void)call_index;

    if (exit_status_out) *exit_status_out = 0;

    if (strstr(command, "-T=ubnt_lcm_gui") && strstr(command, "-M=")) {
        char resp[1200];
        snprintf(resp, sizeof(resp), "{\"response\":{\"payload\":{\"customAnimations\":%s}}}", g_device_lcm_json);
        if (stdout_data) {
            *stdout_data = strdup(resp);
            if (stdout_len) *stdout_len = strlen(*stdout_data);
        }
        return true;
    }

    if (strstr(command, "-T=ubnt_sounds_leds") && strstr(command, "-M=")) {
        char resp[1200];
        snprintf(resp, sizeof(resp), "{\"response\":{\"payload\":{\"customSounds\":%s}}}", g_device_snd_json);
        if (stdout_data) {
            *stdout_data = strdup(resp);
            if (stdout_len) *stdout_len = strlen(*stdout_data);
        }
        return true;
    }

    if (strstr(command, "-T=ubnt_lcm_gui") && strstr(command, "-n=ChangeLcmGuiSettings")) {
        if (stdout_data) {
            *stdout_data = strdup(g_device_lcm_json);
            if (stdout_len) *stdout_len = strlen(*stdout_data);
        }
        return true;
    }

    if (strstr(command, "-T=ubnt_sounds_leds") && strstr(command, "-n=ChangeSoundLedSettings")) {
        if (stdout_data) {
            *stdout_data = strdup(g_device_snd_json);
            if (stdout_len) *stdout_len = strlen(*stdout_data);
        }
        return true;
    }

    return true;
}

static bool sim_doorbell_upload(const char *local_path, const char *remote_dir, unsigned long remote_mode, size_t call_index) {
    (void)remote_dir;
    (void)remote_mode;
    (void)call_index;

    /* Capture uploaded IPC payload JSONs as the doorbell's active in-memory config! */
    if (strstr(local_path, "lcm_ipc_msg.json")) {
        char *text = NULL;
        if (utils_read_file(local_path, &text, NULL)) {
            cJSON *root = cJSON_Parse(text);
            if (root) {
                cJSON *arr = cJSON_GetObjectItem(root, "customAnimations");
                char *printed = cJSON_PrintUnformatted(arr);
                if (printed) {
                    snprintf(g_device_lcm_json, sizeof(g_device_lcm_json), "%s", printed);
                    cJSON_free(printed);
                }
                cJSON_Delete(root);
            }
            free(text);
        }
    } else if (strstr(local_path, "sounds_ipc_msg.json")) {
        char *text = NULL;
        if (utils_read_file(local_path, &text, NULL)) {
            cJSON *root = cJSON_Parse(text);
            if (root) {
                cJSON *arr = cJSON_GetObjectItem(root, "customSounds");
                char *printed = cJSON_PrintUnformatted(arr);
                if (printed) {
                    snprintf(g_device_snd_json, sizeof(g_device_snd_json), "%s", printed);
                    cJSON_free(printed);
                }
                cJSON_Delete(root);
            }
            free(text);
        }
    }
    return true;
}

static bool sim_doorbell_download(const char *remote_path, const char *local_path, size_t call_index) {
    (void)call_index;
    if (strstr(remote_path, "ubnt_lcm_gui.conf")) {
        return test_write_text(
            local_path,
            "{\"customAnimations\":[{\"guiId\":\"WELCOME\",\"file\":\"captured.png\",\"count\":20,\"durationMs\":1200,\"enable\":true}]}"
        );
    }
    if (strstr(remote_path, "ubnt_sounds_leds.conf")) {
        return test_write_text(
            local_path,
            "{\"customSounds\":[{\"soundStateName\":\"RING_BUTTON_PRESSED\",\"file\":\"captured.ogg\",\"repeatTimes\":1,\"volume\":90,\"enable\":true}]}"
        );
    }
    if (strstr(remote_path, ".anim")) {
        return test_write_minimal_png(local_path);
    }
    return test_write_minimal_ogg(local_path);
}

static void dispatch_mqtt_command(const char *cmd_subtopic, const char *payload) {
    char topic[256];
    ha_build_topic(topic, sizeof(topic), cmd_subtopic);

    TEST_ASSERT_TRUE(mqtt_router_start(&g_router_ctx));
    TEST_ASSERT_EQUAL_INT(0, mqtt_router_enqueue(topic, 0, payload, strlen(payload)));
    /* Stopping drains all queued messages deterministically before returning */
    mqtt_router_stop();
}

void setUp(void) {
    TEST_ASSERT_NOT_NULL(getcwd(g_orig_cwd, sizeof(g_orig_cwd)));
    TEST_ASSERT_TRUE(test_make_temp_dir(g_tmp_dir, sizeof(g_tmp_dir)));

    snprintf(g_config_path, sizeof(g_config_path), "%s/config.json", g_tmp_dir);
    snprintf(g_profiles_dir, sizeof(g_profiles_dir), "%s/profiles", g_tmp_dir);
    snprintf(g_sounds_dir, sizeof(g_sounds_dir), "%s/sounds", g_tmp_dir);
    snprintf(g_state_dir, sizeof(g_state_dir), "%s/state", g_tmp_dir);

    TEST_ASSERT_TRUE(utils_create_directory(g_profiles_dir));
    TEST_ASSERT_TRUE(utils_create_directory(g_sounds_dir));
    TEST_ASSERT_TRUE(utils_create_directory(g_state_dir));

    /* Write integration config */
    TEST_ASSERT_TRUE(test_write_text(
        g_config_path,
        "{\n"
        "  \"mqtt\": { \"host\": \"127.0.0.1\", \"port\": 1883, \"prefix\": \"integ\", \"instance\": \"front_door\" },\n"
        "  \"ssh\": { \"host\": \"192.168.1.50\", \"port\": 22, \"username\": \"ubnt\" },\n"
        "  \"unifi\": { \"apply_method\": \"ipc\" },\n"
        "  \"presets\": [ { \"name\": \"Christmas\", \"directory\": \"christmas\" } ],\n"
        "  \"sfx\": { \"defaultVolume\": 80, \"presets\": [ { \"name\": \"Door Chime\", \"file\": \"chime.ogg\" } ] }\n"
        "}\n"
    ));

    char xmas_dir[PATH_MAX];
    snprintf(xmas_dir, sizeof(xmas_dir), "%s/christmas", g_profiles_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(xmas_dir, "xmas.png", "xmas.ogg", true, true));

    char sfx_file[PATH_MAX];
    snprintf(sfx_file, sizeof(sfx_file), "%s/chime.ogg", g_sounds_dir);
    TEST_ASSERT_TRUE(test_write_minimal_ogg(sfx_file));

    memset(&g_cfg, 0, sizeof(g_cfg));
    TEST_ASSERT_TRUE(config_load(g_config_path, &g_cfg));
    TEST_ASSERT_TRUE(profiles_repo_init(g_profiles_dir, &g_cfg.preset_cfg));
    ha_topics_init(&g_cfg);

    g_sfx_ctx.sfx_preset_cfg = &g_cfg.sfx_preset_cfg;
    g_sfx_ctx.sounds_dir = g_sounds_dir;

    g_router_ctx.ssh_cfg = &g_cfg.ssh_cfg;
    g_router_ctx.preset_cfg = &g_cfg.preset_cfg;
    g_router_ctx.unifi_cfg = &g_cfg.unifi_cfg;
    g_router_ctx.default_assets_cfg = &g_cfg.default_assets_cfg;
    g_router_ctx.sfx_ctx = &g_sfx_ctx;
    g_router_ctx.state_dir = g_state_dir;

    if (!g_routes_registered) {
        ha_routes_register_commands();
        g_routes_registered = true;
    }

    mock_mqtt_reset();
    mock_ssh_reset();
    g_mock_ssh.exec_cb = sim_doorbell_exec;
    g_mock_ssh.upload_cb = sim_doorbell_upload;
    g_mock_ssh.download_cb = sim_doorbell_download;

    snprintf(g_device_lcm_json, sizeof(g_device_lcm_json), "[]");
    snprintf(g_device_snd_json, sizeof(g_device_snd_json), "[]");

    TEST_ASSERT_EQUAL_INT(0, chdir(g_tmp_dir));
}

void tearDown(void) {
    mqtt_router_stop();
    (void)chdir(g_orig_cwd);
    profiles_repo_shutdown();
    config_free(&g_cfg);
    test_remove_dir_recursive(g_tmp_dir);
}

void test_integration_apply_validate_drift_reapply_lifecycle(void) {
    /* 1. Apply Christmas preset over MQTT command topic */
    dispatch_mqtt_command("cmd/preset_set", "Christmas");

    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/preset/selected")->payload);
    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);

    /* 2. Validate profile -> must report "match" because simulated doorbell kept uploaded IPC payload */
    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/validate_profile", "PRESS");
    TEST_ASSERT_EQUAL_STRING("match", mock_mqtt_find_last_publish("/profile_validation")->payload);

    /* 3. Simulate doorbell reboot/drift clearing in-memory animations */
    snprintf(g_device_lcm_json, sizeof(g_device_lcm_json), "[]");
    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/validate_profile", "PRESS");
    TEST_ASSERT_EQUAL_STRING("mismatch", mock_mqtt_find_last_publish("/profile_validation")->payload);

    /* 4. Reapply last profile -> restores state, subsequent validate matches again */
    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/reapply_last_profile", "PRESS");
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/last_applied_profile")->payload);

    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/validate_profile", "PRESS");
    TEST_ASSERT_EQUAL_STRING("match", mock_mqtt_find_last_publish("/profile_validation")->payload);
}

void test_integration_download_assets_creates_loadable_profile(void) {
    dispatch_mqtt_command("cmd/download_assets", "PRESS");

    const mock_mqtt_publish_t *attrs_pub = mock_mqtt_find_last_publish("/last_download/time/attributes");
    TEST_ASSERT_NOT_NULL(attrs_pub);

    cJSON *attrs = cJSON_Parse(attrs_pub->payload);
    TEST_ASSERT_NOT_NULL(attrs);
    const char *full_path = cJSON_GetObjectItem(attrs, "full_path")->valuestring;
    TEST_ASSERT_NOT_NULL(full_path);

    /* Generated profile directory must load cleanly via unifi_profile_load_from_file */
    unifi_profile_t downloaded = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(full_path, &downloaded));
    TEST_ASSERT_EQUAL_STRING("captured.png", downloaded.welcome.file);
    TEST_ASSERT_EQUAL_STRING("captured.ogg", downloaded.ring_button.file);

    cJSON_Delete(attrs);
}

void test_integration_sfx_playback_over_mqtt_router(void) {
    dispatch_mqtt_command("cmd/sfx_preset_play", "Door Chime");

    TEST_ASSERT_EQUAL_STRING("none", mock_mqtt_find_last_publish("/sfxpreset/selected")->payload);
    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_ssh.upload_calls);

    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
}

void test_integration_legacy_migration_and_ha_connect_state_restore(void) {
    /* Place a legacy state file in profiles/.state/last_applied.json */
    char legacy_dir[512];
    char legacy_file[PATH_MAX];
    snprintf(legacy_dir, sizeof(legacy_dir), "%s/.state", g_profiles_dir);
    snprintf(legacy_file, sizeof(legacy_file), "%s/last_applied.json", legacy_dir);

    TEST_ASSERT_TRUE(utils_create_directory(legacy_dir));
    TEST_ASSERT_TRUE(test_write_text(
        legacy_file,
        "{\"schemaVersion\":1,\"profileName\":\"Christmas\",\"isPreset\":true,\"applyMethod\":\"ipc\",\"appliedAt\":1700000000,\"hash\":\"abc\"}"
    ));

    /* Run migration then bind HA MQTT and simulate broker connect */
    migration_run(g_profiles_dir, g_sounds_dir, g_state_dir);
    TEST_ASSERT_FALSE(utils_file_exists(legacy_file));

    TEST_ASSERT_TRUE(ha_mqtt_bind(&g_cfg, g_state_dir));
    g_mock_mqtt.on_connect_fn(false, g_mock_mqtt.on_connect_user);

    TEST_ASSERT_EQUAL_STRING("online", mock_mqtt_find_last_publish("/availability")->payload);
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/preset/selected")->payload);
    TEST_ASSERT_EQUAL_UINT(7, g_mock_mqtt.subscribe_count);
}

void test_integration_ssh_timeout_error_propagation_and_recovery(void) {
    /* 1. Simulate TCP connect timeout during preset apply over MQTT router */
    g_mock_ssh.create_succeeds = false;
    g_mock_ssh.create_error = error_create(
        ERROR_SSH_CONNECTION_TIMEOUT,
        "TCP connection to 192.168.1.50:22 timed out after 10s"
    );

    dispatch_mqtt_command("cmd/preset_set", "Christmas");

    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);
    TEST_ASSERT_NULL(mock_mqtt_find_last_publish("/last_applied_profile"));
    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_TIMEOUT, cJSON_GetObjectItem(err, "code")->valueint);
    TEST_ASSERT_NOT_NULL(strstr(cJSON_GetObjectItem(err, "message")->valuestring, "timed out after 10s"));
    cJSON_Delete(err);

    /* 2. Doorbell becomes reachable again -> retry succeeds without restart */
    mock_mqtt_reset();
    g_mock_ssh.create_succeeds = true;
    dispatch_mqtt_command("cmd/preset_set", "Christmas");

    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);

    /* 3. Subsequent validate confirms match */
    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/validate_profile", "PRESS");
    TEST_ASSERT_EQUAL_STRING("match", mock_mqtt_find_last_publish("/profile_validation")->payload);
}

void test_integration_default_assets_seasonal_switch_validate_and_reapply_lifecycle(void) {
    /* Configure default assets directory and assets */
    char def_dir[512];
    snprintf(def_dir, sizeof(def_dir), "%s/default-assets", g_profiles_dir);
    TEST_ASSERT_TRUE(utils_create_directory(def_dir));

    char fpath[PATH_MAX];
    snprintf(fpath, sizeof(fpath), "%s/def-welcome.png", def_dir);
    TEST_ASSERT_TRUE(test_write_minimal_png(fpath));
    snprintf(fpath, sizeof(fpath), "%s/def-ring.ogg", def_dir);
    TEST_ASSERT_TRUE(test_write_minimal_ogg(fpath));

    g_cfg.default_assets_cfg.enabled = true;
    snprintf(g_cfg.default_assets_cfg.directory, sizeof(g_cfg.default_assets_cfg.directory), "%s", def_dir);
    g_cfg.default_assets_cfg.welcome.enabled = true;
    snprintf(g_cfg.default_assets_cfg.welcome.file, sizeof(g_cfg.default_assets_cfg.welcome.file), "def-welcome.png");
    g_cfg.default_assets_cfg.welcome.count = 1;
    g_cfg.default_assets_cfg.welcome.duration_ms = 3000;
    g_cfg.default_assets_cfg.ring_button.enabled = true;
    snprintf(g_cfg.default_assets_cfg.ring_button.file, sizeof(g_cfg.default_assets_cfg.ring_button.file), "def-ring.ogg");
    g_cfg.default_assets_cfg.ring_button.repeat_times = 1;
    g_cfg.default_assets_cfg.ring_button.volume = 88;

    /* Make Christmas image-only (ringButton disabled) so default ring sound is substituted */
    char xmas_dir[PATH_MAX];
    snprintf(xmas_dir, sizeof(xmas_dir), "%s/christmas", g_profiles_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(xmas_dir, "xmas.png", "xmas.ogg", true, false));

    /* 1. Apply seasonal Christmas profile -> combines xmas.png + def-ring.ogg, validates as match */
    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/preset_set", "Christmas");
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);
    TEST_ASSERT_NOT_NULL(strstr(g_device_lcm_json, "xmas.png"));
    TEST_ASSERT_NOT_NULL(strstr(g_device_snd_json, "def-ring.ogg"));

    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/validate_profile", "PRESS");
    TEST_ASSERT_EQUAL_STRING("match", mock_mqtt_find_last_publish("/profile_validation")->payload);

    /* 2. Switch from seasonal profile to virtual Default via MQTT preset command */
    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/preset_set", "Default");
    TEST_ASSERT_EQUAL_STRING("Default", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
    TEST_ASSERT_EQUAL_STRING("Default", mock_mqtt_find_last_publish("/preset/selected")->payload);
    TEST_ASSERT_NOT_NULL(strstr(g_device_lcm_json, "def-welcome.png"));
    TEST_ASSERT_NOT_NULL(strstr(g_device_snd_json, "def-ring.ogg"));

    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/validate_profile", "PRESS");
    TEST_ASSERT_EQUAL_STRING("match", mock_mqtt_find_last_publish("/profile_validation")->payload);

    /* 3. Update default welcome config and reapply -> updated asset applied and validates as match */
    snprintf(fpath, sizeof(fpath), "%s/def-welcome-v2.png", def_dir);
    TEST_ASSERT_TRUE(test_write_minimal_png(fpath));
    snprintf(g_cfg.default_assets_cfg.welcome.file, sizeof(g_cfg.default_assets_cfg.welcome.file), "def-welcome-v2.png");

    /* Validate BEFORE reapply still matches persisted state (does not recompute from new config) */
    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/validate_profile", "PRESS");
    TEST_ASSERT_EQUAL_STRING("match", mock_mqtt_find_last_publish("/profile_validation")->payload);

    /* Now reapply -> applies v2 and updates persisted state */
    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/reapply_last_profile", "PRESS");
    TEST_ASSERT_EQUAL_STRING("Default", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
    TEST_ASSERT_NOT_NULL(strstr(g_device_lcm_json, "def-welcome-v2.png"));

    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/validate_profile", "PRESS");
    TEST_ASSERT_EQUAL_STRING("match", mock_mqtt_find_last_publish("/profile_validation")->payload);

    /* 4. Existing SFX playback and Asset download remain completely unaffected */
    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/sfx_preset_play", "Door Chime");
    TEST_ASSERT_EQUAL_STRING("none", mock_mqtt_find_last_publish("/sfxpreset/selected")->payload);
    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);

    mock_mqtt_reset();
    dispatch_mqtt_command("cmd/download_assets", "PRESS");
    TEST_ASSERT_NOT_NULL(mock_mqtt_find_last_publish("/last_download/time/attributes"));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_integration_apply_validate_drift_reapply_lifecycle);
    RUN_TEST(test_integration_download_assets_creates_loadable_profile);
    RUN_TEST(test_integration_sfx_playback_over_mqtt_router);
    RUN_TEST(test_integration_legacy_migration_and_ha_connect_state_restore);
    RUN_TEST(test_integration_ssh_timeout_error_propagation_and_recovery);
    RUN_TEST(test_integration_default_assets_seasonal_switch_validate_and_reapply_lifecycle);

    return UNITY_END();
}
