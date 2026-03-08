#include "command.h"
#include "config_types.h"
#include "errors.h"
#include "ha_status.h"
#include "mqtt_router_types.h"
#include "ssh.h"
#include "state.h"
#include "state_types.h"
#include "unifi_ipc.h"
#include "unifi_profile.h"
#include "unifi_profile_json.h"
#include "unifi_profiles_repo.h"
#include "unifi_remote.h"
#include "unifi_sfx.h"
#include "utils.h"
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void command_set_preset(const mqtt_router_ctx_t *ctx, const char *payload, size_t payloadLen) {
    if (ctx == NULL || payload == NULL || payloadLen == 0) {
        return;
    }

    status_set_state("uploading");

    bool ok = false;
    ssh_session_t *session = NULL;
    char profile_path[PATH_MAX];
    unifi_profile_t profile;
    unifi_ipc_raw_t ipc_state = {0};
    profile_state_t profile_state = {0};

    if (!profiles_repo_resolve_preset(payload, profile_path, sizeof(profile_path))) {
        HA_ERRF(ERROR_PROFILE_NOT_FOUND, "Profile directory for preset '%s' not found", payload);
        goto cleanup;
    }

    if (!unifi_profile_load_from_file(profile_path, &profile)) {
        HA_ERRF(ERROR_PROFILE_INVALID, "Error loading profile.json for preset '%s'", payload);
        goto cleanup;
    }

    session = ssh_session_create(ctx->ssh_cfg);
    if (!session) {
        HA_ERR(ERROR_SSH_CONNECTION_FAILED, "Failed to create SSH session");
        goto cleanup;
    }

    int rc = unifi_profile_upload_and_apply_ex(session, profile_path, &profile, ctx->unifi_cfg->apply_method, &ipc_state);
    if (rc != ERROR_NONE) {
        HA_ERR(rc, "Failed to upload and apply profile");
        goto cleanup;
    }

    if (ctx->unifi_cfg->apply_method == UNIFI_APPLY_IPC) {
        if (!unifi_ipc_parse_apply_response(&ipc_state, &profile_state)) {
            HA_ERR(ERROR_STATE_PARSE_FAILED, "Failed to parse IPC state response. State will not survive restart of service");
            goto cleanup;
        }
    }

    rc = state_save(&profile_state, payload, false, ctx->unifi_cfg->apply_method, ctx->state_dir);
    if (rc != ERROR_NONE) {
        HA_ERRF(rc, "Failed to persist last_applied.json (profile=%s). State will not survive restart of service.", payload);
    } 

    ok = true;

cleanup: 
    if (session) {
        ssh_session_destroy(session);
    }

    if (!ok) {
        status_set_state("idle");
        return;
    }

    status_set_last_applied_profile(payload);
    status_set_preset_selected(payload);
    status_set_custom_directory("");
    status_set_state("idle");
}

void command_apply_custom(const mqtt_router_ctx_t *ctx, const char *payload, size_t payloadLen) {
    if (ctx == NULL || payload == NULL || payloadLen == 0) {
        return;
    }

    status_set_state("uploading");

    bool ok = false;
    ssh_session_t *session = NULL;
    char profile_path[PATH_MAX];
    unifi_profile_t profile;
    unifi_ipc_raw_t ipc_state = {0};
    profile_state_t profile_state = {0};

    if (!utils_build_path(profile_path, sizeof(profile_path), "./profiles", payload)) {
        goto cleanup;
    }

    if (!utils_directory_exists(profile_path)) {
        HA_ERRF(ERROR_PROFILE_NOT_FOUND, "Custom profile '%s' not found", payload);
        goto cleanup; 
    }
    
    if (!unifi_profile_load_from_file(profile_path, &profile)) {
        HA_ERRF(ERROR_PROFILE_INVALID, "Error loading profile.json for '%s'", payload);
        goto cleanup;
    }

    session = ssh_session_create(ctx->ssh_cfg);
    if (!session) {
        HA_ERR(ERROR_SSH_CONNECTION_FAILED, "Failed to create SSH session");
        goto cleanup;
    }

    int rc = unifi_profile_upload_and_apply_ex(session, profile_path, &profile, ctx->unifi_cfg->apply_method, &ipc_state);
    if (rc != ERROR_NONE) {
        HA_ERR(rc, "Failed to upload and apply profile");
        goto cleanup;
    }

    if (ctx->unifi_cfg->apply_method == UNIFI_APPLY_IPC) {
        if (!unifi_ipc_parse_apply_response(&ipc_state, &profile_state)) {
            HA_ERR(ERROR_STATE_PARSE_FAILED, "Failed to parse IPC state response. State will not survive restart of service");
            goto cleanup;
        }
    }

    rc = state_save(&profile_state, payload, false, ctx->unifi_cfg->apply_method, ctx->state_dir);
    if (rc != ERROR_NONE) {
        HA_ERRF(rc, "Failed to persist last_applied.json (profile=%s). State will not survive restart of service.", payload);
    } 

    ok = true;

cleanup: 
    if (session) {
        ssh_session_destroy(session);
    }

    if (!ok) {
        status_set_state("idle");
        return;
    }

    status_set_last_applied_profile(payload);
    status_set_preset_selected("none");
    status_set_state("idle");
}

void command_download_assets(const mqtt_router_ctx_t *ctx, const char *payload, size_t payloadLen) {
  if (ctx == NULL || payload == NULL || payloadLen == 0) {
    return;
  }

  (void)payload;
  (void)payloadLen;

  bool partial_download = true;
  time_t now = time(NULL);
  char iso_timestamp[25];
  char local_path[PATH_MAX];
  char temp_path[PATH_MAX];
  char final_dir[PATH_MAX];
  char final_path[PATH_MAX];
  unifi_profile_t profile;

  status_set_state("downloading");

  if (!profiles_repo_create_temp_profile_dir(temp_path, sizeof(temp_path))) {
    HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to create temp path");
  }

  ssh_session_t *session = ssh_session_create(ctx->ssh_cfg);
  if (!session) {
    HA_ERR(ERROR_SSH_CONNECTION_FAILED, "Failed to create SSH session.");
    return;
  }

  if (!unifi_profile_download_and_load(session, temp_path, &profile)) {
    HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to download profile assets");
    goto cleanup;
  }

  if (!utils_build_path(local_path, sizeof(local_path), temp_path, "profile.json")) {
    HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to create path for profile.json");
    goto cleanup;
  }

  if (!unifi_profile_write_to_file(local_path, &profile)) {
    HA_ERRF(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to write %s", local_path);
    goto cleanup;
  }

  partial_download = false;

  if (!profiles_repo_rename_temp_profile_dir(temp_path, &now, partial_download, final_dir, sizeof(final_dir), final_path, sizeof(final_path))) {
    HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to rename download temp path");
    goto cleanup;
  }

cleanup:
  if (session) {
    ssh_session_destroy(session);
  }

  if (partial_download) {
    if (!profiles_repo_rename_temp_profile_dir(temp_path, &now, partial_download, final_dir, sizeof(final_dir),final_path, sizeof(final_path))) {
        HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to rename download temp path");
    }
  }

  utils_build_iso_timestamp(&now, iso_timestamp, sizeof(iso_timestamp));

  status_set_last_download(final_dir, final_path, iso_timestamp);
  status_set_state("idle");
}

void command_test_config(const mqtt_router_ctx_t *ctx, const char *payload, size_t payloadLen) {
    if (ctx == NULL || payload == NULL || payloadLen == 0) {
        return;
    }

    (void)payload;
    (void)payloadLen;

    status_set_state("uploading");

    bool ok = false;
    ssh_session_t *session = NULL;
    char profile_path[PATH_MAX] = "./test-profile";
    unifi_profile_t profile;
    unifi_ipc_raw_t ipc_state = {0};
    profile_state_t profile_state = {0};

    if (!unifi_profile_load_from_file(profile_path, &profile)) {
        HA_ERR(ERROR_PROFILE_INVALID, "Error loading profile.json for test");
        goto cleanup;
    }

    session = ssh_session_create(ctx->ssh_cfg);
    if (!session) {
        HA_ERR(ERROR_SSH_CONNECTION_FAILED, "Failed to create SSH session");
        return;
    }

    int rc = unifi_profile_upload_and_apply_ex(session, profile_path, &profile, ctx->unifi_cfg->apply_method, &ipc_state);
    if (rc != ERROR_NONE) {
        HA_ERR(rc, "Failed to upload and apply profile");
        goto cleanup;
    }

    ok = true;

    if (ctx->unifi_cfg->apply_method == UNIFI_APPLY_IPC) {
        if (!unifi_ipc_parse_apply_response(&ipc_state, &profile_state)) {
            HA_ERR(ERROR_STATE_PARSE_FAILED, "Failed to parse IPC state response. State will not survive restart of service");
            goto cleanup;
        }
    }

    rc = state_save(&profile_state, "Test Config", false, ctx->unifi_cfg->apply_method, ctx->state_dir);
    if (rc != ERROR_NONE) {
        HA_ERRF(rc, "Failed to persist last_applied.json (profile=%s). State will not survive restart of service.", payload);
    }
     

cleanup: 
    if (session) {
        ssh_session_destroy(session);
    }

    unifi_ipc_raw_free(&ipc_state);

    if (!ok) {
        status_set_state("idle");
    }

    status_set_last_applied_profile("Test Config");
    status_set_preset_selected("none");
    status_set_custom_directory("");
    status_set_state("idle");
}

void command_play_sfx_preset(const mqtt_router_ctx_t *ctx, const char *payload, size_t payloadLen) {
    (void)payloadLen;

    status_set_sfx_preset_selected(payload);
    status_set_state("uploading");

    ssh_session_t *session = NULL;
    
    const config_sfx_preset_item_t *sfx = unifi_sfx_resolve_sfx(payload, ctx->sfx_ctx);

    if (!sfx) {
        HA_ERRF(4401, "SFX preset '%s' was not found", payload);
        return;
    }

    session = ssh_session_create(ctx->ssh_cfg);
    if (!session) {
        HA_ERR(ERROR_SSH_CONNECTION_FAILED, "Failed to create SSH session");
        return;
    }

    int rc = unifi_sfx_upload_and_play(session, sfx, ctx->sfx_ctx->sounds_dir);
    if (rc != ERROR_NONE) {
        HA_ERR(rc, "Failed to upload and play SFX");
        goto cleanup;
    }

cleanup:
    if (session) {
        ssh_session_destroy(session);
    }

    status_set_sfx_preset_selected("none");
    status_set_state("idle");
}
