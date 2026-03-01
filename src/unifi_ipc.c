#include "unifi_ipc.h"
#include "unifi_profile.h"

#include "cJSON.h"
#include "unifi_profile_conf.h"
#include "utils.h"

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