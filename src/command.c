#include "command.h"
#include "errors.h"
#include "ha_status.h"
#include "ha_topics.h"
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

void command_set_holiday(const mqtt_router_ctx_t *ctx, const char *payload, size_t payloadLen) {
    if (ctx == NULL || payload == NULL || payloadLen == 0) {
        return;
    }

    status_set_state("Uploading");

    bool ok = false;
    ssh_session_t *session = NULL;
    char profile_path[PATH_MAX];
    unifi_profile_t profile;

    if (!profiles_repo_resolve_holiday(payload, profile_path, sizeof(profile_path))) {
        HA_ERRF(ERROR_PROFILE_NOT_FOUND, "Profile directory for holiday '%s' not found", payload);
        goto cleanup;
    }

    if (!unifi_profile_load_from_file(profile_path, &profile)) {
        HA_ERRF(ERROR_PROFILE_INVALID, "Error loading profile.json for holiday '%s'", payload);
        goto cleanup;
    }

    session = ssh_session_create(ctx->ssh_cfg);
    if (!session) {
        HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to create SSH session");
        goto cleanup;
    }

    if (!unifi_profile_upload_and_apply(session, profile_path, &profile)) {
        HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to upload and apply profile");
        goto cleanup;
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

    status_set_active_profile(payload);
    status_set_state("Idle");
}

void command_apply_custom(const mqtt_router_ctx_t *ctx, const char *payload,
                          size_t payloadLen) {
    if (ctx == NULL || payload == NULL || payloadLen == 0) {
        return;
    }

    status_set_state("Uploading");

    bool ok = true;
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
        HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to create SSH session");
        goto cleanup;
    }

    if (!unifi_profile_upload_and_apply(session, profile_path, &profile)) {
        HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to upload and apply profile");
        goto cleanup;
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

    status_set_active_profile(payload);
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
  char tmp_path[PATH_MAX];
  char final_path[PATH_MAX];
  unifi_profile_t profile;

  status_set_state("Downloading");

  if (!utils_create_directory("profiles/.tmp")) {
    HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to create temp directory.");
  }

  char ts[32];
  utils_build_timestamp(ts, sizeof(ts));

  if (!utils_build_path(tmp_path, sizeof(tmp_path), "profiles/.tmp", ts)) {
    HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to create temp path.");
    return;
  }

  if (!utils_create_directory(tmp_path)) {
    HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to create temp directory.");
    return;
  }

  ssh_session_t *session = ssh_session_create(ctx->ssh_cfg);
  if (!session) {
    HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to create SSH session.");
    return;
  }

  if (!unifi_profile_download_and_load(session, tmp_path, &profile)) {
    HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to download profile assets");
    goto cleanup;
  }

  if (!utils_build_path(local_path, sizeof(local_path), tmp_path,
                        "profile.json")) {
    HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED,
           "Failed to create path for profile.json");
    goto cleanup;
  }

  if (!unifi_profile_write_to_file(local_path, &profile)) {
    HA_ERRF(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to write %s", local_path);
    goto cleanup;
  }

  partial_download = false;

  if (!utils_build_path(final_path, sizeof(final_path), "profiles/downloads/",
                        ts)) {
    goto cleanup;
  }

  if (rename(tmp_path, final_path) != 0) {
    HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED,
           "Failed to rename download temp path.");
    goto cleanup;
  }

cleanup:
  if (session) {
    ssh_session_destroy(session);
  }

  if (partial_download) {
    if (utils_build_path(final_path, sizeof(final_path), "profiles/partial/",
                         ts)) {
      if (rename(tmp_path, final_path) != 0) {
        HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED,
               "Failed to rename download temp path.");
      }
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
    char profile_path[PATH_MAX] = "./test-assets";
    unifi_profile_t profile;

    if (!unifi_profile_load_from_file(profile_path, &profile)) {
        HA_ERR(ERROR_PROFILE_INVALID, "Error loading profile.json for test");
        goto cleanup;
    }

    session = ssh_session_create(ctx->ssh_cfg);
    if (!session) {
        HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to create SSH session");
        return;
    }

    if (!unifi_profile_upload_and_apply(session, profile_path, &profile)) {
        HA_ERR(ERROR_PROFILE_DOWNLOAD_FAILED, "Failed to upload and apply profile");
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

    status_set_active_profile("Test");
    status_set_state("Idle");
}
