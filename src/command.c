#include "command.h"
#include "errors.h"
#include "ha_status.h"
#include "mqtt_router_types.h"
#include "ssh.h"
#include "unifi_profile.h"
#include "unifi_profile_json.h"
#include "unifi_profiles_repo.h"
#include "unifi_remote.h"
#include "utils.h"
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void command_set_preset(const mqtt_router_ctx_t *ctx, const char *payload, size_t payloadLen) {
    if (ctx == NULL || payload == NULL || payloadLen == 0) {
        return;
    }

    status_set_state("Uploading");

    bool ok = false;
    ssh_session_t *session = NULL;
    char profile_path[PATH_MAX];
    unifi_profile_t profile;

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

    int rc = unifi_profile_upload_and_apply(session, profile_path, &profile);
    if (rc != ERROR_NONE) {
        HA_ERR(rc, "Failed to upload and apply profile");
        goto cleanup;
    }

    if (!profiles_write_last_applied(payload, true)) {
        LOG_WARN("Failed to persist last_applied.json (profile=%s). State will not survive restart of service.", payload);
    }

    ok = true;

cleanup: 
    if (session) {
        ssh_session_destroy(session);
    }

    if (!ok) {
        status_set_state("Idle");
        return;
    }

    status_set_last_applied_profile(payload);
    status_set_preset_selected(payload);
    status_set_custom_directory("");
    status_set_state("Idle");
}

void command_apply_custom(const mqtt_router_ctx_t *ctx, const char *payload,
                          size_t payloadLen) {
    if (ctx == NULL || payload == NULL || payloadLen == 0) {
        return;
    }

    status_set_state("Uploading");

    bool ok = false;
    ssh_session_t *session = NULL;
    char profile_path[PATH_MAX];
    unifi_profile_t profile;

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

    int rc = unifi_profile_upload_and_apply(session, profile_path, &profile);
    if (rc != ERROR_NONE) {
        HA_ERR(rc, "Failed to upload and apply profile");
        goto cleanup;
    }

    if (!profiles_write_last_applied(payload, false)) {
        LOG_WARN("Failed to persist last_applied.json (profile=%s). State will not survive restart of service.", payload);
    }

    ok = true;

cleanup: 
    if (session) {
        ssh_session_destroy(session);
    }

    if (!ok) {
        status_set_state("Idle");
        return;
    }

    status_set_last_applied_profile(payload);
    status_set_preset_selected("none");
    status_set_state("Idle");
}

void command_download_assets(const mqtt_router_ctx_t *ctx, const char *payload,
                             size_t payloadLen) {
  if (ctx == NULL || payload == NULL || payloadLen == 0) {
    return;
  }

  (void)payload;
  (void)payloadLen;

  bool partial_download = true;
  char local_path[PATH_MAX];
  char temp_path[PATH_MAX];
  char final_path[PATH_MAX];
  unifi_profile_t profile;

  status_set_state("Downloading");

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

  if (!profiles_repo_rename_temp_profile_dir(temp_path, partial_download, final_path, sizeof(final_path))) {
    HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to rename download temp path");
    goto cleanup;
  }

cleanup:
  if (session) {
    ssh_session_destroy(session);
  }

  if (partial_download) {
    if (!profiles_repo_rename_temp_profile_dir(temp_path, partial_download, final_path, sizeof(final_path))) {
        HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to rename download temp path");
    }
  }

  status_set_status_message("download/last_path", final_path);
  status_set_state("Idle");
}

void command_test_config(const mqtt_router_ctx_t *ctx, const char *payload, size_t payloadLen) {
    if (ctx == NULL || payload == NULL || payloadLen == 0) {
        return;
    }

    (void)payload;
    (void)payloadLen;

    bool ok = false;
    ssh_session_t *session = NULL;
    char profile_path[PATH_MAX] = "./test-profile";
    unifi_profile_t profile;

    if (!unifi_profile_load_from_file(profile_path, &profile)) {
        HA_ERR(ERROR_PROFILE_INVALID, "Error loading profile.json for test");
        goto cleanup;
    }

    session = ssh_session_create(ctx->ssh_cfg);
    if (!session) {
        HA_ERR(ERROR_SSH_CONNECTION_FAILED, "Failed to create SSH session");
        return;
    }

    int rc = unifi_profile_upload_and_apply(session, profile_path, &profile);
    if (rc != ERROR_NONE) {
        HA_ERR(rc, "Failed to upload and apply profile");
        goto cleanup;
    }

    ok = true;

cleanup: 
    if (session) {
        ssh_session_destroy(session);
    }

    if (!ok) {
        status_set_state("Idle");
    }

    status_set_last_applied_profile("Test Config");
    status_set_preset_selected("none");
    status_set_state("Idle");
}
