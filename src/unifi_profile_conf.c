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

    bool result = false;

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
        if (json_get_bool(item, "enabled", &enabled)) {
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

    bool result = false;

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

    if (!desired->welcome.enabled || desired->welcome.file[0] == '\0') {
        LOG_DEBUG("Welcome disabled or no file set, copying config unchanged");

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

    cJSON *animations = cJSON_GetObjectItemCaseSensitive(root, "customAnimations");
    if (!cJSON_IsArray(animations)) {
        animations = cJSON_AddArrayToObject(root, "customAnimations");
        if (!animations) {
            LOG_ERROR("Failed to create customAnimations array");
            goto cleanup;
        }
    }

    cJSON *welcome = NULL;
    cJSON *item = NULL;

    cJSON_ArrayForEach(item, animations) {
        if (!cJSON_IsObject(item)) {
            continue;
        }

        cJSON *gui_id = cJSON_GetObjectItemCaseSensitive(item, "guiId");

        if (cJSON_IsString(gui_id) && strcmp(gui_id->valuestring, "WELCOME") == 0) {
            welcome = item;
            break;
        } 
    }

    if (!welcome) {
        welcome = cJSON_CreateObject();
        if (!welcome) goto cleanup;
        cJSON_AddStringToObject(welcome, "guiId", "WELCOME");
        cJSON_AddItemToArray(animations, welcome);
    }

    if (!json_upsert_number_cs(welcome, "count", desired->welcome.count)) {
        goto cleanup;
    }

    if (!json_upsert_number_cs(welcome, "durationMs", desired->welcome.duration_ms)) {
        goto cleanup;
    }
    
    if (!json_upsert_bool_cs(welcome, "enable", desired->welcome.enabled)) {
        goto cleanup;
    }
    
    if (!json_upsert_string_cs(welcome, "file", desired->welcome.file)) {
        goto cleanup;
    }

    if (!json_upsert_bool_cs(welcome, "loop", desired->welcome.loop)) {
        goto cleanup;
    }
    
    json = cJSON_Print(root);

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

    cJSON *sounds = cJSON_GetObjectItemCaseSensitive(root, "customSounds");
    if (!cJSON_IsArray(sounds)) {
        sounds = cJSON_AddArrayToObject(root, "customSounds");
        if (!sounds) {
            LOG_ERROR("Failed to create customSounds array");
            goto cleanup;
        }
    }

    cJSON *sound = NULL;
    cJSON *item = NULL;

    cJSON_ArrayForEach(item, sounds) {
        if (!cJSON_IsObject(item)) {
            continue;
        }

        cJSON *state = cJSON_GetObjectItemCaseSensitive(item, "soundStateName");

        if (cJSON_IsString(state) && strcmp(state->valuestring, "RING_BUTTON_PRESSED") == 0) {
            sound = item;
            break;
        }
    }

    if (!sound) {
        sound = cJSON_CreateObject();
        if (!sound) {
            goto cleanup;
        }

        cJSON_AddStringToObject(sound, "soundStateName", "RING_BUTTON_PRESSED");
        cJSON_AddItemToArray(sounds, sound);
    }

    cJSON_ReplaceItemInObjectCaseSensitive(sound, "enable", cJSON_CreateBool(desired->ring_button.enabled));

    cJSON_ReplaceItemInObjectCaseSensitive(sound, "file", cJSON_CreateString(desired->ring_button.file));

    cJSON_ReplaceItemInObjectCaseSensitive(sound, "repeatTimes", cJSON_CreateNumber(desired->ring_button.repeat_times));

    cJSON_ReplaceItemInObjectCaseSensitive(sound, "volume", cJSON_CreateNumber(desired->ring_button.volume));

    json = cJSON_Print(root);

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

