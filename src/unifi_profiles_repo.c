#include "unifi_profiles_repo.h"
#include "cJSON.h"
#include "config_types.h"
#include "logger.h"
#include "utils.h"
#include "utils_json.h"

#include <errno.h>
#include <glob.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static const config_preset_t *g_profiles_cfg = NULL; 
static char *g_profiles_dir = NULL;

static bool profiles_initialized(void) {
    return g_profiles_dir != NULL && g_profiles_cfg != NULL;
}

static const char *config_get_preset_directory(const char *preset_name) {
    for (size_t i = 0; i < g_profiles_cfg->count; i++) {
        if (strcasecmp(g_profiles_cfg->items[i].display_name, preset_name) == 0) {
            return g_profiles_cfg->items[i].directory;
        }
    }

    LOG_DEBUG("No directory mapping found for preset '%s'.", preset_name);
    return NULL;
}

bool profiles_repo_init(const char *base_dir, const config_preset_t *cfg) {
    if(profiles_initialized()) {
        LOG_ERROR("profiles_init called more than once.");
        return false;
    }


    if(!utils_directory_exists(base_dir)) {
        LOG_ERROR("'%s' not found or not a directory.", base_dir);
        return false;
    }
    
    g_profiles_dir = strdup(base_dir);

    if (!g_profiles_dir) {
        LOG_ERROR("Failed to allocate memory for profile base directory.");
        return false;
    }

    g_profiles_cfg = cfg;

    LOG_INFO("Profile repository initialized with base directory '%s'.", g_profiles_dir);
    return true;
}

bool profiles_repo_resolve_preset(const char *preset_name, char *out_dir, size_t out_len) {
    if (!profiles_initialized()) {
        LOG_ERROR("Called before initializing profiles.");
        return false;
    }

    if (!preset_name || !out_dir) {
        LOG_ERROR("Invalid parameters: preset_name=%p out=%p", (void*)preset_name, (void*)out_dir);
        return false;
    }
    
    const char *directory = config_get_preset_directory(preset_name);

    if (!directory) {
        LOG_WARN("No directory configured for preset '%s'.", preset_name);
        return false;
    }

    if (!utils_build_path(out_dir, out_len, g_profiles_dir, directory)) {
        LOG_ERROR("Failed to build path for '%s' (directory %s).", preset_name, directory);
        return false;
    }

    LOG_DEBUG("Resolved preset '%s' to profile directory '%s'.", preset_name, directory);

    return true;  
}

bool profiles_repo_resolve_custom(const char *custom_directory, char *out_dir, size_t out_len) {
    if (!profiles_initialized()) {
        LOG_ERROR("Called before initializing profiles.");
        return false;
    }

    if (!custom_directory || !out_dir) {
        LOG_ERROR("Invalid parameters: custom_directory=%p out=%p",(void*)custom_directory, (void*)out_dir);
        return false;
    }   

    if (!utils_build_path(out_dir, out_len, g_profiles_dir, custom_directory)) {
        LOG_ERROR("Failed to build path for directory '%s'.", custom_directory);
        return false;
    }

    LOG_DEBUG("Resolved custom profile directory '%s'.", custom_directory);
    return true;
}

bool profiles_repo_create_temp_profile_dir(char *out_dir, size_t out_len) {
    if (!profiles_initialized()) {
        LOG_ERROR("Called before initializing profile repo");
        return false;
    }
    
    if (!out_dir) {
        LOG_ERROR("Invalid parameters: out_dir=%p", (void*)out_dir);
        return false;
    }

    char temp_path[PATH_MAX];

    if (!utils_build_path(temp_path, sizeof(temp_path), g_profiles_dir, "tmp")) {
        LOG_ERROR("Failed to build path for directory '%s/tmp'", g_profiles_dir);
        return false;
    }

    if (!utils_create_directory(temp_path)) {
        LOG_ERROR("Failed to create directory '%s'", temp_path);
        return false;
    }

    char template[PATH_MAX];
    snprintf(template, sizeof(template), "%s/tmp/download_XXXXXX", g_profiles_dir);
    
    char *temp_dir = mkdtemp(template);

    if (!temp_dir) {
        LOG_ERROR("Failed to create temp directory for '%s': %s", template, strerror(errno));
        return false;
    }

    int len = snprintf(out_dir, out_len, "%s", temp_dir);

    if ((size_t)len < strlen(temp_dir)) {
        LOG_ERROR("Download temp path was truncated");
        return false;
    }

    return true;
}

bool profiles_repo_rename_temp_profile_dir(const char *temp_dir, time_t *time, bool partial, char *out_dir, size_t out_dir_len, char *out_path, size_t out_path_len) {
    if (!profiles_initialized()) {
        LOG_ERROR("Called before initializing profile repo");
        return false;
    }

    if (!temp_dir) {
        LOG_ERROR("Invalid parameters: temp_dir=%p", (void*)temp_dir);
        return false;
    }

    char ts[32];
    utils_build_timestamp_dir(time, ts, sizeof(ts));

    snprintf(out_dir, out_dir_len, "%s", ts);

    char download_path[PATH_MAX];
    char final_path[PATH_MAX];

    if (!utils_build_path(download_path, sizeof(download_path), g_profiles_dir, partial ? "partial" : "downloads")) {
        LOG_ERROR("Failed to build path '%s/%s'", g_profiles_dir, partial ? "partial" : "downloads");
        return false;
    }

    if (!utils_create_directory(download_path)) {
        LOG_ERROR("Failed to create directory '%s'", download_path);
        return false;
    }

    if (!utils_build_path(final_path, sizeof(final_path), download_path, ts)) {
        LOG_ERROR("Failed to build path '%s/%s'", download_path, ts);
        return false;
    }

    if (rename(temp_dir, final_path) != 0) {
        LOG_ERROR("Failed to rename directory '%s' to '%s'", temp_dir, final_path);
        return false;
    }

    int len = snprintf(out_path, out_path_len, "%s", final_path);

    if ((size_t)len < strlen(final_path)) {
        LOG_WARN("Out value of '%s' was truncated from '%s'", out_path, final_path);
    }
    
    return true;
}

bool profiles_write_last_applied(const char *name, bool is_preset) {
    if (!name) {
        LOG_ERROR("Invalid parameters: name=%p", (void*)name);
        return false;
    }

    char state_path[PATH_MAX];
    if (!utils_build_path(state_path, sizeof(state_path), g_profiles_dir, ".state/")) {
        LOG_ERROR("Failed to create path for profile '%s/.state/'", g_profiles_dir);
        return false;
    }

    if (!utils_create_directory(state_path)) {
        LOG_ERROR("Failed to create directory '%s'", state_path);
        return false;
    }

    char last_applied_path[PATH_MAX];
    if (!utils_build_path(last_applied_path, sizeof(last_applied_path), state_path, "last_applied.json")) {
        LOG_ERROR("Failed to create path for '%s/last_applied.json", state_path);
        return false;
    }

    cJSON *root = cJSON_CreateObject();

    if (!root) {
        LOG_ERROR("Failed to create 'root' object");
        return false;
    }

    char *json = NULL;
    bool result = false;
    time_t applied_at = time(NULL);

    if (!cJSON_AddNumberToObject(root, "schemaVersion", 1) ||
        !cJSON_AddStringToObject(root, "profileName", name) ||
        !cJSON_AddBoolToObject(root, "isPreset", is_preset) ||
        !cJSON_AddNumberToObject(root, "appliedAt", (double)applied_at)) {
            LOG_ERROR("Failed to populate last applied object");
            goto cleanup;
        }

    json = cJSON_Print(root);
    
    if (!json) {
        goto cleanup;
    }

    if (!utils_write_file(last_applied_path, json)) {
        goto cleanup;
    }

    result = true;

cleanup:
    if (json) {
        cJSON_free(json);
    }

    cJSON_Delete(root);

    return result;
}


bool profile_load_last_applied(unifi_last_applied_profile_t *out) {
    if (!out) {
        LOG_ERROR("Invalid parameters: out=%p", (void*)out);
        return false;
    }

    char path[PATH_MAX];
    if (!utils_build_path(path, sizeof(path), g_profiles_dir, "/.state/last_applied.json")) {
        LOG_ERROR("Failed to create path for '%s/.state/last_applied.json", g_profiles_dir);
        return false;
    }

    memset((void*)out, 0, sizeof(*out));

    char *json_buffer = NULL;

    if (!utils_read_file(path, &json_buffer, NULL)) {
        LOG_ERROR("Failed to read the profile file: %s", path);
        return false;
    }

    const char *error_ptr = NULL;

    cJSON *root = cJSON_ParseWithOpts(json_buffer, &error_ptr, false);

    if (!root) {
        LOG_ERROR("JSON parsing error in %s before: %s", path, error_ptr ? error_ptr : "(unknown position)");
        free(json_buffer);
        return false;
    }

    free(json_buffer);
    json_buffer = NULL;

    const char *profile_name = json_get_string(root, "profileName");

    if (profile_name) {
        snprintf(out->profile_name, sizeof(out->profile_name), "%s", profile_name);
    }

    bool is_preset = false;
    if(json_get_bool(root, "isPreset", &is_preset)) {
       out->is_preset = is_preset; 
    }

    double applied_at = 0;
    if (json_get_double(root, "appliedAt", &applied_at)) {
        out->appied_at = (time_t)applied_at;
    }

    if (out->profile_name[0] == '\0') {
        cJSON_Delete(root);
        return false;
    }

    cJSON_Delete(root);

    return true;
}

void profiles_repo_shutdown(void) {
    LOG_DEBUG("Shutting down profiles subsystem.");
    free(g_profiles_dir);
    
    g_profiles_dir = NULL;
    g_profiles_cfg = NULL;
}


