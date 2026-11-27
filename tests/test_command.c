#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "mocks/mock_mqtt.h"
#include "mocks/mock_ssh.h"
#include "support/test_helpers.h"
#include "cJSON.h"
#include "command.h"
#include "config.h"
#include "errors.h"
#include "ha_topics.h"
#include "state.h"
#include "unifi_profiles_repo.h"
#include "utils.h"

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char g_orig_cwd[PATH_MAX];
static char g_tmp_dir[128];
static char g_profiles_dir[256];
static char g_sounds_dir[256];
static char g_state_dir[256];

static config_t g_cfg;
static sfx_ctx_t g_sfx_ctx;
static mqtt_router_ctx_t g_ctx;

static const char *g_ipc_apply_lcm =
    "{\"response\":{\"payload\":{\"customAnimations\":[{\"guiId\":\"WELCOME\",\"file\":\"anim.png\",\"count\":10,\"durationMs\":1000,\"enable\":true,\"loop\":true}]}}}";
static const char *g_ipc_apply_snd =
    "{\"response\":{\"payload\":{\"customSounds\":[{\"soundStateName\":\"RING_BUTTON_PRESSED\",\"file\":\"ring.ogg\",\"repeatTimes\":1,\"volume\":85,\"enable\":true}]}}}";
static const char *g_ipc_val_lcm =
    "[{\"guiId\":\"WELCOME\",\"file\":\"anim.png\",\"count\":10,\"durationMs\":1000,\"enable\":true,\"loop\":true}]";
static const char *g_ipc_val_snd =
    "[{\"soundStateName\":\"RING_BUTTON_PRESSED\",\"file\":\"ring.ogg\",\"repeatTimes\":1,\"volume\":85,\"enable\":true}]";

static bool g_fail_download = false;
static bool g_fail_fetch = false;
static int g_cmd_lcm_ipc_rc = 0;
static int g_cmd_apply_script_exit = 0;
static const char *g_cmd_apply_script_stderr = NULL;
static size_t g_cmd_fail_upload_idx = 999;

static bool cmd_exec_cb(
    const char *command,
    int *exit_status_out,
    char **stdout_data,
    size_t *stdout_len,
    char **stderr_data,
    size_t *stderr_len,
    size_t call_index
) {
    (void)call_index;

    if (exit_status_out) *exit_status_out = 0;

    if (strstr(command, "set -eu")) {
        if (exit_status_out) *exit_status_out = g_cmd_apply_script_exit;
        if (stderr_data && g_cmd_apply_script_stderr) {
            *stderr_data = strdup(g_cmd_apply_script_stderr);
            if (stderr_len) *stderr_len = strlen(*stderr_data);
        }
        return true;
    }
    if (strstr(command, "-T=ubnt_lcm_gui") && strstr(command, "-M=")) {
        if (exit_status_out) *exit_status_out = g_cmd_lcm_ipc_rc;
        if (stdout_data) {
            *stdout_data = strdup(g_ipc_apply_lcm);
            if (stdout_len) *stdout_len = strlen(*stdout_data);
        }
        return true;
    }
    if (strstr(command, "-T=ubnt_sounds_leds") && strstr(command, "-M=")) {
        if (stdout_data) {
            *stdout_data = strdup(g_ipc_apply_snd);
            if (stdout_len) *stdout_len = strlen(*stdout_data);
        }
        return true;
    }
    if (strstr(command, "-T=ubnt_lcm_gui") && strstr(command, "-n=ChangeLcmGuiSettings")) {
        if (g_fail_fetch) return false;
        if (stdout_data) {
            *stdout_data = strdup(g_ipc_val_lcm);
            if (stdout_len) *stdout_len = strlen(*stdout_data);
        }
        return true;
    }
    if (strstr(command, "-T=ubnt_sounds_leds") && strstr(command, "-n=ChangeSoundLedSettings")) {
        if (g_fail_fetch) return false;
        if (stdout_data) {
            *stdout_data = strdup(g_ipc_val_snd);
            if (stdout_len) *stdout_len = strlen(*stdout_data);
        }
        return true;
    }
    return true;
}

static bool cmd_upload_cb(const char *local_path, const char *remote_dir, unsigned long remote_mode, size_t call_index) {
    (void)local_path;
    (void)remote_dir;
    (void)remote_mode;
    return call_index != g_cmd_fail_upload_idx;
}

static bool cmd_download_cb(const char *remote_path, const char *local_path, size_t call_index) {
    (void)call_index;
    if (g_fail_download) return false;
    if (strstr(remote_path, "ubnt_lcm_gui.conf")) {
        return test_write_text(local_path, "{\"customAnimations\":[{\"guiId\":\"WELCOME\",\"file\":\"dl.png\",\"count\":10,\"durationMs\":1000,\"enable\":true}]}");
    }
    if (strstr(remote_path, "ubnt_sounds_leds.conf")) {
        return test_write_text(local_path, "{\"customSounds\":[{\"soundStateName\":\"RING_BUTTON_PRESSED\",\"file\":\"dl.ogg\",\"repeatTimes\":1,\"volume\":85,\"enable\":true}]}");
    }
    if (strstr(remote_path, ".anim")) {
        return test_write_minimal_png(local_path);
    }
    return test_write_minimal_ogg(local_path);
}

void setUp(void) {
    TEST_ASSERT_NOT_NULL(getcwd(g_orig_cwd, sizeof(g_orig_cwd)));
    TEST_ASSERT_TRUE(test_make_temp_dir(g_tmp_dir, sizeof(g_tmp_dir)));

    snprintf(g_profiles_dir, sizeof(g_profiles_dir), "%s/profiles", g_tmp_dir);
    snprintf(g_sounds_dir, sizeof(g_sounds_dir), "%s/sounds", g_tmp_dir);
    snprintf(g_state_dir, sizeof(g_state_dir), "%s/state", g_tmp_dir);

    TEST_ASSERT_TRUE(utils_create_directory(g_profiles_dir));
    TEST_ASSERT_TRUE(utils_create_directory(g_sounds_dir));
    TEST_ASSERT_TRUE(utils_create_directory(g_state_dir));

    /* Create valid preset profile, custom profile, test-profile, and sfx sound inside isolated tmp dir */
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/christmas", g_profiles_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(dir, "anim.png", "ring.ogg", true, true));

    snprintf(dir, sizeof(dir), "%s/my_custom", g_profiles_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(dir, "anim.png", "ring.ogg", true, true));

    snprintf(dir, sizeof(dir), "%s/test-profile", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(dir, "anim.png", "ring.ogg", true, true));

    char sfx_path[PATH_MAX];
    snprintf(sfx_path, sizeof(sfx_path), "%s/chime.ogg", g_sounds_dir);
    TEST_ASSERT_TRUE(test_write_minimal_ogg(sfx_path));

    memset(&g_cfg, 0, sizeof(g_cfg));
    snprintf(g_cfg.mqtt_cfg.prefix, sizeof(g_cfg.mqtt_cfg.prefix), "test");
    snprintf(g_cfg.mqtt_cfg.instance, sizeof(g_cfg.mqtt_cfg.instance), "default");
    snprintf(g_cfg.mqtt_cfg.instance_human, sizeof(g_cfg.mqtt_cfg.instance_human), "Default");
    snprintf(g_cfg.ssh_cfg.host, sizeof(g_cfg.ssh_cfg.host), "192.168.1.50");
    g_cfg.ssh_cfg.port = 22;
    snprintf(g_cfg.ssh_cfg.user, sizeof(g_cfg.ssh_cfg.user), "ubnt");
    g_cfg.unifi_cfg.apply_method = UNIFI_APPLY_IPC;

    g_cfg.preset_cfg.count = 1;
    g_cfg.preset_cfg.items = calloc(1, sizeof(*g_cfg.preset_cfg.items));
    g_cfg.preset_cfg.items[0].display_name = strdup("Christmas");
    g_cfg.preset_cfg.items[0].key_name = strdup("christmas");
    g_cfg.preset_cfg.items[0].directory = strdup("christmas");

    g_cfg.sfx_preset_cfg.count = 1;
    g_cfg.sfx_preset_cfg.items = calloc(1, sizeof(*g_cfg.sfx_preset_cfg.items));
    g_cfg.sfx_preset_cfg.items[0].name = strdup("Chime");
    g_cfg.sfx_preset_cfg.items[0].key_name = strdup("chime");
    g_cfg.sfx_preset_cfg.items[0].file = strdup("chime.ogg");
    g_cfg.sfx_preset_cfg.items[0].volume = 80;

    g_sfx_ctx.sfx_preset_cfg = &g_cfg.sfx_preset_cfg;
    g_sfx_ctx.sounds_dir = g_sounds_dir;

    g_ctx.ssh_cfg = &g_cfg.ssh_cfg;
    g_ctx.preset_cfg = &g_cfg.preset_cfg;
    g_ctx.unifi_cfg = &g_cfg.unifi_cfg;
    g_ctx.sfx_ctx = &g_sfx_ctx;
    g_ctx.state_dir = g_state_dir;

    ha_topics_init(&g_cfg);
    TEST_ASSERT_TRUE(profiles_repo_init(g_profiles_dir, &g_cfg.preset_cfg));

    mock_mqtt_reset();
    mock_ssh_reset();
    g_mock_ssh.exec_cb = cmd_exec_cb;
    g_mock_ssh.upload_cb = cmd_upload_cb;
    g_mock_ssh.download_cb = cmd_download_cb;
    g_fail_download = false;
    g_fail_fetch = false;
    g_cmd_lcm_ipc_rc = 0;
    g_cmd_apply_script_exit = 0;
    g_cmd_apply_script_stderr = NULL;
    g_cmd_fail_upload_idx = 999;
    g_ipc_apply_lcm = "{\"response\":{\"payload\":{\"customAnimations\":[{\"guiId\":\"WELCOME\",\"file\":\"anim.png\",\"count\":10,\"durationMs\":1000,\"enable\":true,\"loop\":true}]}}}";
    g_ipc_val_lcm = "[{\"guiId\":\"WELCOME\",\"file\":\"anim.png\",\"count\":10,\"durationMs\":1000,\"enable\":true,\"loop\":true}]";

    /* Run in isolated tmp dir so relative "./profiles" and "./test-profile" never touch repo */
    TEST_ASSERT_EQUAL_INT(0, chdir(g_tmp_dir));
}

void tearDown(void) {
    (void)chdir(g_orig_cwd);
    profiles_repo_shutdown();
    config_free(&g_cfg);
    test_remove_dir_recursive(g_tmp_dir);
}

void test_command_set_preset_success_and_errors(void) {
    /* Invalid args ignored */
    command_set_preset(NULL, "Christmas", 9);
    command_set_preset(&g_ctx, NULL, 9);
    command_set_preset(&g_ctx, "Christmas", 0);
    TEST_ASSERT_EQUAL_UINT(0, g_mock_mqtt.publish_count);

    /* Unknown preset */
    command_set_preset(&g_ctx, "Unknown", 7);
    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);
    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_NOT_FOUND, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);

    /* SSH connect failure */
    mock_mqtt_reset();
    g_mock_ssh.create_succeeds = false;
    command_set_preset(&g_ctx, "Christmas", 9);
    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
    g_mock_ssh.create_succeeds = true;

    /* IPC success */
    mock_mqtt_reset();
    command_set_preset(&g_ctx, "Christmas", 9);
    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/preset/selected")->payload);
    TEST_ASSERT_EQUAL_STRING("", mock_mqtt_find_last_publish("/custom/directory")->payload);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);

    /* Invalid IPC response */
    mock_mqtt_reset();
    g_ipc_apply_lcm = "{bad json";
    command_set_preset(&g_ctx, "Christmas", 9);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_STATE_PARSE_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
    g_ipc_apply_lcm = "{\"response\":{\"payload\":{\"customAnimations\":[{\"guiId\":\"WELCOME\",\"file\":\"anim.png\",\"count\":10,\"durationMs\":1000,\"enable\":true,\"loop\":true}]}}}";

    /* Invalid profile.json */
    char pjson[PATH_MAX];
    snprintf(pjson, sizeof(pjson), "%s/christmas/profile.json", g_profiles_dir);
    TEST_ASSERT_TRUE(test_write_text(pjson, "{bad"));
    mock_mqtt_reset();
    command_set_preset(&g_ctx, "Christmas", 9);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_INVALID, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
}

void test_command_apply_custom_success_and_missing_dir(void) {
    command_apply_custom(NULL, "my_custom", 9);
    TEST_ASSERT_EQUAL_UINT(0, g_mock_mqtt.publish_count);

    command_apply_custom(&g_ctx, "no_such_dir", 11);
    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_NOT_FOUND, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);

    /* SSH connect failure */
    mock_mqtt_reset();
    g_mock_ssh.create_succeeds = false;
    command_apply_custom(&g_ctx, "my_custom", 9);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
    g_mock_ssh.create_succeeds = true;

    /* Invalid IPC response */
    mock_mqtt_reset();
    g_ipc_apply_lcm = "{bad";
    command_apply_custom(&g_ctx, "my_custom", 9);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_STATE_PARSE_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
    g_ipc_apply_lcm = "{\"response\":{\"payload\":{\"customAnimations\":[{\"guiId\":\"WELCOME\",\"file\":\"anim.png\",\"count\":10,\"durationMs\":1000,\"enable\":true,\"loop\":true}]}}}";

    /* Success */
    mock_mqtt_reset();
    command_apply_custom(&g_ctx, "my_custom", 9);
    TEST_ASSERT_EQUAL_STRING("my_custom", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
    TEST_ASSERT_EQUAL_STRING("none", mock_mqtt_find_last_publish("/preset/selected")->payload);
    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);

    /* Invalid profile.json */
    char pjson[PATH_MAX];
    snprintf(pjson, sizeof(pjson), "%s/my_custom/profile.json", g_profiles_dir);
    TEST_ASSERT_TRUE(test_write_text(pjson, "{bad"));
    mock_mqtt_reset();
    command_apply_custom(&g_ctx, "my_custom", 9);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_INVALID, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
}

void test_command_download_assets_success_and_partial(void) {
    command_download_assets(NULL, "1", 1);
    TEST_ASSERT_EQUAL_UINT(0, g_mock_mqtt.publish_count);

    /* SSH connect failure */
    g_mock_ssh.create_succeeds = false;
    command_download_assets(&g_ctx, "PRESS", 5);
    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
    g_mock_ssh.create_succeeds = true;

    mock_mqtt_reset();
    command_download_assets(&g_ctx, "PRESS", 5);
    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);
    const mock_mqtt_publish_t *attrs = mock_mqtt_find_last_publish("/last_download/time/attributes");
    TEST_ASSERT_NOT_NULL(attrs);
    TEST_ASSERT_NOT_NULL(strstr(attrs->payload, "/downloads/"));

    /* Download failure saves to partial/ */
    mock_mqtt_reset();
    g_fail_download = true;
    command_download_assets(&g_ctx, "PRESS", 5);
    attrs = mock_mqtt_find_last_publish("/last_download/time/attributes");
    TEST_ASSERT_NOT_NULL(attrs);
    TEST_ASSERT_NOT_NULL(strstr(attrs->payload, "/partial/"));
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_DOWNLOAD_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
}

void test_command_test_config_success(void) {
    command_test_config(NULL, "1", 1);
    TEST_ASSERT_EQUAL_UINT(0, g_mock_mqtt.publish_count);

    /* SSH connect failure */
    g_mock_ssh.create_succeeds = false;
    command_test_config(&g_ctx, "PRESS", 5);
    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
    g_mock_ssh.create_succeeds = true;

    /* Invalid IPC response */
    mock_mqtt_reset();
    g_ipc_apply_lcm = "{bad";
    command_test_config(&g_ctx, "PRESS", 5);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_STATE_PARSE_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
    g_ipc_apply_lcm = "{\"response\":{\"payload\":{\"customAnimations\":[{\"guiId\":\"WELCOME\",\"file\":\"anim.png\",\"count\":10,\"durationMs\":1000,\"enable\":true,\"loop\":true}]}}}";

    /* Success */
    mock_mqtt_reset();
    command_test_config(&g_ctx, "PRESS", 5);
    TEST_ASSERT_EQUAL_STRING("test-config", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
    TEST_ASSERT_EQUAL_STRING("none", mock_mqtt_find_last_publish("/preset/selected")->payload);
    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);

    /* Corrupt test-profile/profile.json */
    char pjson[PATH_MAX];
    snprintf(pjson, sizeof(pjson), "%s/test-profile/profile.json", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(pjson, "{bad"));
    mock_mqtt_reset();
    command_test_config(&g_ctx, "PRESS", 5);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_INVALID, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
}

void test_command_play_sfx_preset_success_and_not_found(void) {
    command_play_sfx_preset(&g_ctx, "Chime", 5);
    TEST_ASSERT_EQUAL_STRING("none", mock_mqtt_find_last_publish("/sfxpreset/selected")->payload);
    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);
    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);

    mock_mqtt_reset();
    g_mock_ssh.create_succeeds = false;
    command_play_sfx_preset(&g_ctx, "Chime", 5);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
    g_mock_ssh.create_succeeds = true;

    mock_mqtt_reset();
    command_play_sfx_preset(&g_ctx, "DoesNotExist", 12);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_SFX_PRESET_NOT_FOUND, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
}

void test_command_validate_profile_legacy_match_mismatch_and_unknown(void) {
    /* Legacy -> unsupported */
    g_cfg.unifi_cfg.apply_method = UNIFI_APPLY_LEGACY;
    command_validate_profile(&g_ctx, "PRESS", 5);
    TEST_ASSERT_EQUAL_STRING("unsupported", mock_mqtt_find_last_publish("/profile_validation")->payload);
    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);
    g_cfg.unifi_cfg.apply_method = UNIFI_APPLY_IPC;

    /* SSH connect failure */
    mock_mqtt_reset();
    g_mock_ssh.create_succeeds = false;
    command_validate_profile(&g_ctx, "PRESS", 5);
    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
    g_mock_ssh.create_succeeds = true;

    /* No last_applied.json -> unknown */
    mock_mqtt_reset();
    command_validate_profile(&g_ctx, "PRESS", 5);
    TEST_ASSERT_EQUAL_STRING("unknown", mock_mqtt_find_last_publish("/profile_validation")->payload);

    /* Apply preset first, then validate -> match */
    command_set_preset(&g_ctx, "Christmas", 9);
    mock_mqtt_reset();
    command_validate_profile(&g_ctx, "PRESS", 5);
    TEST_ASSERT_EQUAL_STRING("match", mock_mqtt_find_last_publish("/profile_validation")->payload);

    /* Device configuration drifts -> mismatch */
    mock_mqtt_reset();
    g_ipc_val_lcm = "[{\"guiId\":\"WELCOME\",\"file\":\"other.png\",\"count\":10,\"durationMs\":1000,\"enable\":true,\"loop\":true}]";
    command_validate_profile(&g_ctx, "PRESS", 5);
    TEST_ASSERT_EQUAL_STRING("mismatch", mock_mqtt_find_last_publish("/profile_validation")->payload);

    /* Fetch failure -> unknown */
    mock_mqtt_reset();
    g_fail_fetch = true;
    command_validate_profile(&g_ctx, "PRESS", 5);
    TEST_ASSERT_EQUAL_STRING("unknown", mock_mqtt_find_last_publish("/profile_validation")->payload);
}

void test_command_reapply_last_profile_preset_test_and_custom(void) {
    /* Missing state file */
    command_reapply_last_profile(&g_ctx, "PRESS", 5);
    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_EQUAL_INT(ERROR_STATE_FILE_READ_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);

    /* Reapply preset */
    command_set_preset(&g_ctx, "Christmas", 9);
    mock_mqtt_reset();
    command_reapply_last_profile(&g_ctx, "PRESS", 5);
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/last_applied_profile")->payload);

    /* Reapply test-config */
    command_test_config(&g_ctx, "PRESS", 5);
    mock_mqtt_reset();
    command_reapply_last_profile(&g_ctx, "PRESS", 5);
    TEST_ASSERT_EQUAL_STRING("test-config", mock_mqtt_find_last_publish("/last_applied_profile")->payload);

    /* Reapply custom */
    command_apply_custom(&g_ctx, "my_custom", 9);
    mock_mqtt_reset();
    command_reapply_last_profile(&g_ctx, "PRESS", 5);
    TEST_ASSERT_EQUAL_STRING("my_custom", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
}

void test_defect1_command_set_preset_does_not_report_success_on_apply_failure(void) {
    g_cmd_lcm_ipc_rc = 1;
    command_set_preset(&g_ctx, "Christmas", 9);

    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);
    TEST_ASSERT_NULL(mock_mqtt_find_last_publish("/last_applied_profile"));
    TEST_ASSERT_NULL(mock_mqtt_find_last_publish("/preset/selected"));

    char state_file[PATH_MAX];
    snprintf(state_file, sizeof(state_file), "%s/last_applied.json", g_state_dir);
    TEST_ASSERT_FALSE(utils_file_exists(state_file));

    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_APPLY_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);

    /* Retry after resolving failure succeeds and persists state */
    mock_mqtt_reset();
    g_cmd_lcm_ipc_rc = 0;
    command_set_preset(&g_ctx, "Christmas", 9);
    TEST_ASSERT_EQUAL_STRING("Christmas", mock_mqtt_find_last_publish("/last_applied_profile")->payload);
    TEST_ASSERT_TRUE(utils_file_exists(state_file));
}

void test_defect1_command_apply_custom_does_not_report_success_on_staging_failure(void) {
    g_cmd_fail_upload_idx = 4; /* lcm_ipc_msg.json staging upload failure */
    command_apply_custom(&g_ctx, "my_custom", 9);

    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);
    TEST_ASSERT_NULL(mock_mqtt_find_last_publish("/last_applied_profile"));

    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_UPLOAD_TRANSFER_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
}

void test_defect1_command_test_config_does_not_report_success_on_failure(void) {
    g_cmd_lcm_ipc_rc = 1;
    command_test_config(&g_ctx, "PRESS", 5);

    TEST_ASSERT_EQUAL_STRING("idle", mock_mqtt_find_last_publish("/status")->payload);
    TEST_ASSERT_NULL(mock_mqtt_find_last_publish("/last_applied_profile"));

    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_APPLY_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    cJSON_Delete(err);
}

void test_defect3_command_handlers_preserve_specific_apply_error_codes(void) {
    /* IPC permission denied */
    g_cfg.unifi_cfg.apply_method = UNIFI_APPLY_IPC;
    g_cmd_apply_script_exit = 13;
    g_cmd_apply_script_stderr = "ERROR step=move_anim rc=13\n";
    command_set_preset(&g_ctx, "Christmas", 9);
    cJSON *err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_APPLY_PERMISSION_DENIED, cJSON_GetObjectItem(err, "code")->valueint);
    TEST_ASSERT_EQUAL_STRING("ERROR_PROFILE_APPLY_PERMISSION_DENIED", cJSON_GetObjectItem(err, "name")->valuestring);
    cJSON_Delete(err);

    /* IPC remote disk full */
    mock_mqtt_reset();
    g_cmd_apply_script_exit = 28;
    g_cmd_apply_script_stderr = "ERROR step=move_snd rc=28\n";
    command_set_preset(&g_ctx, "Christmas", 9);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_INT(ERROR_REMOTE_DISK_FULL, cJSON_GetObjectItem(err, "code")->valueint);
    TEST_ASSERT_EQUAL_STRING("ERROR_REMOTE_DISK_FULL", cJSON_GetObjectItem(err, "name")->valuestring);
    cJSON_Delete(err);

    /* Legacy service restart failure */
    mock_mqtt_reset();
    g_cfg.unifi_cfg.apply_method = UNIFI_APPLY_LEGACY;
    g_cmd_apply_script_exit = 210;
    g_cmd_apply_script_stderr = "ERROR step=restart_services rc=210\n";
    command_set_preset(&g_ctx, "Christmas", 9);
    err = cJSON_Parse(mock_mqtt_find_last_publish("/last_error")->payload);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_APPLY_RESTART_FAILED, cJSON_GetObjectItem(err, "code")->valueint);
    TEST_ASSERT_EQUAL_STRING("ERROR_PROFILE_APPLY_RESTART_FAILED", cJSON_GetObjectItem(err, "name")->valuestring);
    cJSON_Delete(err);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_command_set_preset_success_and_errors);
    RUN_TEST(test_command_apply_custom_success_and_missing_dir);
    RUN_TEST(test_command_download_assets_success_and_partial);
    RUN_TEST(test_command_test_config_success);
    RUN_TEST(test_command_play_sfx_preset_success_and_not_found);
    RUN_TEST(test_command_validate_profile_legacy_match_mismatch_and_unknown);
    RUN_TEST(test_command_reapply_last_profile_preset_test_and_custom);

    /* Defect 1 & 3 regression tests */
    RUN_TEST(test_defect1_command_set_preset_does_not_report_success_on_apply_failure);
    RUN_TEST(test_defect1_command_apply_custom_does_not_report_success_on_staging_failure);
    RUN_TEST(test_defect1_command_test_config_does_not_report_success_on_failure);
    RUN_TEST(test_defect3_command_handlers_preserve_specific_apply_error_codes);

    return UNITY_END();
}
