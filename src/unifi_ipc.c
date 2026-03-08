#include "unifi_ipc.h"
#include "state_types.h"
#include "unifi_profile.h"

#include "cJSON.h"
#include "unifi_profile_conf.h"
#include "utils.h"
#include "utils_json.h"
#include <stdlib.h>
#include <string.h>

static bool parse_custom_animations(cJSON *arr, profile_state_t *state) {
    if (!cJSON_IsArray(arr)) {
        return false;
    }

    size_t count = cJSON_GetArraySize(arr);

    state->custom_animation_count = count;
    state->custom_animations = calloc(count, sizeof(profile_animation_t));

    if (!state->custom_animations) {
        return false;
    }

    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        profile_animation_t *anim = &state->custom_animations[i];

        if (!json_get_int(item, "count", &anim->count)) {
            anim->count = 0;
        }

        if (!json_get_int(item, "durationMs", &anim->duration_ms)) {
            anim->duration_ms = 0;
        }

        if (!json_get_bool(item, "enable", &anim->enable)) {
            anim->enable = false;
        }

        if (!json_get_bool(item, "loop", &anim->loop)) {
            anim->loop = false;
        }

        anim->file = strdup(json_get_string(item, "file"));
        anim->gui_id = strdup(json_get_string(item, "guiId"));
    }

    return true;
}

static bool parse_custom_sounds(cJSON *arr, profile_state_t *state) {
    if (!cJSON_IsArray(arr)) {
        return false;
    }

    size_t count = cJSON_GetArraySize(arr);

    state->custom_sound_count = count;
    state->custom_sounds = calloc(count, sizeof(profile_sound_t));

    if (!state->custom_sounds) {
        return false;
    }

    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        profile_sound_t *sound = &state->custom_sounds[i];

        if (!json_get_int(item, "volume", &sound->volume)) {
            sound->volume = 0;
        }

        if (!json_get_int(item, "repeatTimes", &sound->repeat_times)) {
            sound->repeat_times = 0;
        }

        if (!json_get_bool(item, "enable", &sound->enable)) {
            sound->enable = false;
        }

        sound->file = strdup(json_get_string(item, "file"));
        sound->sound_state_name = strdup(json_get_string(item, "soundStateName"));
    }

    return true;
}

bool unifi_ipc_build_lcm_gui_message(const char *out_path, const unifi_profile_t *profile) {
    if (!out_path || !profile) {
        return false;
    }

    cJSON *root = cJSON_CreateObject();
    char *json = NULL;
    bool result = false;

    if (!root) {
        return result;
    }

    cJSON_AddStringToObject(root, "functionName", "ChangeLcmGuiSettings");
    
    cJSON *custom_animations = unifi_profile_build_custom_animations_array(profile);

    if (!custom_animations) {
        goto cleanup;
    }

    cJSON_AddItemToObject(root, "customAnimations", custom_animations);

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

bool unifi_ipc_build_sounds_leds_message(const char *out_path,const unifi_profile_t *profile) {
    if (!out_path || !profile) {
        return false;
    }
    
    cJSON *root = cJSON_CreateObject();
    char *json = NULL;
    bool result = false;

    if (!root) {
        return result;
    }

    cJSON_AddStringToObject(root, "functionName", "ChangeSoundLedSettings");
    
    cJSON *custom_sounds = unifi_profile_build_custom_sounds_array(profile);

    if (!custom_sounds) {
        goto cleanup;
    }

    cJSON_AddItemToObject(root, "customSounds", custom_sounds);

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

bool unifi_ipc_parse_apply_response(unifi_ipc_raw_t *ipc_state, profile_state_t *out_state) {
    bool ok = false;
    cJSON *lcm_root = NULL;
    cJSON *sound_root = NULL;

    lcm_root = cJSON_Parse(ipc_state->lcm_gui_json);
    if (!lcm_root) {
        goto cleanup;
    }

    cJSON *lcm_response = cJSON_GetObjectItem(lcm_root, "response");
    cJSON *lcm_payload = cJSON_GetObjectItem(lcm_response, "payload");
    cJSON *lcm_arr = cJSON_GetObjectItem(lcm_payload, "customAnimations");

    if (!parse_custom_animations(lcm_arr, out_state)) {
        goto cleanup;
    }

    sound_root = cJSON_Parse(ipc_state->sounds_leds_json);
    if (!sound_root) {
        goto cleanup;
    }

    cJSON *sound_response = cJSON_GetObjectItem(sound_root, "response");
    cJSON *sound_payload = cJSON_GetObjectItem(sound_response, "payload");
    cJSON *sound_arr = cJSON_GetObjectItem(sound_payload, "customSounds");

    if (!parse_custom_sounds(sound_arr, out_state)) {
        goto cleanup;
    }

    ok = true;

cleanup:
    if (lcm_root) {
        cJSON_Delete(lcm_root);
    }
    
    if (sound_root) {
        cJSON_Delete(sound_root);
    }

    return ok;

}

bool unifi_ipc_parse_validate_response(unifi_ipc_raw_t *ipc_state, profile_state_t *out_state) {
    bool ok = false;
    cJSON *lcm_root = NULL;
    cJSON *sound_root = NULL;

    lcm_root = cJSON_Parse(ipc_state->lcm_gui_json);

    if (!lcm_root) {
        goto cleanup;
    }

    if (!parse_custom_animations(lcm_root, out_state)) {
        goto cleanup;
    }

    sound_root = cJSON_Parse(ipc_state->sounds_leds_json);

    if (!sound_root) {
        goto cleanup;
    }

    if (!parse_custom_sounds(sound_root, out_state)) {
        goto cleanup;
    }

    ok = true;
cleanup:
    if (lcm_root) {
        cJSON_Delete(lcm_root);
    }
    
    if (sound_root) {
        cJSON_Delete(sound_root);
    }

    return ok;
}

void unifi_ipc_raw_free(unifi_ipc_raw_t *ipc_state) {
    if (!ipc_state) {
        return;
    }

    free(ipc_state->lcm_gui_json);
    free(ipc_state->sounds_leds_json);

    ipc_state->lcm_gui_json = NULL;
    ipc_state->sounds_leds_json = NULL;
}