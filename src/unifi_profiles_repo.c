#include "unifi_profiles_repo.h"
#include "cJSON.h"
#include "config_types.h"
#include "logger.h"
#include "utils.h"

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
        LOG_ERROR("Failed to allocate memory for assets base directory.");
        return false;
    }

    g_profiles_cfg = cfg;

    LOG_INFO("Assets initialized with base directory '%s'.", g_profiles_dir);
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

    LOG_DEBUG("Resolved preset '%s' to asset directory '%s'.", preset_name, directory);

    return true;  
}

bool profiles_repo_resolve_custom(const char *custom_directory, char *out_dir, size_t out_len) {
    if (!profiles_initialized()) {
        LOG_ERROR("Called before initializing assets.");
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

    LOG_DEBUG("Resolved custom asset directory '%s'.", custom_directory);
    return true;
}

void profiles_repo_shutdown(void) {
    LOG_DEBUG("Shutting down profiles subsystem.");
    free(g_profiles_dir);
    
    g_profiles_dir = NULL;
    g_profiles_cfg = NULL;
}


