#include "unifi_remote.h"
#include "errors.h"
#include "logger.h"
#include "ssh.h"
#include "ssh_commands.h"
#include "unifi_profile.h"
#include "unifi_profile_conf.h"
#include "utils.h"

#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool unifi_conf_download(ssh_session_t *session, const char *tmp_dir) {
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


int unifi_profile_upload_and_apply(ssh_session_t *session, const char *profile_dir, const unifi_profile_t *profile) {
    if (!session || !profile_dir || !profile) {
        LOG_ERROR("Invalid parameters session=%p, profile_dir=%p, profile=%p", (void*)session, (void*)profile_dir , (void*)profile);
        return ERROR_PROFILE_INVALID;
    }

    int result = ERROR_NONE;

    char remote_temp_path[PATH_MAX] = "/tmp/doorbell-mqtt-unifi/";
    
    char local_img_path[PATH_MAX];
    char local_img_md5_path[PATH_MAX];
    
    char local_snd_path[PATH_MAX];
    char local_snd_md5_path[PATH_MAX];
    
    char md5_hex[33];
    char md5_file[265];
    char ssh_cmd[8192];

    char *out = NULL;
    char *err = NULL;
    size_t out_len = 0;
    size_t err_len = 0;

    if (!utils_create_directory("/tmp/doorbell-mqtt-unifi")) {
        LOG_ERROR("Failed to create /tmp/doorbell-mqtt-unifi directory");
        return ERROR_PROFILE_UPLOAD_FAILED;
    }

    char template[] = "/tmp/doorbell-mqtt-unifi/upload-XXXXXX";
    char *temp_dir = mkdtemp(template);

    if (!temp_dir) {
        LOG_ERROR("Failed to create temp directory for '%s': %s", template, strerror(errno));
        return ERROR_PROFILE_UPLOAD_FAILED;
    }
    
    if (!unifi_conf_download(session, temp_dir)) {
        result = ERROR_PROFILE_DOWNLOAD_FAILED;
        goto cleanup;
    }

    if (!ssh_cmd_rm_rf(ssh_cmd, sizeof(ssh_cmd), remote_temp_path)) {
        result = ERROR_PROFILE_UPLOAD_FAILED;
        goto cleanup;
    }

    if (!ssh_exec_command(session, ssh_cmd, NULL, NULL, NULL, NULL)) {
        result = ERROR_PROFILE_UPLOAD_FAILED;
        goto cleanup;
    }

    if (!ssh_cmd_mkdir(ssh_cmd, sizeof(ssh_cmd), remote_temp_path)) {
        result = ERROR_PROFILE_UPLOAD_FAILED;
        goto cleanup;
    }

    if (!ssh_exec_command(session, ssh_cmd, NULL, NULL, NULL, NULL)) {
        result = ERROR_PROFILE_UPLOAD_FAILED;
        goto cleanup;
    }

    if (profile->welcome.enabled) {

        char lcm_in[PATH_MAX];
        char lcm_out[PATH_MAX];

        if (!utils_build_path(lcm_in, sizeof(lcm_in), temp_dir, "ubnt_lcm_gui.conf")) {
            LOG_ERROR("Failed to build path for ubnt_lcm_gui.conf");
            result = ERROR_PROFILE_DOWNLOAD_FAILED;
            goto cleanup;
        }

        if (!utils_build_path(lcm_out, sizeof(lcm_out), temp_dir, "ubnt_lcm_gui.conf.patched")) {
            LOG_ERROR("Failed to build path for ubnt_lcm_gui.conf.patched");
            result = ERROR_PROFILE_DOWNLOAD_FAILED;
            goto cleanup;
        }

        if (!unifi_profile_patch_lcm_gui_conf(lcm_in, lcm_out, profile)) {
            LOG_ERROR("Failed to patch ubnt_lcm_gui.conf");
            result = ERROR_PROFILE_DOWNLOAD_FAILED;
            goto cleanup;
        }
        
        if (!utils_build_path(local_img_path, sizeof(local_img_path), profile_dir, profile->welcome.file)) {
            LOG_ERROR("Error building path for image '%s'", profile->welcome.file);
            result = ERROR_PROFILE_INVALID;
            goto cleanup;
        }

        if (!utils_file_exists(local_img_path)) {
            LOG_ERROR("File '%s' does not exist", local_img_path);
            result = ERROR_PROFILE_INVALID;
            goto cleanup;
        }

        if (!utils_md5_file_hex(local_img_path, md5_hex)) {
            LOG_ERROR("Failed to create MD5 hash for '%s'", local_img_path);
            result = ERROR_PROFILE_UPLOAD_FAILED;
            goto cleanup;
        }

        snprintf(md5_file, sizeof(md5_file), "%s.md5", profile->welcome.file);

        if (!utils_build_path(local_img_md5_path, sizeof(local_img_md5_path), temp_dir, md5_file)) {
            LOG_ERROR("Failed to create MD5 file path for '%s'", md5_file);
            result = ERROR_PROFILE_UPLOAD_FAILED;
            goto cleanup;
        }

        if (!utils_write_file(local_img_md5_path, md5_hex)) {
            LOG_ERROR("Failed to write MD5 file '%s'", local_img_md5_path);
            result = ERROR_PROFILE_UPLOAD_FAILED;
            goto cleanup;
        }

        if (!ssh_scp_upload_file(session, local_img_path, remote_temp_path, 0644)) {
            result = ERROR_PROFILE_UPLOAD_TRANSFER_FAILED;
            goto cleanup;
        }

        if (!ssh_scp_upload_file(session, local_img_md5_path, remote_temp_path, 0644)) {
            result = ERROR_PROFILE_UPLOAD_TRANSFER_FAILED;
            goto cleanup;
        }

        if (!ssh_scp_upload_file(session, lcm_out, remote_temp_path, 0644)) {
            result = ERROR_PROFILE_UPLOAD_TRANSFER_FAILED;
            goto cleanup;
        }
    }

    if (profile->ring_button.enabled) {

        char sounds_in[PATH_MAX];
        char sounds_out[PATH_MAX];

        if (!utils_build_path(sounds_in, sizeof(sounds_in), temp_dir, "ubnt_sounds_leds.conf")) {
            LOG_ERROR("Failed to build path for ubnt_sounds_leds.conf");
            result = ERROR_PROFILE_DOWNLOAD_FAILED;
            goto cleanup;
        }

        if (!utils_build_path(sounds_out, sizeof(sounds_out), temp_dir, "ubnt_sounds_leds.conf.patched")) {
            LOG_ERROR("Failed to build path for ubnt_sounds_leds.conf.patched");
            result = ERROR_PROFILE_DOWNLOAD_FAILED;
            goto cleanup;
        }

        if (!unifi_profile_patch_sounds_leds_conf(sounds_in, sounds_out, profile)) {
            LOG_ERROR("Failed to patch ubnt_sounds_leds.conf");
            result = ERROR_PROFILE_DOWNLOAD_FAILED;
            goto cleanup;
        }
        
        if (!utils_build_path(local_snd_path, sizeof(local_snd_path), profile_dir, profile->ring_button.file)) {
            LOG_ERROR("Error building path for sound '%s'", profile->ring_button.file);
            result = ERROR_PROFILE_UPLOAD_FAILED;
            goto cleanup;
        }

        if (!utils_file_exists(local_snd_path)) {
            LOG_ERROR("File '%s' does not exist", local_snd_path);
            result = ERROR_PROFILE_UPLOAD_FAILED;
            goto cleanup;
        }

        if (!utils_md5_file_hex(local_snd_path, md5_hex)) {
            LOG_ERROR("Failed to create MD5 hash for '%s'", local_snd_path);
            result = ERROR_PROFILE_UPLOAD_FAILED;
            goto cleanup;
        }

        snprintf(md5_file, sizeof(md5_file), "%s.md5", profile->ring_button.file);

        if (!utils_build_path(local_snd_md5_path, sizeof(local_snd_md5_path), temp_dir, md5_file)) {
            LOG_ERROR("Failed to create MD5 file path for '%s'", md5_file);
            result = ERROR_PROFILE_UPLOAD_FAILED;
            goto cleanup;
        }

        if (!utils_write_file(local_snd_md5_path, md5_hex)) {
            LOG_ERROR("Failed to write MD5 file '%s'", local_snd_md5_path);
            result = ERROR_PROFILE_UPLOAD_FAILED;
            goto cleanup;
        }

        if (!ssh_scp_upload_file(session, local_snd_path, remote_temp_path, 0644)) {
            result = ERROR_PROFILE_UPLOAD_TRANSFER_FAILED;
            goto cleanup;
        }

        if (!ssh_scp_upload_file(session, local_snd_md5_path, remote_temp_path, 0644)) {
            result = ERROR_PROFILE_UPLOAD_TRANSFER_FAILED;
            goto cleanup;
        }

        if (!ssh_scp_upload_file(session, sounds_out, remote_temp_path, 0644)) {
            result = ERROR_PROFILE_UPLOAD_TRANSFER_FAILED;
            goto cleanup;
        }
    }

    if (!build_apply_profile_command(ssh_cmd, sizeof(ssh_cmd), remote_temp_path, profile->welcome.enabled ? profile->welcome.file : NULL, profile->ring_button.enabled ? profile->ring_button.file : NULL)) {
        result = ERROR_PROFILE_APPLY_FAILED;
        goto cleanup;
    }

    if (!ssh_exec_command(session, ssh_cmd, &out, &out_len, &err, &err_len)) {
        ssh_step_error_t step_error;
        if (ssh_parse_step_error(err, &step_error)) {
            LOG_ERROR("Apply profiles failed at step '%s' with return code '%d'", step_error.step, step_error.rc);
            result = map_apply_step_to_error(step_error.step, step_error.rc);
        }
        
        result = ERROR_PROFILE_APPLY_FAILED;
        goto cleanup;
    }

cleanup:
    if (!utils_delete_directory(temp_dir)) {
        LOG_WARN("Failed to delete '%s'", temp_dir);
    }

    if (out) {
        free(out);
    }

    if (err) {
        free(err);
    }

    return result;
}