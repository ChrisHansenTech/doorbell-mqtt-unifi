#include "state.h"

#include "cJSON.h"
#include "config_types.h"
#include "errors.h"
#include "logger.h"
#include "state_types.h"
#include "utils.h"

#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>


static int compare_animation_gui_id(const void *a, const void *b)
{
    const profile_animation_t *aa = a;
    const profile_animation_t *bb = b;

    const char *ga = aa->gui_id ? aa->gui_id : "";
    const char *gb = bb->gui_id ? bb->gui_id : "";

    return strcmp(ga, gb);
}
static int compare_sound_state_name(const void *a, const void *b)
{
    const profile_sound_t *sa = a;
    const profile_sound_t *sb = b;

    const char *a_name = sa->sound_state_name ? sa->sound_state_name : "";
    const char *b_name = sb->sound_state_name ? sb->sound_state_name : "";

    return strcmp(a_name, b_name);
}

static void profile_state_sort(profile_state_t *state)
{
    if (!state)
        return;

    if (state->custom_animation_count > 1) {
        qsort(state->custom_animations,
              state->custom_animation_count,
              sizeof(profile_animation_t),
              compare_animation_gui_id);
    }

    if (state->custom_sound_count > 1) {
        qsort(state->custom_sounds,
              state->custom_sound_count,
              sizeof(profile_sound_t),
              compare_sound_state_name);
    }
}

static void sha256_update_string(EVP_MD_CTX *ctx, const char *value) {
    const char *s = value ? value : "";
    EVP_DigestUpdate(ctx, s, strlen(s));
}

static void sha256_update_sep(EVP_MD_CTX *ctx, const char *sep) {
    EVP_DigestUpdate(ctx, sep, strlen(sep));
}

static void sha256_update_int(EVP_MD_CTX *ctx, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    EVP_DigestUpdate(ctx, buf, strlen(buf));
}

static void sha256_update_bool(EVP_MD_CTX *ctx, bool value) {
    const char *s = value ? "1" : "0";
    EVP_DigestUpdate(ctx, s, 1);
}

static bool profile_state_compute_hash(const profile_state_t *state, char out_hash[65]) {
    if (!state) {
        return false;
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return false;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    for (size_t i = 0; i < state->custom_animation_count; i++) {
        const profile_animation_t *a = &state->custom_animations[i];

        sha256_update_sep(ctx, "A|");
        sha256_update_string(ctx, a->gui_id);
        sha256_update_sep(ctx, "|");
        sha256_update_string(ctx, a->file);
        sha256_update_sep(ctx, "|");
        sha256_update_int(ctx, a->count);
        sha256_update_sep(ctx, "|");
        sha256_update_int(ctx, a->duration_ms);
        sha256_update_sep(ctx, "|");
        sha256_update_bool(ctx, a->enable);
        sha256_update_sep(ctx, "|");
        sha256_update_bool(ctx, a->loop);
        sha256_update_sep(ctx, "\n");
    }

    for (size_t i = 0; i < state->custom_sound_count; i++) {
        const profile_sound_t *s = &state->custom_sounds[i];

        sha256_update_sep(ctx, "S|");
        sha256_update_string(ctx, s->sound_state_name);
        sha256_update_sep(ctx, "|");
        sha256_update_string(ctx, s->file);
        sha256_update_sep(ctx, "|");
        sha256_update_int(ctx, s->repeat_times);
        sha256_update_sep(ctx, "|");
        sha256_update_int(ctx, s->volume);
        sha256_update_sep(ctx, "|");
        sha256_update_bool(ctx, s->enable);
        sha256_update_sep(ctx, "\n");
    }

    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    for (size_t i = 0; i < digest_len; i++) {
        sprintf(out_hash + (i * 2), "%02x", digest[i]);
    }
    out_hash[64] = '\0';

    return true;
}

static bool state_create_json(applied_state_t *state, const char *state_dir) {

    if (!utils_create_directory(state_dir)) {
        LOG_ERROR("Failed to create directory '%s'", state_dir);
        return false;
    }

    char last_applied_path[PATH_MAX];
    if (!utils_build_path(last_applied_path, sizeof(last_applied_path), state_dir, "last_applied.json")) {
        LOG_ERROR("Failed to create path for '%s/last_applied.json", state_dir);
        return false;
    }

    bool ok = false;
    char *json = NULL;

    cJSON *root = cJSON_CreateObject();
    
    if (!root) {
        LOG_ERROR("Failed to create 'root' object for applied state");
        return false;
    }

    if (!cJSON_AddNumberToObject(root, "schemaVersion", 1) ||
        !cJSON_AddStringToObject(root, "profileName", state->profile_name) ||
        !cJSON_AddBoolToObject(root, "isPreset", state->is_preset) ||
        !cJSON_AddStringToObject(root, "applyMethod", state->apply_method == UNIFI_APPLY_IPC ? "ipc" : "legacy") ||
        !cJSON_AddNumberToObject(root, "appliedAt", (double)state->applied_at) ||
        !cJSON_AddStringToObject(root, "hash", !state->hash ? "" : state->hash)) {
            LOG_ERROR("Failed to populate last applied state object");
            goto cleanup;
    }

    json = cJSON_Print(root);

    if (!json) {
        LOG_ERROR("Failed to create JSON string");
        goto cleanup;
    }

    if (!utils_write_file(last_applied_path, json)) {
        goto cleanup;
    }

    ok = true;
cleanup:
    if (json) {
        cJSON_free(json);
    }
    
    cJSON_Delete(root);

    return ok;
}

int state_load(applied_state_t *state, const char *state_dir) {
    if (!state || !state_dir) {
        LOG_ERROR("Invalid parameters state=%p, state_dir=%p", (void*)state, (void*)state_dir);
        return ERROR_CONFIG_INVALID;
    }

    return 0;
}

int state_save(profile_state_t *profile_state, const char *profile_name, bool is_preset, unifi_apply_method_t apply_method, const char *state_dir) {
    if (!profile_state || !profile_name || !state_dir) {
        LOG_ERROR("Invalid parameters profile_state=%p, profile_name=%p, state_dir==%p", (void*)profile_state, (void*)profile_name, (void*)state_dir);
        return ERROR_CONFIG_INVALID;
    }

    int rc = ERROR_NONE;
    applied_state_t state = {0};
    time_t applied_at = time(NULL);
    char hash[65] = {0};

    if (apply_method == UNIFI_APPLY_IPC) {
        profile_state_sort(profile_state);
        
        if (!profile_state_compute_hash(profile_state, hash)) {
            LOG_WARN("Failed to compute profile state hash; validation will be unavailable");
            state.hash = NULL;
        } else {
            state.hash = strdup(hash);
        }
    }

    state.profile_name = strdup(profile_name);
    state.applied_at = applied_at;
    state.is_preset = is_preset;
    state.apply_method = apply_method;

    if (!state_create_json(&state, state_dir)) {
        LOG_WARN("Failed to write 'last_applied.json; validation and state will be unavailable");
        rc = ERROR_STATE_FILE_WRITE_FAILED;
        goto cleanup;
    }

cleanup:
    state_applied_free(&state);

    return rc;
}

int state_compare(profile_state_t *active_state) {
    if (!active_state) {
        LOG_ERROR("Invalid parameters state=%p", (void*)active_state);
        return ERROR_CONFIG_INVALID;
    }

    return ERROR_NONE;
}

void state_applied_free(applied_state_t *state) {
    if (!state) {
        return;
    }

    free(state->profile_name);
    state->profile_name = NULL;

    free(state->hash);
    state->hash = NULL;

    state_free_profile_state(&state->state);
}

void state_free_profile_state(profile_state_t *state) {
    if (!state) {
        return;
    }

    for (size_t i = 0; i < state->custom_animation_count; i++) {
        free(state->custom_animations[i].file);
        state->custom_animations[i].file = NULL;

        free(state->custom_animations[i].gui_id);
        state->custom_animations[i].gui_id = NULL;
    }

    free(state->custom_animations);
    state->custom_animations = NULL;
    state->custom_animation_count = 0;

    for (size_t i = 0; i < state->custom_sound_count; i++) {
        free(state->custom_sounds[i].file);
        state->custom_sounds[i].file = NULL;

        free(state->custom_sounds[i].sound_state_name);
        state->custom_sounds[i].sound_state_name = NULL;
    }

    free(state->custom_sounds);
    state->custom_sounds = NULL;
    state->custom_sound_count = 0;
}