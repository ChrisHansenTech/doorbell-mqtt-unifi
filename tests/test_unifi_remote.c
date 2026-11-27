#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "mocks/mock_ssh.h"
#include "support/test_helpers.h"
#include "errors.h"
#include "unifi_ipc.h"
#include "unifi_profile.h"
#include "unifi_profile_json.h"
#include "unifi_remote.h"
#include "utils.h"

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static char g_tmp_dir[128];
static int g_guardrail_rc = 0;
static int g_lcm_ipc_rc = 0;
static int g_snd_ipc_rc = 0;
static bool g_fail_reset_dir = false;
static bool g_fail_cleanup_reset_dir = false;
static bool g_fail_guardrail_exec = false;
static bool g_fail_mkdir = false;
static bool g_fail_play_cmd = false;
static bool g_fail_fetch_1 = false;
static bool g_fail_fetch_2 = false;
static bool g_fetch_emit_stderr = false;
static size_t g_fail_upload_on_idx = 999;

/* Apply-script simulation controls */
static bool g_apply_script_exec_ok = true;
static int g_apply_script_exit_status = 0;
static const char *g_apply_script_stderr = NULL;
static bool g_fail_ipc_cli_lcm_exec = false;
static bool g_fail_ipc_cli_snd_exec = false;

/* Call observation counters */
static size_t g_apply_script_calls = 0;
static size_t g_ipc_cli_lcm_calls = 0;
static size_t g_ipc_cli_snd_calls = 0;
static size_t g_reset_dir_calls = 0;

static const char *k_lcm_apply_resp =
    "{\"response\":{\"payload\":{\"customAnimations\":[{\"guiId\":\"WELCOME\",\"file\":\"anim.png\",\"count\":10,\"durationMs\":1000,\"enable\":true,\"loop\":true}]}}}";
static const char *k_snd_apply_resp =
    "{\"response\":{\"payload\":{\"customSounds\":[{\"soundStateName\":\"RING_BUTTON_PRESSED\",\"file\":\"ring.ogg\",\"repeatTimes\":1,\"volume\":85,\"enable\":true}]}}}";

static bool fake_exec_cb(
    const char *command,
    int *exit_status_out,
    char **stdout_data,
    size_t *stdout_len,
    char **stderr_data,
    size_t *stderr_len,
    size_t call_index
) {
    (void)call_index;

    if (strstr(command, "rm -rf")) {
        g_reset_dir_calls++;
        if (g_reset_dir_calls == 1 && g_fail_reset_dir) {
            return false;
        }
        if (g_reset_dir_calls >= 2 && g_fail_cleanup_reset_dir) {
            return false;
        }
        return true;
    }
    if (strncmp(command, "mkdir -p", 8) == 0) {
        return !g_fail_mkdir;
    }
    if (strstr(command, "projected_free_kb")) {
        if (g_fail_guardrail_exec) return false;
        if (exit_status_out) *exit_status_out = g_guardrail_rc;
        if (stdout_data) {
            *stdout_data = strdup("projected_free_kb=4096\n");
            if (stdout_len) *stdout_len = strlen(*stdout_data);
        }
        if (stderr_data && g_guardrail_rc != 0) {
            *stderr_data = strdup("guardrail stderr\n");
            if (stderr_len) *stderr_len = strlen(*stderr_data);
        }
        return true;
    }
    if (strstr(command, "set -eu")) {
        g_apply_script_calls++;
        if (exit_status_out) *exit_status_out = g_apply_script_exit_status;
        if (stderr_data && g_apply_script_stderr) {
            *stderr_data = strdup(g_apply_script_stderr);
            if (stderr_len) *stderr_len = strlen(*stderr_data);
        }
        return g_apply_script_exec_ok;
    }
    if (strstr(command, "-T=ubnt_lcm_gui") && strstr(command, "-M=")) {
        g_ipc_cli_lcm_calls++;
        if (g_fail_ipc_cli_lcm_exec) return false;
        if (exit_status_out) *exit_status_out = g_lcm_ipc_rc;
        if (stdout_data) {
            *stdout_data = strdup(k_lcm_apply_resp);
            if (stdout_len) *stdout_len = strlen(*stdout_data);
        }
        return true;
    }
    if (strstr(command, "-T=ubnt_sounds_leds") && strstr(command, "-M=")) {
        g_ipc_cli_snd_calls++;
        if (g_fail_ipc_cli_snd_exec) return false;
        if (exit_status_out) *exit_status_out = g_snd_ipc_rc;
        if (stdout_data) {
            *stdout_data = strdup(k_snd_apply_resp);
            if (stdout_len) *stdout_len = strlen(*stdout_data);
        }
        return true;
    }
    if (strstr(command, "-T=ubnt_lcm_gui") && strstr(command, "-n=ChangeLcmGuiSettings")) {
        if (g_fail_fetch_1) return false;
        if (exit_status_out) *exit_status_out = g_lcm_ipc_rc;
        if (stdout_data) {
            *stdout_data = strdup("[{\"guiId\":\"WELCOME\",\"file\":\"anim.png\"}]");
            if (stdout_len) *stdout_len = strlen(*stdout_data);
        }
        if (stderr_data && g_fetch_emit_stderr) {
            *stderr_data = strdup("lcm stderr");
            if (stderr_len) *stderr_len = strlen(*stderr_data);
        }
        return true;
    }
    if (strstr(command, "-T=ubnt_sounds_leds") && strstr(command, "-n=ChangeSoundLedSettings")) {
        if (g_fail_fetch_2) return false;
        if (exit_status_out) *exit_status_out = g_snd_ipc_rc;
        if (stdout_data) {
            *stdout_data = strdup("[{\"soundStateName\":\"RING_BUTTON_PRESSED\",\"file\":\"ring.ogg\"}]");
            if (stdout_len) *stdout_len = strlen(*stdout_data);
        }
        if (stderr_data && g_fetch_emit_stderr) {
            *stderr_data = strdup("snd stderr");
            if (stderr_len) *stderr_len = strlen(*stderr_data);
        }
        return true;
    }
    if (strstr(command, "playSound.sh")) {
        return !g_fail_play_cmd;
    }
    return true;
}

static bool fake_upload_cb(const char *local_path, const char *remote_dir, unsigned long remote_mode, size_t call_index) {
    (void)local_path;
    (void)remote_dir;
    (void)remote_mode;
    return call_index != g_fail_upload_on_idx;
}

static bool g_dl_include_anim = true;
static bool g_dl_corrupt_lcm_conf = false;
static bool g_dl_corrupt_snd_conf = false;
static size_t g_fail_download_on_idx = 999;

static bool fake_download_cb(const char *remote_path, const char *local_path, size_t call_index) {
    if (call_index == g_fail_download_on_idx) {
        return false;
    }
    if (strstr(remote_path, "ubnt_lcm_gui.conf")) {
        if (g_dl_corrupt_lcm_conf) {
            return test_write_text(local_path, "{corrupt json");
        }
        if (g_dl_include_anim) {
            return test_write_text(local_path, "{\"customAnimations\":[{\"guiId\":\"WELCOME\",\"file\":\"dl.png\",\"count\":12,\"durationMs\":800,\"enable\":true}]}");
        }
        return test_write_text(local_path, "{\"customAnimations\":[]}");
    }
    if (strstr(remote_path, "ubnt_sounds_leds.conf")) {
        if (g_dl_corrupt_snd_conf) {
            return test_write_text(local_path, "{corrupt json");
        }
        return test_write_text(local_path, "{\"customSounds\":[{\"soundStateName\":\"RING_BUTTON_PRESSED\",\"file\":\"dl.ogg\",\"repeatTimes\":1,\"volume\":90,\"enable\":true}]}");
    }
    if (strstr(remote_path, ".anim")) {
        return test_write_minimal_png(local_path);
    }
    if (strstr(remote_path, ".ogg")) {
        return test_write_minimal_ogg(local_path);
    }
    return test_write_text(local_path, "dummy");
}

static void reset_sim_state(void) {
    mock_ssh_reset();
    g_guardrail_rc = 0;
    g_lcm_ipc_rc = 0;
    g_snd_ipc_rc = 0;
    g_fail_reset_dir = false;
    g_fail_cleanup_reset_dir = false;
    g_fail_guardrail_exec = false;
    g_fail_mkdir = false;
    g_fail_play_cmd = false;
    g_fail_fetch_1 = false;
    g_fail_fetch_2 = false;
    g_fetch_emit_stderr = false;
    g_fail_upload_on_idx = 999;
    g_dl_include_anim = true;
    g_dl_corrupt_lcm_conf = false;
    g_dl_corrupt_snd_conf = false;
    g_fail_download_on_idx = 999;

    g_apply_script_exec_ok = true;
    g_apply_script_exit_status = 0;
    g_apply_script_stderr = NULL;
    g_fail_ipc_cli_lcm_exec = false;
    g_fail_ipc_cli_snd_exec = false;

    g_apply_script_calls = 0;
    g_ipc_cli_lcm_calls = 0;
    g_ipc_cli_snd_calls = 0;
    g_reset_dir_calls = 0;

    g_mock_ssh.exec_cb = fake_exec_cb;
    g_mock_ssh.upload_cb = fake_upload_cb;
    g_mock_ssh.download_cb = fake_download_cb;
}

void setUp(void) {
    reset_sim_state();
    TEST_ASSERT_TRUE(test_make_temp_dir(g_tmp_dir, sizeof(g_tmp_dir)));
}

void tearDown(void) {
    test_remove_dir_recursive(g_tmp_dir);
}

/* Helper to isolate tests that could SIGSEGV before Defect 2 is fixed */
static int run_isolated(int (*fn)(const char *pdir, const unifi_profile_t *profile), const char *pdir, const unifi_profile_t *profile) {
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    TEST_ASSERT_TRUE(pid >= 0);
    if (pid == 0) {
        int rc = fn(pdir, profile);
        exit(rc);
    }
    int status = 0;
    TEST_ASSERT_EQUAL_INT(pid, waitpid(pid, &status, 0));
    if (WIFSIGNALED(status)) {
        return -WTERMSIG(status);
    }
    return WEXITSTATUS(status);
}

/* ===== Existing baseline tests ===== */

void test_upload_and_apply_ipc_success(void) {
    char pdir[PATH_MAX];
    snprintf(pdir, sizeof(pdir), "%s/prof_ipc", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(pdir, "anim.png", "ring.ogg", true, true));

    unifi_profile_t profile = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(pdir, &profile));

    unifi_ipc_raw_t ipc_state = {0};
    error_return_t err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_NOT_NULL(ipc_state.lcm_gui_json);
    TEST_ASSERT_NOT_NULL(ipc_state.sounds_leds_json);
    /* Uploads: anim.png, anim.png.md5, ring.ogg, ring.ogg.md5, lcm_ipc_msg.json, sounds_ipc_msg.json */
    TEST_ASSERT_EQUAL_UINT(6, g_mock_ssh.upload_calls);

    unifi_ipc_raw_free(&ipc_state);
}

void test_upload_and_apply_legacy_success(void) {
    char pdir[PATH_MAX];
    snprintf(pdir, sizeof(pdir), "%s/prof_legacy", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(pdir, "anim.png", "ring.ogg", true, true));

    unifi_profile_t profile = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(pdir, &profile));

    error_return_t err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_LEGACY, NULL);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_EQUAL_UINT(2, g_mock_ssh.download_calls);
    TEST_ASSERT_EQUAL_UINT(6, g_mock_ssh.upload_calls);
}

void test_upload_and_apply_guardrail_warning_and_failures(void) {
    char pdir[256];
    snprintf(pdir, sizeof(pdir), "%s/prof_guard", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(pdir, "anim.png", "ring.ogg", true, true));

    unifi_profile_t profile = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(pdir, &profile));

    /* Invalid parameters */
    TEST_ASSERT_EQUAL_INT(ERROR_INVALID_PARAMETERS, unifi_profile_upload_and_apply(NULL, pdir, &profile).error_code);
    TEST_ASSERT_EQUAL_INT(ERROR_INVALID_PARAMETERS, unifi_profile_upload_and_apply_ex(NULL, pdir, &profile, UNIFI_APPLY_IPC, NULL).error_code);

    /* Guardrail exit 10 = warning only -> still succeeds */
    g_guardrail_rc = 10;
    unifi_ipc_raw_t ipc_state = {0};
    error_return_t err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, err.error_code);
    unifi_ipc_raw_free(&ipc_state);

    /* Guardrail exit 1 = remote disk full */
    reset_sim_state();
    g_guardrail_rc = 1;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_REMOTE_DISK_FULL, err.error_code);

    /* Guardrail unexpected exit 2 */
    reset_sim_state();
    g_guardrail_rc = 2;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_COMMAND_FAILED, err.error_code);

    /* Prepare workdirs SSH reset failure */
    reset_sim_state();
    g_fail_reset_dir = true;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_COMMAND_FAILED, err.error_code);

    /* Asset upload SCP transfer failure (asset + MD5) */
    reset_sim_state();
    g_fail_upload_on_idx = 0;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_UPLOAD_TRANSFER_FAILED, err.error_code);

    reset_sim_state();
    g_fail_upload_on_idx = 1;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_UPLOAD_TRANSFER_FAILED, err.error_code);

    /* mkdir failure during staging */
    reset_sim_state();
    g_fail_mkdir = true;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_COMMAND_FAILED, err.error_code);

    /* Guardrail SSH exec failure */
    reset_sim_state();
    g_fail_guardrail_exec = true;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_COMMAND_FAILED, err.error_code);

    /* Corrupted asset file (fails magic header validation & wraps error) */
    reset_sim_state();
    char bad_png[PATH_MAX];
    snprintf(bad_png, sizeof(bad_png), "%s/bad.png", pdir);
    TEST_ASSERT_TRUE(test_write_text(bad_png, "not a png header"));
    snprintf(profile.welcome.file, sizeof(profile.welcome.file), "bad.png");
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_FILE_TYPE_INVALID, err.error_code);

    /* Missing ring button asset file when welcome is disabled */
    profile.welcome.enabled = false;
    snprintf(profile.ring_button.file, sizeof(profile.ring_button.file), "missing.ogg");
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_FILE_NOT_FOUND, err.error_code);
    profile.welcome.enabled = true;

    /* Missing asset file */
    snprintf(profile.welcome.file, sizeof(profile.welcome.file), "missing.png");
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_FILE_NOT_FOUND, err.error_code);

    /* Invalid asset filename */
    snprintf(profile.welcome.file, sizeof(profile.welcome.file), "../evil.png");
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_FILENAME_INVALID, err.error_code);
}

void test_download_and_load_success_and_failures(void) {
    char dl_dir[PATH_MAX];
    snprintf(dl_dir, sizeof(dl_dir), "%s/dl1", g_tmp_dir);
    TEST_ASSERT_TRUE(utils_create_directory(dl_dir));

    unifi_profile_t out = {0};
    TEST_ASSERT_FALSE(unifi_profile_download_and_load(NULL, dl_dir, &out));
    TEST_ASSERT_FALSE(unifi_profile_download_and_load(mock_ssh_fake_session(), NULL, &out));
    TEST_ASSERT_FALSE(unifi_profile_download_and_load(mock_ssh_fake_session(), dl_dir, NULL));

    TEST_ASSERT_TRUE(unifi_profile_download_and_load(mock_ssh_fake_session(), dl_dir, &out));
    TEST_ASSERT_EQUAL_STRING("dl.png", out.welcome.file);
    TEST_ASSERT_EQUAL_STRING("dl.ogg", out.ring_button.file);
    TEST_ASSERT_EQUAL_UINT(4, g_mock_ssh.download_calls);

    /* No animation configured -> downloads 2 confs + 1 sound = 3 downloads */
    reset_sim_state();
    g_dl_include_anim = false;
    TEST_ASSERT_TRUE(unifi_profile_download_and_load(mock_ssh_fake_session(), dl_dir, &out));
    TEST_ASSERT_EQUAL_STRING("", out.welcome.file);
    TEST_ASSERT_EQUAL_STRING("dl.ogg", out.ring_button.file);
    TEST_ASSERT_EQUAL_UINT(3, g_mock_ssh.download_calls);

    /* Download failures at each stage (0=lcm.conf, 1=sounds.conf, 2=anim, 3=sound) */
    for (size_t fail_idx = 0; fail_idx < 4; fail_idx++) {
        reset_sim_state();
        g_fail_download_on_idx = fail_idx;
        TEST_ASSERT_FALSE(unifi_profile_download_and_load(mock_ssh_fake_session(), dl_dir, &out));
    }

    /* Corrupt downloaded conf JSONs */
    reset_sim_state();
    g_dl_corrupt_lcm_conf = true;
    TEST_ASSERT_FALSE(unifi_profile_download_and_load(mock_ssh_fake_session(), dl_dir, &out));

    reset_sim_state();
    g_dl_corrupt_snd_conf = true;
    TEST_ASSERT_FALSE(unifi_profile_download_and_load(mock_ssh_fake_session(), dl_dir, &out));
}

void test_sfx_upload_and_play_success_and_failures(void) {
    char sounds_dir[256];
    char sfx_file[PATH_MAX];
    snprintf(sounds_dir, sizeof(sounds_dir), "%s/sounds", g_tmp_dir);
    TEST_ASSERT_TRUE(utils_create_directory(sounds_dir));
    snprintf(sfx_file, sizeof(sfx_file), "%s/chime.ogg", sounds_dir);
    TEST_ASSERT_TRUE(test_write_minimal_ogg(sfx_file));

    config_sfx_preset_item_t sfx = {
        .name = "Chime",
        .key_name = "chime",
        .file = "chime.ogg",
        .volume = 85
    };

    /* Invalid args */
    TEST_ASSERT_EQUAL_INT(ERROR_INVALID_PARAMETERS, unifi_sfx_upload_and_play(NULL, &sfx, sounds_dir).error_code);
    TEST_ASSERT_EQUAL_INT(ERROR_INVALID_PARAMETERS, unifi_sfx_upload_and_play(mock_ssh_fake_session(), NULL, sounds_dir).error_code);
    TEST_ASSERT_EQUAL_INT(ERROR_INVALID_PARAMETERS, unifi_sfx_upload_and_play(mock_ssh_fake_session(), &sfx, NULL).error_code);

    /* Workdir prepare failure */
    reset_sim_state();
    g_fail_reset_dir = true;
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_COMMAND_FAILED, unifi_sfx_upload_and_play(mock_ssh_fake_session(), &sfx, sounds_dir).error_code);

    /* Success */
    reset_sim_state();
    error_return_t err = unifi_sfx_upload_and_play(mock_ssh_fake_session(), &sfx, sounds_dir);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_ssh.upload_calls);

    /* Upload failure */
    reset_sim_state();
    g_fail_upload_on_idx = 0;
    err = unifi_sfx_upload_and_play(mock_ssh_fake_session(), &sfx, sounds_dir);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_UPLOAD_TRANSFER_FAILED, err.error_code);

    /* Play command failure */
    reset_sim_state();
    g_fail_play_cmd = true;
    err = unifi_sfx_upload_and_play(mock_ssh_fake_session(), &sfx, sounds_dir);
    TEST_ASSERT_EQUAL_INT(ERROR_SFX_PLAY_COMMAND_FAILED, err.error_code);

    /* Invalid sound file (missing) */
    reset_sim_state();
    sfx.file = "missing.ogg";
    err = unifi_sfx_upload_and_play(mock_ssh_fake_session(), &sfx, sounds_dir);
    TEST_ASSERT_EQUAL_INT(ERROR_FILE_TYPE_INVALID, err.error_code);
}

void test_fetch_state_success_and_failures(void) {
    unifi_ipc_raw_t st = {0};

    TEST_ASSERT_EQUAL_INT(ERROR_CONFIG_INVALID, unifi_fetch_state(NULL, &st));
    TEST_ASSERT_EQUAL_INT(ERROR_CONFIG_INVALID, unifi_fetch_state(mock_ssh_fake_session(), NULL));

    g_fetch_emit_stderr = true;
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, unifi_fetch_state(mock_ssh_fake_session(), &st));
    TEST_ASSERT_NOT_NULL(st.lcm_gui_json);
    TEST_ASSERT_NOT_NULL(st.sounds_leds_json);
    unifi_ipc_raw_free(&st);
    g_fetch_emit_stderr = false;

    /* First exec fails */
    g_fail_fetch_1 = true;
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_COMMAND_FAILED, unifi_fetch_state(mock_ssh_fake_session(), &st));
    g_fail_fetch_1 = false;

    /* Second exec fails */
    g_fail_fetch_2 = true;
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_COMMAND_FAILED, unifi_fetch_state(mock_ssh_fake_session(), &st));
    g_fail_fetch_2 = false;

    /* Non-zero remote IPC exit status */
    g_lcm_ipc_rc = 1;
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_APPLY_FAILED, unifi_fetch_state(mock_ssh_fake_session(), &st));
    TEST_ASSERT_NULL(st.lcm_gui_json);
    TEST_ASSERT_NULL(st.sounds_leds_json);
}

/* ===== Defect 1 Regression Tests: Staging & Apply Error Propagation ===== */

void test_defect1_ipc_staging_failure_propagates_error_and_skips_apply(void) {
    char pdir[256];
    snprintf(pdir, sizeof(pdir), "%s/d1_ipc_stage", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(pdir, "anim.png", "ring.ogg", true, true));

    unifi_profile_t profile = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(pdir, &profile));
    unifi_ipc_raw_t ipc_state = {0};

    /* Fail upload of lcm_ipc_msg.json (5th upload, index 4) */
    reset_sim_state();
    g_fail_upload_on_idx = 4;
    error_return_t err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_UPLOAD_TRANSFER_FAILED, err.error_code);
    TEST_ASSERT_EQUAL_UINT(0, g_apply_script_calls);
    TEST_ASSERT_EQUAL_UINT(0, g_ipc_cli_lcm_calls);
    TEST_ASSERT_EQUAL_UINT(0, g_ipc_cli_snd_calls);
    TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls); /* prepare + cleanup */

    /* Fail upload of sounds_ipc_msg.json (6th upload, index 5) */
    reset_sim_state();
    g_fail_upload_on_idx = 5;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_UPLOAD_TRANSFER_FAILED, err.error_code);
    TEST_ASSERT_EQUAL_UINT(0, g_apply_script_calls);
    TEST_ASSERT_EQUAL_UINT(0, g_ipc_cli_lcm_calls);
    TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls);
}

void test_defect1_legacy_staging_failure_propagates_error_and_skips_apply(void) {
    char pdir[256];
    snprintf(pdir, sizeof(pdir), "%s/d1_legacy_stage", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(pdir, "anim.png", "ring.ogg", true, true));

    unifi_profile_t profile = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(pdir, &profile));

    /* Fail conf download -> ERROR_PROFILE_DOWNLOAD_FAILED, apply skipped */
    reset_sim_state();
    g_fail_download_on_idx = 0;
    error_return_t err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_LEGACY, NULL);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_DOWNLOAD_FAILED, err.error_code);
    TEST_ASSERT_EQUAL_UINT(0, g_apply_script_calls);
    TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls);

    /* Corrupt lcm conf prevents patching -> ERROR_PROFILE_DOWNLOAD_FAILED, apply skipped */
    reset_sim_state();
    g_dl_corrupt_lcm_conf = true;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_LEGACY, NULL);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_DOWNLOAD_FAILED, err.error_code);
    TEST_ASSERT_EQUAL_UINT(0, g_apply_script_calls);

    /* Corrupt sounds conf prevents patching -> ERROR_PROFILE_DOWNLOAD_FAILED, apply skipped */
    reset_sim_state();
    g_dl_corrupt_snd_conf = true;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_LEGACY, NULL);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_DOWNLOAD_FAILED, err.error_code);
    TEST_ASSERT_EQUAL_UINT(0, g_apply_script_calls);

    /* Fail upload of patched lcm conf (index 4) */
    reset_sim_state();
    g_fail_upload_on_idx = 4;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_LEGACY, NULL);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_UPLOAD_TRANSFER_FAILED, err.error_code);
    TEST_ASSERT_EQUAL_UINT(0, g_apply_script_calls);
    TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls);

    /* Fail upload of patched sounds conf (index 5) */
    reset_sim_state();
    g_fail_upload_on_idx = 5;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_LEGACY, NULL);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_UPLOAD_TRANSFER_FAILED, err.error_code);
    TEST_ASSERT_EQUAL_UINT(0, g_apply_script_calls);
    TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls);
}

void test_defect1_ipc_apply_failure_propagates_error_and_cleans_up(void) {
    char pdir[256];
    snprintf(pdir, sizeof(pdir), "%s/d1_ipc_apply", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(pdir, "anim.png", "ring.ogg", true, true));

    unifi_profile_t profile = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(pdir, &profile));
    unifi_ipc_raw_t ipc_state = {0};

    /* ubnt_lcm_gui IPC command returns non-zero exit code -> fails, skips sounds IPC */
    reset_sim_state();
    g_lcm_ipc_rc = 1;
    error_return_t err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_APPLY_FAILED, err.error_code);
    TEST_ASSERT_NULL(ipc_state.lcm_gui_json);
    TEST_ASSERT_NULL(ipc_state.sounds_leds_json);
    TEST_ASSERT_EQUAL_UINT(1, g_ipc_cli_lcm_calls);
    TEST_ASSERT_EQUAL_UINT(0, g_ipc_cli_snd_calls);
    TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls);

    /* ubnt_sounds_leds IPC command returns non-zero exit code */
    reset_sim_state();
    g_snd_ipc_rc = 2;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_APPLY_FAILED, err.error_code);
    TEST_ASSERT_NULL(ipc_state.lcm_gui_json);
    TEST_ASSERT_NULL(ipc_state.sounds_leds_json);
    TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls);

    /* SSH transport exec failure on ubnt_lcm_gui IPC command */
    reset_sim_state();
    g_fail_ipc_cli_lcm_exec = true;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_NOT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_EQUAL_UINT(0, g_ipc_cli_snd_calls);
    TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls);

    /* SSH transport exec failure on ubnt_sounds_leds IPC command */
    reset_sim_state();
    g_fail_ipc_cli_snd_exec = true;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_NOT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls);
}

void test_defect1_legacy_apply_failure_propagates_error_and_cleans_up(void) {
    char pdir[256];
    snprintf(pdir, sizeof(pdir), "%s/d1_legacy_apply", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(pdir, "anim.png", "ring.ogg", true, true));

    unifi_profile_t profile = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(pdir, &profile));

    /* Remote apply script exits non-zero without step marker -> generic apply failure */
    reset_sim_state();
    g_apply_script_exit_status = 1;
    g_apply_script_stderr = "unexpected shell error\n";
    error_return_t err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_LEGACY, NULL);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_APPLY_FAILED, err.error_code);
    TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls);

    /* SSH channel/exec transport failure during legacy apply */
    reset_sim_state();
    g_apply_script_exec_ok = false;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_LEGACY, NULL);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_COMMAND_FAILED, err.error_code);
    TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls);
}

void test_defect1_cleanup_failure_does_not_overwrite_primary_error_and_retry_succeeds(void) {
    char pdir[256];
    snprintf(pdir, sizeof(pdir), "%s/d1_cleanup", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(pdir, "anim.png", "ring.ogg", true, true));

    unifi_profile_t profile = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(pdir, &profile));
    unifi_ipc_raw_t ipc_state = {0};

    /* Staging fails with transfer error AND remote cleanup also fails -> original error preserved */
    reset_sim_state();
    g_fail_upload_on_idx = 4;
    g_fail_cleanup_reset_dir = true;
    error_return_t err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_UPLOAD_TRANSFER_FAILED, err.error_code);
    TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls);

    /* Retry after transient failure resolves -> succeeds cleanly */
    reset_sim_state();
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_NOT_NULL(ipc_state.lcm_gui_json);
    TEST_ASSERT_NOT_NULL(ipc_state.sounds_leds_json);
    unifi_ipc_raw_free(&ipc_state);
}

/* ===== Defect 2 Regression Tests: Optional applied_state / No NULL Dereference ===== */

static int child_convenience_wrapper_ipc_test(const char *pdir, const unifi_profile_t *profile) {
    /* Success via convenience wrapper (passes applied_state = NULL with UNIFI_APPLY_IPC) */
    reset_sim_state();
    error_return_t err = unifi_profile_upload_and_apply(mock_ssh_fake_session(), pdir, profile);
    if (err.error_code != ERROR_NONE) {
        return 10;
    }

    /* Failure via convenience wrapper -> no crash, returns error */
    reset_sim_state();
    g_lcm_ipc_rc = 1;
    err = unifi_profile_upload_and_apply(mock_ssh_fake_session(), pdir, profile);
    if (err.error_code != ERROR_PROFILE_APPLY_FAILED) {
        return 11;
    }

    /* Repeated call after failure succeeds cleanly */
    reset_sim_state();
    err = unifi_profile_upload_and_apply(mock_ssh_fake_session(), pdir, profile);
    if (err.error_code != ERROR_NONE) {
        return 12;
    }
    return 0;
}

void test_defect2_convenience_wrapper_ipc_no_crash_and_retry(void) {
    char pdir[256];
    snprintf(pdir, sizeof(pdir), "%s/d2_wrapper", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(pdir, "anim.png", "ring.ogg", true, true));

    unifi_profile_t profile = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(pdir, &profile));

    int child_rc = run_isolated(child_convenience_wrapper_ipc_test, pdir, &profile);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child_rc, "Convenience wrapper crashed or returned wrong code (negative = signal number)");
}

static int child_extended_null_state_test(const char *pdir, const unifi_profile_t *profile) {
    /* Extended IPC with NULL applied_state -> succeeds cleanly */
    reset_sim_state();
    error_return_t err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, profile, UNIFI_APPLY_IPC, NULL);
    if (err.error_code != ERROR_NONE) {
        return 20;
    }

    /* Extended IPC with NULL applied_state on failure -> returns error without crash */
    reset_sim_state();
    g_snd_ipc_rc = 1;
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, profile, UNIFI_APPLY_IPC, NULL);
    if (err.error_code != ERROR_PROFILE_APPLY_FAILED) {
        return 21;
    }

    /* Extended Legacy with NULL applied_state -> succeeds cleanly */
    reset_sim_state();
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, profile, UNIFI_APPLY_LEGACY, NULL);
    if (err.error_code != ERROR_NONE) {
        return 22;
    }
    return 0;
}

void test_defect2_extended_null_applied_state_ipc_and_legacy(void) {
    char pdir[256];
    snprintf(pdir, sizeof(pdir), "%s/d2_ext_null", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(pdir, "anim.png", "ring.ogg", true, true));

    unifi_profile_t profile = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(pdir, &profile));

    int child_rc = run_isolated(child_extended_null_state_test, pdir, &profile);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, child_rc, "Extended IPC/Legacy with NULL applied_state crashed or returned wrong code");
}

void test_defect2_extended_valid_applied_state_populated_and_cleared_on_fail(void) {
    char pdir[256];
    snprintf(pdir, sizeof(pdir), "%s/d2_ext_valid", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(pdir, "anim.png", "ring.ogg", true, true));

    unifi_profile_t profile = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(pdir, &profile));

    unifi_ipc_raw_t state = {0};

    /* Failure leaves output fields NULL with no memory leak */
    reset_sim_state();
    g_snd_ipc_rc = 1;
    error_return_t err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &state);
    TEST_ASSERT_EQUAL_INT(ERROR_PROFILE_APPLY_FAILED, err.error_code);
    TEST_ASSERT_NULL(state.lcm_gui_json);
    TEST_ASSERT_NULL(state.sounds_leds_json);

    /* Subsequent success populates both raw JSON strings */
    reset_sim_state();
    err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &state);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_NOT_NULL(state.lcm_gui_json);
    TEST_ASSERT_NOT_NULL(state.sounds_leds_json);
    unifi_ipc_raw_free(&state);
}

/* ===== Defect 3 Regression Tests: Preserve Specific Apply Error Codes ===== */

void test_defect3_legacy_apply_preserves_mapped_step_errors(void) {
    char pdir[256];
    snprintf(pdir, sizeof(pdir), "%s/d3_legacy", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(pdir, "anim.png", "ring.ogg", true, true));

    unifi_profile_t profile = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(pdir, &profile));

    const struct {
        const char *stderr_msg;
        int exit_code;
        int expected_err;
    } cases[] = {
        { "ERROR step=move_anim rc=13\n",       13,  ERROR_PROFILE_APPLY_PERMISSION_DENIED },
        { "ERROR step=move_anim rc=28\n",       28,  ERROR_REMOTE_DISK_FULL },
        { "ERROR step=move_snd rc=1\n",         1,   ERROR_PROFILE_APPLY_FILE_OPERATION_FAILED },
        { "ERROR step=verify_anim rc=1\n",      1,   ERROR_PROFILE_APPLY_VERIFY_FAILED },
        { "ERROR step=restart_services rc=210\n", 210, ERROR_PROFILE_APPLY_RESTART_FAILED },
        { "ERROR step=restart rc=1\n",          1,   ERROR_PROFILE_APPLY_RESTART_FAILED },
        { "ERROR step=unknown_step rc=99\n",    99,  ERROR_PROFILE_APPLY_FAILED },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_sim_state();
        g_apply_script_exit_status = cases[i].exit_code;
        g_apply_script_stderr = cases[i].stderr_msg;
        error_return_t err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_LEGACY, NULL);
        TEST_ASSERT_EQUAL_INT(cases[i].expected_err, err.error_code);
        TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls);
    }
}

void test_defect3_ipc_apply_preserves_mapped_step_errors_and_skips_ipc_cli(void) {
    char pdir[256];
    snprintf(pdir, sizeof(pdir), "%s/d3_ipc", g_tmp_dir);
    TEST_ASSERT_TRUE(test_create_valid_profile_dir(pdir, "anim.png", "ring.ogg", true, true));

    unifi_profile_t profile = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(pdir, &profile));

    const struct {
        const char *stderr_msg;
        int exit_code;
        int expected_err;
    } cases[] = {
        { "ERROR step=move_anim rc=13\n",       13, ERROR_PROFILE_APPLY_PERMISSION_DENIED },
        { "ERROR step=move_snd rc=28\n",        28, ERROR_REMOTE_DISK_FULL },
        { "ERROR step=move_anim rc=1\n",        1,  ERROR_PROFILE_APPLY_FILE_OPERATION_FAILED },
        { "ERROR step=verify_assets rc=1\n",    1,  ERROR_PROFILE_APPLY_VERIFY_FAILED },
        { "ERROR step=other_step rc=42\n",      42, ERROR_PROFILE_APPLY_FAILED },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_sim_state();
        g_apply_script_exit_status = cases[i].exit_code;
        g_apply_script_stderr = cases[i].stderr_msg;
        unifi_ipc_raw_t ipc_state = {0};
        error_return_t err = unifi_profile_upload_and_apply_ex(mock_ssh_fake_session(), pdir, &profile, UNIFI_APPLY_IPC, &ipc_state);
        TEST_ASSERT_EQUAL_INT(cases[i].expected_err, err.error_code);
        TEST_ASSERT_EQUAL_UINT(0, g_ipc_cli_lcm_calls);
        TEST_ASSERT_EQUAL_UINT(0, g_ipc_cli_snd_calls);
        TEST_ASSERT_NULL(ipc_state.lcm_gui_json);
        TEST_ASSERT_NULL(ipc_state.sounds_leds_json);
        TEST_ASSERT_EQUAL_UINT(2, g_reset_dir_calls);
    }
}

int main(void) {
    UNITY_BEGIN();

    /* Existing baseline tests */
    RUN_TEST(test_upload_and_apply_ipc_success);
    RUN_TEST(test_upload_and_apply_legacy_success);
    RUN_TEST(test_upload_and_apply_guardrail_warning_and_failures);
    RUN_TEST(test_download_and_load_success_and_failures);
    RUN_TEST(test_sfx_upload_and_play_success_and_failures);
    RUN_TEST(test_fetch_state_success_and_failures);

    /* Defect 1 regression tests */
    RUN_TEST(test_defect1_ipc_staging_failure_propagates_error_and_skips_apply);
    RUN_TEST(test_defect1_legacy_staging_failure_propagates_error_and_skips_apply);
    RUN_TEST(test_defect1_ipc_apply_failure_propagates_error_and_cleans_up);
    RUN_TEST(test_defect1_legacy_apply_failure_propagates_error_and_cleans_up);
    RUN_TEST(test_defect1_cleanup_failure_does_not_overwrite_primary_error_and_retry_succeeds);

    /* Defect 2 regression tests */
    RUN_TEST(test_defect2_convenience_wrapper_ipc_no_crash_and_retry);
    RUN_TEST(test_defect2_extended_null_applied_state_ipc_and_legacy);
    RUN_TEST(test_defect2_extended_valid_applied_state_populated_and_cleared_on_fail);

    /* Defect 3 regression tests */
    RUN_TEST(test_defect3_legacy_apply_preserves_mapped_step_errors);
    RUN_TEST(test_defect3_ipc_apply_preserves_mapped_step_errors_and_skips_ipc_cli);

    return UNITY_END();
}
