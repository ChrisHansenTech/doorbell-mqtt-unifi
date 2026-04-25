#include "unifi_remote.h"
#include "config_types.h"
#include "errors.h"
#include "logger.h"
#include "ssh.h"
#include "ssh_commands.h"
#include "unifi_ipc.h"
#include "unifi_profile.h"
#include "unifi_profile_conf.h"
#include "utils.h"

#include <complex.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *remote_temp_path;
    char local_temp_dir[PATH_MAX];
} unifi_workdir_t;

typedef enum {
    ANIM,
    SND,
} asset_type;

static int unifi_prepare_workdirs(ssh_session_t *session, unifi_workdir_t *wd) {
    char ssh_cmd[8192];
    
    if (!utils_create_directory("/tmp/doorbell-mqtt-unifi")) {
        LOG_ERROR("Failed to create /tmp/doorbell-mqtt-unifi directory");
        return ERROR_LOCAL_TEMP_DIR_CREATE_FAILED;
    }
    
    char template[] = "/tmp/doorbell-mqtt-unifi/upload-XXXXXX";
    char *dir = mkdtemp(template);
    
    if (!dir) {
        LOG_ERROR("Failed to create temp directory for '%s': %s", template, strerror(errno));
        return ERROR_LOCAL_TEMP_DIR_CREATE_FAILED;
    }

    snprintf(wd->local_temp_dir, sizeof(wd->local_temp_dir), "%s", dir);
    
    if (!ssh_cmd_reset_dir(ssh_cmd, sizeof(ssh_cmd), wd->remote_temp_path)) {
        return ERROR_SSH_COMMAND_FAILED;
    }
    
    if (!ssh_exec_command(session, ssh_cmd, NULL, NULL, NULL, NULL, NULL)) {
        return ERROR_SSH_COMMAND_FAILED;
    }
    
    return ERROR_NONE;
}

static void unifi_cleanup_workdirs(ssh_session_t *session, unifi_workdir_t *wd) {
    char ssh_cmd[8192];

    if (!utils_delete_directory(wd->local_temp_dir)) {
        LOG_WARN("Failed to delete '%s'", wd->local_temp_dir);
    }

    if (!ssh_cmd_reset_dir(ssh_cmd, sizeof(ssh_cmd), wd->remote_temp_path)) {
        LOG_WARN("Failed to delete reset'%s'", wd->remote_temp_path);
        return;
    }
    
    if (!ssh_exec_command(session, ssh_cmd, NULL, NULL, NULL, NULL, NULL)) {
        LOG_WARN("Failed to delete reset'%s'", wd->remote_temp_path);
    }
}

static int map_apply_step_to_error(const char *step, int rc) {
    if (!step) {
        return ERROR_PROFILE_APPLY_FAILED;
    }

    if (rc == 13) {
        return ERROR_PROFILE_APPLY_PERMISSION_DENIED;
    }

    if (rc == 28) {
        return ERROR_REMOTE_DISK_FULL;
    }
    
    if (strncmp(step, "move_", 5) == 0) {
        return ERROR_PROFILE_APPLY_FILE_OPERATION_FAILED;
    }

    if (strncmp(step, "verify_", 7) == 0) {
        return ERROR_PROFILE_APPLY_VERIFY_FAILED;
    }

    if (strcmp(step, "restart") == 0) {
        return ERROR_PROFILE_APPLY_RESTART_FAILED;
    }

    return ERROR_PROFILE_APPLY_FAILED;
}

static bool unifi_conf_download(ssh_session_t *session, const char *tmp_dir) {
    if (!session || !tmp_dir) {
        LOG_ERROR("Invalid parameters session=%p, tmp_dir=%p", (void*)session, (void*)tmp_dir);
        return false;
    }
    
    char lcm_gui_path[PATH_MAX];
    char sounds_leds_path[PATH_MAX];

    if (!utils_build_path(lcm_gui_path, sizeof(lcm_gui_path), tmp_dir, "ubnt_lcm_gui.conf")) {
        LOG_ERROR("Failed to create local path for ubnt_lcm_gui.conf");
        return false;
    }

    if (!ssh_scp_download_file(session, "/etc/persistent/ubnt_lcm_gui.conf", lcm_gui_path)) {
        LOG_ERROR("Failed to download ubnt_lcm_gui.conf");
        return false;
    }

    if (!utils_build_path(sounds_leds_path, sizeof(sounds_leds_path), tmp_dir, "ubnt_sounds_leds.conf")) {
        LOG_ERROR("Failed to create local path for ubnt_sounds_leds.conf");
        return false;
    }

    if (!ssh_scp_download_file(session, "/etc/persistent/ubnt_sounds_leds.conf", sounds_leds_path)) {
        LOG_ERROR("Failed to download ubnt_sounds_leds.conf");
        return false;
    }

    return true;
}

static error_return_t unifi_check_persistent_storage(ssh_session_t *session) {
    if (!session) {
        return error_create(ERROR_INVALID_PARAMETERS, "Invalid parameters");
    }

    error_return_t err = error_none();
    int exit_status = -1;
    char *stdout_buf = NULL;
    char *stderr_buf = NULL;
    size_t stdout_len = 0;
    size_t stderr_len = 0;
    char ssh_cmd[8192];

    if (!ssh_cmd_storage_guardrail(ssh_cmd, sizeof(ssh_cmd))) {
        err = error_create(ERROR_SSH_COMMAND_FAILED, "Failed to create SSH command");
        goto cleanup;
    }

    if (!ssh_exec_command(session, ssh_cmd, &exit_status, &stdout_buf, &stdout_len, &stderr_buf, &stderr_len)) {
        err = error_create(ERROR_SSH_COMMAND_FAILED, "Failed to execute SSH command");
        goto cleanup;
    }

    if (stdout_buf && *stdout_buf) {
        LOG_DEBUG("Storage guardrail output:\n%s", stdout_buf);
    }


    if (stderr_buf && *stderr_buf) {
        LOG_ERROR("%s", stderr_buf);
    }

    switch (exit_status) {
        case 0:
            break;
        case 10: {
            LOG_WARN("Doorbell storage guardrail warning: persistent storage nearly full.");
            break;
        }
        case 1: {
            LOG_ERROR("Storage guardrail failed: insufficient persistent storage.");
            err = error_create(ERROR_REMOTE_DISK_FULL, "Storage guardrail failed: insufficient persistent storage.");
            break;
        }
        default: {
            LOG_ERROR("Unexpected storage guardrail exit status: %d", exit_status);
            err = error_createf(ERROR_SSH_COMMAND_FAILED, "Unexpected storage guardrail exit status: %d", exit_status);
            break;
        }
    }

cleanup:
    free(stderr_buf);
    free(stdout_buf);

    return err;
}

static int unifi_create_asset_md5_file(const char *asset_file_path, const char *asset_filename, const char *temp_dir, char *out, size_t out_sz) {
    char md5_hex[33];
    char md5_file[PATH_MAX];
    char md5_file_path[PATH_MAX];
    
    if (!utils_md5_file_hex(asset_file_path, md5_hex)) {
        LOG_ERROR("Failed to create MD5 hash for '%s'", asset_filename);
        return ERROR_PROFILE_UPLOAD_FAILED;
    }
    
    snprintf(md5_file, sizeof(md5_file), "%s.md5", asset_filename);
    
    if (!utils_build_path(md5_file_path, sizeof(md5_file_path), temp_dir, md5_file)) {
        LOG_ERROR("Failed to create MD5 file path for '%s'", md5_file);
        return ERROR_PROFILE_UPLOAD_FAILED;
    }
    
    if (!utils_write_file(md5_file_path, md5_hex)) {
        LOG_ERROR("Failed to write MD5 file '%s'", md5_file_path);
        return ERROR_PROFILE_UPLOAD_FAILED;
    }

    int written = snprintf(out, out_sz, "%s", md5_file_path);

    if (written < 0 || (size_t)written >= out_sz) {
        LOG_ERROR("MD5 file path exceeds maximum length (%zu).", out_sz - 1);
        return ERROR_PROFILE_UPLOAD_FAILED;
    }

    return ERROR_NONE;
}

static error_return_t unifi_stage_single_asset(ssh_session_t *session, const char* profile_dir
    , const char *asset_filename, utils_file_class_t cls, const unifi_workdir_t *wd) {
    if (!session || !profile_dir || !asset_filename || !wd || wd->local_temp_dir[0] == '\0' || !wd->remote_temp_path) {
        return error_create(ERROR_INVALID_PARAMETERS, "Invalid parameters");
    }

    error_return_t err;
    
    char asset_file_path[PATH_MAX];
    char md5_file_path[PATH_MAX];
    char remote_path[PATH_MAX];
    char ssh_cmd[8192];

    err = utils_is_valid_filename(asset_filename, cls);
    if (err.error_code != ERROR_NONE) {
        goto out;
    }

    if (!utils_build_path(asset_file_path, sizeof(asset_file_path), profile_dir, asset_filename)) {
        LOG_ERROR("Error building path for asset '%s/%s'", profile_dir, asset_filename);
        err = error_createf(ERROR_PATH_TOO_LONG, "Error building path for asset %s", asset_filename);
        goto out;
    }

    if (!utils_file_exists(asset_file_path)) {
        LOG_ERROR("File '%s' does not exist", asset_file_path);
        err = error_createf(ERROR_FILE_NOT_FOUND, "File %s does not exist", asset_filename);
        goto out;
    }

    err = utils_is_valid_file(asset_file_path, cls);
    if (err.error_code != ERROR_NONE) {
        err = error_wrap(err, "Profile asset '%s' validation failed", asset_filename);
        goto out;
    }
    
    int result = unifi_create_asset_md5_file(asset_file_path, asset_filename, wd->local_temp_dir, md5_file_path, sizeof(md5_file_path));

    if (result != ERROR_NONE) {
        err = error_createf(result, "Failed to create MD5 hash for %s", asset_filename);
        goto out;
    }


    if (!utils_build_path(remote_path, sizeof(remote_path), wd->remote_temp_path, cls == UTILS_FILE_CLASS_ANIMATION ? "anim" : "sound")) {
        err = error_createf(ERROR_PATH_TOO_LONG, "Error building path for asset %s", asset_filename);
        goto out;
    }

    if (!ssh_cmd_mkdir(ssh_cmd, sizeof(ssh_cmd), remote_path)) {
        err = error_create(ERROR_SSH_COMMAND_FAILED, "Failed to create SSH command");
        goto out;
    }

    if (!ssh_exec_command(session, ssh_cmd,NULL, NULL, NULL, NULL, NULL)) {
        err = error_create(ERROR_SSH_COMMAND_FAILED, "Failed to execute SSH command");
        goto out;
    }

    if (!ssh_scp_upload_file(session, asset_file_path, remote_path, 0644)) {
        err = error_createf(ERROR_PROFILE_UPLOAD_TRANSFER_FAILED, "Failed to upload %s", asset_filename);
        goto out;
    }

    if (!ssh_scp_upload_file(session, md5_file_path, remote_path, 0644)) {
        err = error_createf(ERROR_PROFILE_UPLOAD_TRANSFER_FAILED, "Failed to upload MD5 file for %s", asset_filename);
        goto out;
    }

out:

    return err;
}

static error_return_t unifi_stage_assets(ssh_session_t *session, const char *profile_dir, const unifi_profile_t *profile, const unifi_workdir_t *wd) {
    if (!session || !profile_dir || !wd || wd->local_temp_dir[0] == '\0' || !wd->remote_temp_path) {
        return error_create(ERROR_INVALID_PARAMETERS, "Invalid parameters");
    }

    error_return_t err = error_none();
    
    if (profile->welcome.enabled) {
        err = unifi_stage_single_asset(session, profile_dir, profile->welcome.file, UTILS_FILE_CLASS_ANIMATION, wd);
        if (err.error_code != ERROR_NONE) {
            goto out;
        }
    }

    if (profile->ring_button.enabled) {
        err = unifi_stage_single_asset(session, profile_dir, profile->ring_button.file, UTILS_FILE_CLASS_SOUND, wd);
        if (err.error_code != ERROR_NONE) {
            goto out;
        }
    }

out:

    return err;
}

static int legacy_stage_artifacts(ssh_session_t *session, const unifi_profile_t *profile, const unifi_workdir_t *wd) {
    if (!session || !profile || !wd) {
        return ERROR_PROFILE_INVALID;
    }
    
    int rc = ERROR_NONE;

    char lcm_in[PATH_MAX];
    char lcm_out[PATH_MAX];
    

    if (!unifi_conf_download(session, wd->local_temp_dir)) {
       rc = ERROR_PROFILE_DOWNLOAD_FAILED;
       goto out;
    }
    
    if (!utils_build_path(lcm_in, sizeof(lcm_in), wd->local_temp_dir, "ubnt_lcm_gui.conf")) {
        LOG_ERROR("Failed to build path for ubnt_lcm_gui.conf");
        rc = ERROR_PROFILE_DOWNLOAD_FAILED;
        goto out;
    }
    
    if (!utils_build_path(lcm_out, sizeof(lcm_out), wd->local_temp_dir, "ubnt_lcm_gui.conf.patched")) {
        LOG_ERROR("Failed to build path for ubnt_lcm_gui.conf.patched");
        rc = ERROR_PROFILE_DOWNLOAD_FAILED;
        goto out;
    }
    
    if (!unifi_profile_patch_lcm_gui_conf(lcm_in, lcm_out, profile)) {
        LOG_ERROR("Failed to patch ubnt_lcm_gui.conf");
        rc = ERROR_PROFILE_DOWNLOAD_FAILED;
        goto out;
    }

    if (!ssh_scp_upload_file(session, lcm_out, wd->remote_temp_path, 0644)) {
        rc = ERROR_PROFILE_UPLOAD_TRANSFER_FAILED;
        goto out;
    }

    
    char sounds_in[PATH_MAX];
    char sounds_out[PATH_MAX];

    if (!utils_build_path(sounds_in, sizeof(sounds_in), wd->local_temp_dir, "ubnt_sounds_leds.conf")) {
        LOG_ERROR("Failed to build path for ubnt_sounds_leds.conf");
        rc = ERROR_PROFILE_DOWNLOAD_FAILED;
        goto out;
    }

    if (!utils_build_path(sounds_out, sizeof(sounds_out), wd->local_temp_dir, "ubnt_sounds_leds.conf.patched")) {
        LOG_ERROR("Failed to build path for ubnt_sounds_leds.conf.patched");
        rc = ERROR_PROFILE_DOWNLOAD_FAILED;
        goto out;
    }

    if (!unifi_profile_patch_sounds_leds_conf(sounds_in, sounds_out, profile)) {
        LOG_ERROR("Failed to patch ubnt_sounds_leds.conf");
        rc = ERROR_PROFILE_DOWNLOAD_FAILED;
        goto out;
    }

    if (!ssh_scp_upload_file(session, sounds_out, wd->remote_temp_path, 0644)) {
        rc = ERROR_PROFILE_UPLOAD_TRANSFER_FAILED;
        goto out;
    }

out:
    return rc;
}

static int unifi_ipc_stage_artifacts(ssh_session_t *session, const unifi_profile_t *profile, const unifi_workdir_t *wd) {
    if (!session || !profile || !wd) {
        return ERROR_PROFILE_INVALID;
    }

    int rc = ERROR_NONE;
    char ipc_msg_path[PATH_MAX];

    if (!utils_build_path(ipc_msg_path, sizeof(ipc_msg_path), wd->local_temp_dir, "lcm_ipc_msg.json")) {
        LOG_ERROR("Failed to build path '%s/lcm_ipc_msg.json'", wd->local_temp_dir);
        rc = ERROR_PROFILE_UPLOAD_FAILED;
        goto out;
    }

    if (!unifi_ipc_build_lcm_gui_message(ipc_msg_path, profile)) {
        LOG_ERROR("Failed to create '%s'", ipc_msg_path);
        rc = ERROR_PROFILE_UPLOAD_FAILED;
        goto out;
    }

    if (!ssh_scp_upload_file(session, ipc_msg_path, wd->remote_temp_path, 0644)) {
        LOG_ERROR("Failed to upload '%s'", ipc_msg_path);
        rc = ERROR_PROFILE_UPLOAD_TRANSFER_FAILED;
        goto out;
    }

    if (!utils_build_path(ipc_msg_path, sizeof(ipc_msg_path), wd->local_temp_dir, "sounds_ipc_msg.json")) {
        LOG_ERROR("Failed to build path '%s/lcm_ipc_msg.json'", wd->local_temp_dir);
        rc = ERROR_PROFILE_UPLOAD_FAILED;
        goto out;
    }

    if (!unifi_ipc_build_sounds_leds_message(ipc_msg_path, profile)) {
        LOG_ERROR("Failed to create '%s'", ipc_msg_path);
        rc = ERROR_PROFILE_UPLOAD_FAILED;
        goto out;
    }

    if (!ssh_scp_upload_file(session, ipc_msg_path, wd->remote_temp_path, 0644)) {
        LOG_ERROR("Failed to upload '%s'", ipc_msg_path);
         rc = ERROR_PROFILE_UPLOAD_TRANSFER_FAILED;
    }

out:
    return rc;
}

static int unifi_stage_method_artifacts(ssh_session_t *session, const unifi_profile_t *profile, const unifi_workdir_t *wd, unifi_apply_method_t method) {
    if (!session || !profile || !wd) {
        return ERROR_PROFILE_INVALID;
    }

    switch (method) {
        case UNIFI_APPLY_IPC:
            return unifi_ipc_stage_artifacts(session, profile, wd);
        case UNIFI_APPLY_LEGACY:
        default:
            return legacy_stage_artifacts(session, profile, wd);
    }
}

static int legacy_apply(ssh_session_t *session, const unifi_workdir_t *wd) {
    if (!session || !wd) {
        return ERROR_PROFILE_INVALID;
    }

    int rc = ERROR_NONE;
    char ssh_cmd[8192];
    char *out = NULL;
    char *err = NULL;
    size_t out_len = 0;
    size_t err_len = 0;
    
    if (!build_apply_profile_command(ssh_cmd, sizeof(ssh_cmd), wd->remote_temp_path)) {
        rc = ERROR_PROFILE_APPLY_FAILED;
        goto cleanup;
    }

    if (!ssh_exec_command(session, ssh_cmd, NULL, &out, &out_len, &err, &err_len)) {
        ssh_step_error_t step_error;
        if (ssh_parse_step_error(err, &step_error)) {
            LOG_ERROR("Apply profiles failed at step '%s' with return code '%d'", step_error.step, step_error.rc);
            rc = map_apply_step_to_error(step_error.step, step_error.rc);
        }
        
        rc = ERROR_PROFILE_APPLY_FAILED;
        goto cleanup;
    }

cleanup:
    if (out) {
        free(out);
    }

    if (err) {
        free(err);
    }

    return rc;
}

static int ipc_apply(ssh_session_t *session, const unifi_workdir_t *wd, unifi_ipc_raw_t *state) {
    if (!session || !wd) {
        return ERROR_PROFILE_INVALID;
    }

    int rc = ERROR_NONE;
    
    
    char ssh_cmd[8192];
    char lcm_payload_path[PATH_MAX];
    char sound_payload_path[PATH_MAX];
    
    char *std_out = NULL;
    char *err_out = NULL;
    size_t std_out_len = 0;
    size_t err_out_len = 0;

    int lcm_rc = -1;
    char *lcm_out = NULL;
    char *lcm_err = NULL;
    size_t lcm_out_len = 0;
    size_t lcm_err_len = 0;

    int sound_rc = -1;
    char *sound_out = NULL;
    char *sound_err = NULL;
    size_t sound_out_len = 0;
    size_t sound_err_len = 0;

    if (!build_move_assets_ipc(ssh_cmd, sizeof(ssh_cmd), wd->remote_temp_path)) {
        rc = ERROR_PROFILE_APPLY_FAILED;
        goto cleanup;
    }
    
    if (!ssh_exec_command(session, ssh_cmd, NULL, &std_out, &std_out_len, &err_out, &err_out_len)) {
        ssh_step_error_t step_error;
        if (ssh_parse_step_error(err_out, &step_error)) {
            LOG_ERROR("Move assets IPC failed at step '%s' with return code '%d'", step_error.step, step_error.rc);
            rc = map_apply_step_to_error(step_error.step, step_error.rc);
        }
        
        rc = ERROR_PROFILE_APPLY_FAILED;
        goto cleanup;
    }

    if (!utils_build_path(lcm_payload_path, sizeof(lcm_payload_path), wd->remote_temp_path, "lcm_ipc_msg.json")) {
        rc = ERROR_PROFILE_APPLY_FAILED;
        goto cleanup;
    }

    if (!ssh_cmd_ipc_cli(ssh_cmd, sizeof(ssh_cmd), "ubnt_lcm_gui", lcm_payload_path)) {
        rc = ERROR_PROFILE_APPLY_FAILED;
        goto cleanup;
    }

    if (!ssh_exec_command(session, ssh_cmd, &lcm_rc, &lcm_out, &lcm_out_len, &lcm_err, &lcm_err_len)) {
        rc = ERROR_PROFILE_APPLY_FAILED;
        goto cleanup;
    }

    if (!utils_build_path(sound_payload_path, sizeof(sound_payload_path), wd->remote_temp_path, "sounds_ipc_msg.json")) {
        rc = ERROR_PROFILE_APPLY_FAILED;
        goto cleanup;
    }

    if (!ssh_cmd_ipc_cli(ssh_cmd, sizeof(ssh_cmd), "ubnt_sounds_leds", sound_payload_path)) {
        rc = ERROR_PROFILE_APPLY_FAILED;
        goto cleanup;
    }

    if (!ssh_exec_command(session, ssh_cmd, &sound_rc, &sound_out, &sound_out_len, &sound_err, &sound_err_len)) {
        rc = ERROR_PROFILE_APPLY_FAILED;
        goto cleanup;
    }

    if (lcm_rc != ERROR_NONE || sound_rc != ERROR_NONE) {
        rc = ERROR_PROFILE_APPLY_FAILED;
    }

    state->lcm_gui_json = lcm_out;
    lcm_out = NULL;
    
    state->sounds_leds_json = sound_out;
    sound_out = NULL;
    
cleanup:
    if (std_out) {
        free(std_out);
    }

    if (err_out) {
        free(err_out);
    }

    if (lcm_out) {
        free(lcm_out);
    }

    if (lcm_err) {
        free(lcm_err);
    }

    if (sound_out) {
        free(sound_out);
    }
    
    if (sound_err) {
        free(sound_err);
    }

    if (rc != ERROR_NONE) {
        state->lcm_gui_json = NULL;
        state->sounds_leds_json = NULL;
    }

    return rc;
}

static int unifi_apply_method(ssh_session_t *session, const unifi_workdir_t *wd, unifi_apply_method_t method, unifi_ipc_raw_t *state) {
    if (!session || !wd) {
        return ERROR_PROFILE_INVALID;
    }

    switch (method) {
        case UNIFI_APPLY_IPC:
            return ipc_apply(session, wd, state);
        case UNIFI_APPLY_LEGACY:
        default:
            return legacy_apply(session, wd);
    }
}

static error_return_t unifi_stage_sfx(ssh_session_t *session, const char *sfx_file, const char *sound_dir, const unifi_workdir_t *wd) {
    if (!session || !sfx_file || !sound_dir || !wd) {
        return error_create(ERROR_INVALID_PARAMETERS, "Invalid parameters");
    }

    error_return_t err = error_none();

    char local_path[PATH_MAX];

    if (!utils_build_path(local_path, sizeof(local_path), sound_dir, sfx_file)) {
        LOG_ERROR("Failed to build path '%s/%s'", sound_dir, sfx_file);
        err = error_createf(ERROR_PATH_TOO_LONG, "Error building path for asset %s", sfx_file);
        goto out;
    }

    err = utils_is_valid_file(local_path, UTILS_FILE_CLASS_SOUND);
    if (err.error_code != ERROR_NONE) {
        err = error_wrap(err, "Sound '%s' validation failed", sfx_file);
        goto out;
    }

    if (!ssh_scp_upload_file(session, local_path, wd->remote_temp_path, 0644)) {
        LOG_ERROR("Failed to upload '%s'", local_path);
        err = error_createf(ERROR_PROFILE_UPLOAD_TRANSFER_FAILED, "Failed to upload %s", sfx_file);
    }

out:
    return err;
}

static error_return_t unifi_play_sfx(ssh_session_t *session, const char *sfx_file, const int volume, const unifi_workdir_t *wd) {
    if (!session || !sfx_file || !wd) {
        return error_create(ERROR_INVALID_PARAMETERS, "Invalid parameters");
    }

    error_return_t err = error_none();
    char sfx_path[PATH_MAX];
    char ssh_cmd[8192];

    if (!utils_build_path(sfx_path, sizeof(sfx_path), wd->remote_temp_path, sfx_file)) {
        LOG_ERROR("Failed to build path '%s/lcm_ipc_msg.json'", wd->local_temp_dir);
        err = error_create(ERROR_PATH_TOO_LONG, "Error building path for IPC message");
        goto out;
    }

    if (!ssh_cmd_play_sfx(ssh_cmd, sizeof(ssh_cmd), sfx_path, volume)) {
        err = error_createf(ERROR_SFX_PLAY_COMMAND_FAILED, "Failed to play %s", sfx_file);
        goto out;
    }

    if (!ssh_exec_command(session, ssh_cmd, NULL, NULL, NULL, NULL, NULL)) {
        err = error_createf(ERROR_SFX_PLAY_COMMAND_FAILED, "Failed to play %s", sfx_file);
        goto out;
    }

out:
    return err;
}

bool unifi_profile_download_and_load(ssh_session_t *session, const char *tmp_dir, unifi_profile_t *out) {
    if (!session || !tmp_dir || !out) {
        LOG_ERROR("Invalid parameters session=%p, tmp_dir=%p, out=%p", (void*)session, (void*)tmp_dir , (void*)out);
        return false;
    }

    if (!unifi_conf_download(session, tmp_dir)) {
        return false;
    }

    memset(out, 0, sizeof(*out));

    char conf_path[PATH_MAX];

    if (!utils_build_path(conf_path, sizeof(conf_path), tmp_dir, "ubnt_lcm_gui.conf")) {
        LOG_ERROR("Failed to create path for ubnt_lcm_gui.conf");
        return false;
    }

    if (!unifi_profile_read_from_lcm_gui_conf(conf_path, out)) {
        LOG_ERROR("Error loading '%s' into profile", conf_path);
        return false;
    }

    if (!utils_build_path(conf_path, sizeof(conf_path), tmp_dir, "ubnt_sounds_leds.conf")) {
        LOG_ERROR("Failed to create path for ubnt_sounds_leds.conf");
        return false;
    }

    if (!unifi_profile_read_from_sounds_leds_conf(conf_path, out)) {
        LOG_ERROR("Error loading '%s' into profile", conf_path);
        return false;
    }

    char remote_image_path[PATH_MAX];
    char local_image_path[PATH_MAX];
    char remote_sound_path[PATH_MAX];
    char local_sound_path[PATH_MAX];

    char remote_image_file[265];

    if (out->welcome.file[0] != '\0') {
        snprintf(remote_image_file, sizeof(remote_image_file), "%s.anim", out->welcome.file);

        if (!utils_build_path(remote_image_path, sizeof(remote_image_path), "/etc/persistent/lcm/animation", remote_image_file)) {
            return false;
        }

        if (!utils_build_path(local_image_path, sizeof(local_image_path), tmp_dir, out->welcome.file)) {
            return false;
        }

        if (!ssh_scp_download_file(session, remote_image_path, local_image_path)) {
            return false;
        }
    }

    if (!utils_build_path(remote_sound_path, sizeof(remote_sound_path), "/etc/persistent/sounds", out->ring_button.file)) {
        return false;
    }

    if (!utils_build_path(local_sound_path, sizeof(local_sound_path), tmp_dir, out->ring_button.file)) {
        return false;
    }

    if (!ssh_scp_download_file(session, remote_sound_path, local_sound_path)) {
        return false;
    }

    return true;
}


error_return_t unifi_profile_upload_and_apply(ssh_session_t *session, const char *profile_dir, const unifi_profile_t *profile) {
    if (!session || !profile_dir || !profile) {
        LOG_ERROR("Invalid parameters session=%p, profile_dir=%p, profile=%p", (void*)session, (void*)profile_dir , (void*)profile);
        return error_create(ERROR_INVALID_PARAMETERS, "Invalid parameters");
    }

    error_return_t result = {0};

    result = unifi_profile_upload_and_apply_ex(session, profile_dir, profile, UNIFI_APPLY_IPC, NULL);

    return result;
}


error_return_t unifi_profile_upload_and_apply_ex(ssh_session_t *session, const char *profile_dir, const unifi_profile_t *profile, unifi_apply_method_t method, unifi_ipc_raw_t *applied_state) {
    if (!session || !profile_dir || !profile) {
        LOG_ERROR("Invalid parameters session=%p, profile_dir=%p, profile=%p", (void*)session, (void*)profile_dir , (void*)profile);
        return error_create(ERROR_INVALID_PARAMETERS, "Invalid parameters");
    }

    error_return_t err;
    int rc;

    unifi_workdir_t wd = {0};

    wd.remote_temp_path = "/tmp/doorbell-mqtt-unifi/profile";

    if ((rc = unifi_prepare_workdirs(session, &wd)) != ERROR_NONE) {
        err = error_create(rc, "Error creating temp directories");
        goto cleanup;
    }

    err = unifi_stage_assets(session, profile_dir, profile, &wd);
    if (err.error_code != ERROR_NONE) {
        goto cleanup;
    }

    err = unifi_check_persistent_storage(session);
    if (err.error_code != ERROR_NONE) {
        goto cleanup;
    }

    if ((rc = unifi_stage_method_artifacts(session, profile, &wd, method) != ERROR_NONE)) {
        goto cleanup;
    }

    if ((rc = unifi_apply_method(session, &wd, method, applied_state)) != ERROR_NONE) {
        goto cleanup;
    }

cleanup:
    unifi_cleanup_workdirs(session, &wd);

    return err;
}

error_return_t unifi_sfx_upload_and_play(ssh_session_t *session, const config_sfx_preset_item_t *sfx, const char *sounds_dir) {
    if (!session || !sfx || !sounds_dir) {
        LOG_ERROR("Invalid parameters session=%p, sfx=%p, sounds_dir=%p", (void*)session, (void*)sfx, (void*)sounds_dir);
        return error_create(ERROR_INVALID_PARAMETERS, "Invalid parameters");
    }

    error_return_t err = error_none();
    int rc;

    unifi_workdir_t wd = {0};

    wd.remote_temp_path = "/tmp/doorbell-mqtt-unifi/sfx";

    if ((rc = unifi_prepare_workdirs(session, &wd)) != ERROR_NONE) {
        err = error_create(rc, "Error creating temp directories");
        goto cleanup;
    }

    err = unifi_stage_sfx(session, sfx->file, sounds_dir, &wd);
    if (err.error_code != ERROR_NONE) {
        goto cleanup;
    } 

    err = unifi_play_sfx(session, sfx->file, sfx->volume, &wd);
    if (err.error_code != ERROR_NONE) {
        goto cleanup;
    }

cleanup:
    unifi_cleanup_workdirs(session, &wd);

    return err;
}

int unifi_fetch_state(ssh_session_t *session, unifi_ipc_raw_t *out_state) {
    if (!session || !out_state) {
        LOG_ERROR("Invalid parameters session=%p, out_state=%p", (void*)session, (void*)out_state);
        return ERROR_CONFIG_INVALID;
    }

    int rc = ERROR_NONE;

    char ssh_cmd[8192];

    int lcm_rc = -1;
    char *lcm_out = NULL;
    char *lcm_err = NULL;
    size_t lcm_out_len = 0;
    size_t lcm_err_len = 0;

    int sound_rc = -1;
    char *sound_out = NULL;
    char *sound_err = NULL;
    size_t sound_out_len = 0;
    size_t sound_err_len = 0;

    if (!ssh_cmd_ipc_cli_cfg(ssh_cmd, sizeof(ssh_cmd), "ubnt_lcm_gui", "ChangeLcmGuiSettings", "customAnimations")) {
        rc = ERROR_SSH_COMMAND_FAILED;
        goto cleanup;
    }

    if (!ssh_exec_command(session, ssh_cmd, &lcm_rc, &lcm_out, &lcm_out_len, &lcm_err, &lcm_err_len)) {
        rc = ERROR_SSH_COMMAND_FAILED;
        goto cleanup;
    }

    if (!ssh_cmd_ipc_cli_cfg(ssh_cmd, sizeof(ssh_cmd), "ubnt_sounds_leds", "ChangeSoundLedSettings", "customSounds")) {
        rc = ERROR_SSH_COMMAND_FAILED;
        goto cleanup;
    }

    if (!ssh_exec_command(session, ssh_cmd, &sound_rc, &sound_out, &sound_out_len, &sound_err, &sound_err_len)) {
        rc = ERROR_SSH_COMMAND_FAILED;
        goto cleanup;
    }

    if (lcm_rc != ERROR_NONE || sound_rc != ERROR_NONE) {
        rc = ERROR_PROFILE_APPLY_FAILED;
    }

    out_state->lcm_gui_json = lcm_out;
    lcm_out = NULL;
    
    out_state->sounds_leds_json = sound_out;
    sound_out = NULL;
    
cleanup:
    if (lcm_out) {
        free(lcm_out);
    }

    if (lcm_err) {
        free(lcm_err);
    }

    if (sound_out) {
        free(sound_out);
    }
    
    if (sound_err) {
        free(sound_err);
    }

    if (rc != ERROR_NONE) {
        out_state->lcm_gui_json = NULL;
        out_state->sounds_leds_json = NULL;
    }

    return rc;

}