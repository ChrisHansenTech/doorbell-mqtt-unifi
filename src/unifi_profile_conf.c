#include "unifi_profile_conf.h"
#include "cJSON.h"
#include "logger.h"
#include "utils.h"
#include "utils_json.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void unifi_profile_welcome_reset(unifi_profile_welcome_t *w) {
    if (!w) return;
    w->enabled = false;
    w->file[0] = '\0';
    w->count = 0;
    w->duration_ms = 0;
    w->loop = false;
    w->gui_id[0] = '\0';
}

static void unifi_profile_ring_button_reset(unifi_profile_ring_button_t *r) {
    if (!r) return;
    r->enabled = false;
    r->file[0] = '\0';
    r->repeat_times = 1;   
    r->volume = 100; 
    r->sound_state_name[0] = '\0';
}


bool unifi_profile_read_from_lcm_gui_conf(const char *path, unifi_profile_t *out) {
    if (!path || !out) {
        LOG_ERROR("Invalid parameters: path=%p out=%p", (void*)path, (void*)out);
        return false;
    }

    char *file_buffer = NULL;

    if (!utils_read_file(path, &file_buffer, NULL)) {
        LOG_ERROR("Failed to read config file: %s", path);
        return false;
    }

    const char *error_ptr  = NULL;

    cJSON *root = cJSON_ParseWithOpts(file_buffer, &error_ptr, false);
    
    if (!root) {
        LOG_ERROR("Error reading conf file '%s' at '%s'", path, error_ptr);
        free(file_buffer);
        return false;
    }

    free(file_buffer);
    file_buffer = NULL;

    unifi_profile_welcome_reset(&out->welcome);

    bool result = true;

    cJSON *animations = cJSON_GetObjectItemCaseSensitive(root, "customAnimations");
    if (!cJSON_IsArray(animations)) {
        goto cleanup;
    }

    cJSON *item = NULL;

    cJSON_ArrayForEach(item, animations) {
        if (!cJSON_IsObject(item)) {
            continue;
        }

        const char *gui_id = json_get_string(item, "guiId");
        if (!gui_id) {
            continue;
        }

        if (strcmp(gui_id, "WELCOME") != 0) {
            continue;
        }

        snprintf(out->welcome.gui_id, sizeof(out->welcome.gui_id), "%s", gui_id);

        const char *file = json_get_string(item, "file");
        if (file) {
            snprintf(out->welcome.file, sizeof(out->welcome.file), "%s", file);
        }

        int count = 0;
        if (json_get_int(item, "count", &count)) {
            out->welcome.count = count;
        } else {
            out->welcome.count = 1;
        }

        int duration_ms = 0;
        if (json_get_int(item, "durationMs", &duration_ms)) {
            out->welcome.duration_ms = duration_ms;
        }

        bool enabled = false;
        if (json_get_bool(item, "enable", &enabled)) {
            out->welcome.enabled = enabled;
        }

        result = true;
        break;
    }

cleanup:
    cJSON_Delete(root);

    return result;
}

bool unifi_profile_read_from_sounds_leds_conf(const char *path, unifi_profile_t *out) {
    if (!path || !out) {
        LOG_ERROR("Invalid parameters: path=%p out=%p", (void*)path, (void*)out);
        return false;
    }

    char *file_buffer = NULL;

    if (!utils_read_file(path, &file_buffer, NULL)) {
        LOG_ERROR("Failed to read config file: %s", path);
        return false;
    }

    const char *error_ptr  = NULL;

    cJSON *root = cJSON_ParseWithOpts(file_buffer, &error_ptr, false);
    
    if (!root) {
        LOG_ERROR("Error reading conf file '%s' at '%s'", path, error_ptr);
        free(file_buffer);
        return false;
    }

    free(file_buffer);
    file_buffer = NULL;

    unifi_profile_ring_button_reset(&out->ring_button);

    bool result = true;

    cJSON *sounds = cJSON_GetObjectItemCaseSensitive(root, "customSounds");
    if (!cJSON_IsArray(sounds)) {
        goto cleanup;
    }

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, sounds) {
        if (!cJSON_IsObject(item)) {
            continue;
        }

        const char *state = json_get_string(item, "soundStateName");
        if (!state) {
            continue;
        }

        if (strcmp(state, "RING_BUTTON_PRESSED") != 0) {
            continue;
        }

        snprintf(out->ring_button.sound_state_name, sizeof(out->ring_button.sound_state_name), "%s", state);

        bool enable = false;
        if (json_get_bool(item, "enable", &enable)) {
            out->ring_button.enabled = enable;
        }

        const char *file = json_get_string(item, "file");
        if (file) {
            snprintf(out->ring_button.file, sizeof(out->ring_button.file), "%s", file);
        }

        int repeat_times = 1;
        if (json_get_int(item, "repeatTimes", &repeat_times)) {
            out->ring_button.repeat_times = repeat_times;
        }

        int volume = 100;
        if (json_get_int(item, "volume", &volume)) {
            out->ring_button.volume = volume;
        }

        result = true;
        break;
    }

cleanup:
    cJSON_Delete(root);

    return result;
}

bool unifi_profile_patch_lcm_gui_conf(const char *in_path, const char *out_path, const unifi_profile_t *desired) {
    if (!in_path || !out_path || !desired) {
        LOG_ERROR("Invalid parameters: in_path=%p out_path=%p, desired=%p", (void*)in_path, (void*)out_path, (void*)desired);
        return false;
    }

    char *file_buffer = NULL;

    if (!utils_read_file(in_path, &file_buffer, NULL)) {
        LOG_ERROR("Failed to read config file: %s", in_path);
        return false;
    }

    const char *error_ptr  = NULL;

    cJSON *root = cJSON_ParseWithOpts(file_buffer, &error_ptr, false);
    
    if (!root) {
        LOG_ERROR("Error reading conf file '%s' at '%s'", in_path, error_ptr ? error_ptr : "(unkown error)");
        free(file_buffer);
        return false;
    }

    free(file_buffer);
    file_buffer = NULL;

    char *json = NULL;
    bool result = false;

    cJSON *new_animations = unifi_profile_build_custom_animations_array(desired);

    cJSON_DeleteItemFromObjectCaseSensitive(root, "customAnimations");

    if (!cJSON_AddItemToObject(root, "customAnimations", new_animations)) {
        LOG_ERROR("Failed to create customAnimations array");
        cJSON_Delete(new_animations);
        goto cleanup;
    }

    new_animations = NULL;

    json = cJSON_PrintUnformatted(root);

    if (!json) {
        goto cleanup;
    }

    if (!utils_write_file(out_path, json)) {
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


bool unifi_profile_patch_sounds_leds_conf(const char *in_path, const char *out_path, const unifi_profile_t *desired) {
    if (!in_path || !out_path || !desired) {
        LOG_ERROR("Invalid parameters: in_path=%p out_path=%p, desired=%p", (void*)in_path, (void*)out_path, (void*)desired);
        return false;
    }

    if (!desired->ring_button.enabled || desired->ring_button.file[0] == '\0') {
        LOG_DEBUG("Ring button disabled or no file set, copying config unchanged");

        if (rename(in_path, out_path) != 0) {
            LOG_ERROR("Failed to rename '%s' to '%s': %s", in_path, out_path, strerror(errno));
            return false;
        }   

        return true;
    }

    char *file_buffer = NULL;

    if (!utils_read_file(in_path, &file_buffer, NULL)) {
        LOG_ERROR("Failed to read config file: %s", in_path);
        return false;
    }

    const char *error_ptr  = NULL;

    cJSON *root = cJSON_ParseWithOpts(file_buffer, &error_ptr, false);
    
    if (!root) {
        LOG_ERROR("Error reading conf file '%s' at '%s'", in_path, error_ptr ? error_ptr : "(unkown error)");
        free(file_buffer);
        return false;
    }

    free(file_buffer);
    file_buffer = NULL;

    char *json = NULL;
    bool result = false;

    cJSON *new_sounds = unifi_profile_build_custom_sounds_array(desired);

    cJSON_DeleteItemFromObjectCaseSensitive(root, "customSounds");

    if (!cJSON_AddItemToObject(root, "customSounds", new_sounds)) {
        LOG_ERROR("Failed to create customSounds array");
        cJSON_Delete(new_sounds);
        goto cleanup;
    }

    new_sounds = NULL;

    json = cJSON_PrintUnformatted(root);

    if (!json) {
        goto cleanup;
    }

    if (!utils_write_file(out_path, json)) {
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

cJSON *unifi_profile_build_custom_animations_array(const unifi_profile_t *profile) {
    if (!profile) {
        return NULL;
    }

    cJSON *custom_animations = cJSON_CreateArray();

    if (!custom_animations) {
        return NULL;
    }

    cJSON *welcome = cJSON_CreateObject();

    if (!welcome) {
        cJSON_Delete(custom_animations);
        return NULL;
    }
    
    cJSON_AddStringToObject(welcome, "guiId", "WELCOME");
    cJSON_AddNumberToObject(welcome, "count", profile->welcome.count);
    cJSON_AddNumberToObject(welcome, "durationMs", profile->welcome.duration_ms);
    cJSON_AddBoolToObject(welcome, "enable", profile->welcome.enabled);
    cJSON_AddStringToObject(welcome, "file", profile->welcome.file);
    cJSON_AddBoolToObject(welcome, "loop", profile->welcome.loop);

    cJSON_AddItemToArray(custom_animations, welcome);

    return custom_animations;
}

cJSON *unifi_profile_build_custom_sounds_array(const unifi_profile_t *profile) {
    if (!profile) {
        return NULL;
    }
    
    cJSON *custom_sounds = cJSON_CreateArray();

    if (!custom_sounds) {
        return NULL;
    }

    cJSON *ring_button = cJSON_CreateObject();

    if (!ring_button) {
        cJSON_Delete(custom_sounds);
        return NULL;
    }

    cJSON_AddStringToObject(ring_button, "soundStateName", "RING_BUTTON_PRESSED");
    cJSON_AddBoolToObject(ring_button, "enable", profile->ring_button.enabled);
    cJSON_AddStringToObject(ring_button, "file", profile->ring_button.file);
    cJSON_AddNumberToObject(ring_button, "repeatTimes", profile->ring_button.repeat_times);
    cJSON_AddNumberToObject(ring_button, "volume", profile->ring_button.volume);

    cJSON_AddItemToArray(custom_sounds, ring_button);

    return custom_sounds;    
}