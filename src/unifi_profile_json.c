#include "unifi_profile_json.h"
#include "cJSON.h"
#include "logger.h"
#include "utils.h"
#include "utils_json.h"
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool unifi_profile_load_from_file(const char *path, unifi_profile_t *p) {
    if (!path || !p) {
        LOG_ERROR("Invalid parameters: path=%p p=%p", (void*)path, (void*)p);
        return false;
    }

    char profile_path[PATH_MAX];

    if (!utils_build_path(profile_path, sizeof(profile_path), path, "profile.json")) {
        LOG_ERROR("Error building path profile path");
        return false;
    }

    if (!utils_file_exists(profile_path)) {
        LOG_ERROR("Profile.json does not exists in '%s'", profile_path);
        return false;
    }

    memset((void*)p, 0, sizeof(*p));

    char *json_buffer = NULL;

    if (!utils_read_file(profile_path, &json_buffer, NULL)) {
        LOG_ERROR("Failed to read the profile file: %s", profile_path);
        return false;
    }

    const char *error_ptr = NULL;

    cJSON *root = cJSON_ParseWithOpts(json_buffer, &error_ptr, false);

    if (!root) {
        LOG_ERROR("JSON parsing error in %s before: %s", profile_path, error_ptr ? error_ptr : "(unknown position)");
        free(json_buffer);
    }

    if (!json_get_int(root, "schemaVersion", &p->schema_version)) {
        LOG_ERROR("Failed to load schema version using default of 1");
        p->schema_version = 1;
    }

    cJSON *animation = cJSON_GetObjectItemCaseSensitive(root, "welcome");
    cJSON *sound = cJSON_GetObjectItemCaseSensitive(root, "ringButton");

    if (animation) {
        bool enabled = false;
        if (json_get_bool(animation, "enabled", &enabled)) {
            p->welcome.enabled = enabled;
        }

        const char *file = json_get_string(animation, "file");

        if (file) {
            snprintf(p->welcome.file, sizeof(p->welcome.file), "%s", file);
        }

        int count = 0;
        if (json_get_int(animation, "count", &count)) {
            p->welcome.count = count;
        }

        int durationMs = 0;
        if (json_get_int(animation, "durationMs", &durationMs)) {
            p->welcome.duration_ms = durationMs;
        }

        bool loop = false;
        if (json_get_bool(animation, "loop", &loop)) {
            p->welcome.loop = loop;
        }

        const char *guiId = json_get_string(animation, "guiId");

        if (guiId) {
            snprintf(p->welcome.gui_id, sizeof(p->welcome.gui_id), "%s", guiId);
        }
    }

    if (sound) {
        bool enabled = false;
        if (json_get_bool(sound, "enabled", &enabled)) {
            p->ring_button.enabled = enabled;
        }

        const char *file = json_get_string(sound, "file");

        if (file) {
            snprintf(p->ring_button.file, sizeof(p->ring_button.file), "%s", file);
        }

        int repeat_times = 1;
        if (json_get_int(sound, "repeatTimes", &repeat_times)) {
            p->ring_button.repeat_times = repeat_times;
        }

        int volume = 100;
        if (json_get_int(sound, "volume", &volume)) {
            p->ring_button.volume = volume;
        }

        const char *state_name = json_get_string(sound, "soundStateName");

        if (state_name) {
            snprintf(p->ring_button.sound_state_name, sizeof(p->ring_button.sound_state_name), "%s", state_name);
        }
    }

    cJSON_Delete(root);

    return true;
    
}

bool unifi_profile_write_to_file(const char *path, const unifi_profile_t *p) {
    if (!path || !p) {
        LOG_ERROR("Invalid parameters: path=%p p=%p", (void*)path, (void*)p);
        return false;
    }

    cJSON *root = cJSON_CreateObject();

    if (!root) {
        LOG_ERROR("Failed to create 'root' object");
        return false;
    }

    char *json = NULL;
    bool result = false;

    cJSON_AddNumberToObject(root, "schemaVersion", 1);

    cJSON *welcome = cJSON_AddObjectToObject(root, "welcome");

    if (!welcome) {
        LOG_ERROR("Failed to create 'welcome' object");
        goto cleanup;
    }

    if (!cJSON_AddBoolToObject(welcome, "enabled", p->welcome.enabled) ||
        !cJSON_AddStringToObject(welcome, "file", p->welcome.file) ||
        !cJSON_AddNumberToObject(welcome, "count", p->welcome.count) ||
        !cJSON_AddNumberToObject(welcome, "durationMs", p->welcome.duration_ms) ||
        !cJSON_AddBoolToObject(welcome, "loop", p->welcome.loop) ||
        !cJSON_AddStringToObject(welcome, "guiId", p->welcome.gui_id)) {
            LOG_ERROR("Failed to populate 'welcome' object");
            goto cleanup;
        }

    cJSON *ringButton = cJSON_AddObjectToObject(root, "ringButton");

    if (!ringButton) {
        goto cleanup;
    }

    if (!cJSON_AddBoolToObject(ringButton, "enabled", p->ring_button.enabled) ||
        !cJSON_AddStringToObject(ringButton, "file", p->ring_button.file) ||
        !cJSON_AddNumberToObject(ringButton, "repeatTimes", p->ring_button.repeat_times) ||
        !cJSON_AddNumberToObject(ringButton, "volume", p->ring_button.volume) ||
        !cJSON_AddStringToObject(ringButton, "soundStateName", p->ring_button.sound_state_name)) {
            LOG_ERROR("Failed to populate 'ringButton' object");
            goto cleanup;
        }

    json = cJSON_Print(root);

    if (!json) {
        goto cleanup;
    }

    if (!utils_write_file(path, json)) {
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