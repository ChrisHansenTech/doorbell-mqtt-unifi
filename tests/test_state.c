#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "support/test_helpers.h"
#include "errors.h"
#include "state.h"
#include "state_types.h"
#include "utils.h"

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_tmp_dir[128];

void setUp(void) {
    TEST_ASSERT_TRUE(test_make_temp_dir(g_tmp_dir, sizeof(g_tmp_dir)));
}

void tearDown(void) {
    test_remove_dir_recursive(g_tmp_dir);
}

static profile_state_t make_sample_state(bool reversed_order) {
    profile_state_t st = {0};

    st.custom_animation_count = 2;
    st.custom_animations = calloc(2, sizeof(profile_animation_t));

    size_t a0 = reversed_order ? 1 : 0;
    size_t a1 = reversed_order ? 0 : 1;

    st.custom_animations[a0].gui_id = strdup("WELCOME");
    st.custom_animations[a0].file = strdup("christmas.png");
    st.custom_animations[a0].count = 57;
    st.custom_animations[a0].duration_ms = 3240;
    st.custom_animations[a0].enable = true;
    st.custom_animations[a0].loop = true;

    st.custom_animations[a1].gui_id = strdup("ALPHA");
    st.custom_animations[a1].file = strdup("alpha.png");
    st.custom_animations[a1].count = 10;
    st.custom_animations[a1].duration_ms = 500;
    st.custom_animations[a1].enable = false;
    st.custom_animations[a1].loop = false;

    st.custom_sound_count = 2;
    st.custom_sounds = calloc(2, sizeof(profile_sound_t));

    size_t s0 = reversed_order ? 1 : 0;
    size_t s1 = reversed_order ? 0 : 1;

    st.custom_sounds[s0].sound_state_name = strdup("RING_BUTTON_PRESSED");
    st.custom_sounds[s0].file = strdup("christmas.ogg");
    st.custom_sounds[s0].repeat_times = 1;
    st.custom_sounds[s0].volume = 100;
    st.custom_sounds[s0].enable = true;

    st.custom_sounds[s1].sound_state_name = strdup("ALARM_TRIGGERED");
    st.custom_sounds[s1].file = strdup("alarm.wav");
    st.custom_sounds[s1].repeat_times = 2;
    st.custom_sounds[s1].volume = 80;
    st.custom_sounds[s1].enable = false;

    return st;
}

void test_state_save_and_load_ipc_roundtrip(void) {
    char state_dir[PATH_MAX];
    snprintf(state_dir, sizeof(state_dir), "%s/state", g_tmp_dir);

    profile_state_t ps = make_sample_state(false);

    int rc = state_save(&ps, "Christmas", true, UNIFI_APPLY_IPC, state_dir);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, rc);

    applied_state_t loaded = {0};
    rc = state_load(&loaded, state_dir);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, rc);
    TEST_ASSERT_EQUAL_STRING("Christmas", loaded.profile_name);
    TEST_ASSERT_TRUE(loaded.is_preset);
    TEST_ASSERT_EQUAL_INT(UNIFI_APPLY_IPC, loaded.apply_method);
    TEST_ASSERT_TRUE(loaded.applied_at > 0);
    TEST_ASSERT_NOT_NULL(loaded.hash);
    TEST_ASSERT_EQUAL_UINT(64, strlen(loaded.hash));

    state_applied_free(&loaded);
    state_free_profile_state(&ps);
}

void test_state_save_and_load_legacy_roundtrip(void) {
    profile_state_t ps = {0};

    int rc = state_save(&ps, "custom_dir", false, UNIFI_APPLY_LEGACY, g_tmp_dir);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, rc);

    applied_state_t loaded = {0};
    rc = state_load(&loaded, g_tmp_dir);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, rc);
    TEST_ASSERT_EQUAL_STRING("custom_dir", loaded.profile_name);
    TEST_ASSERT_FALSE(loaded.is_preset);
    TEST_ASSERT_EQUAL_INT(UNIFI_APPLY_LEGACY, loaded.apply_method);
    TEST_ASSERT_EQUAL_STRING("", loaded.hash);

    state_applied_free(&loaded);
}

void test_state_compare_order_independent_match(void) {
    profile_state_t saved_ps = make_sample_state(false);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, state_save(&saved_ps, "Christmas", true, UNIFI_APPLY_IPC, g_tmp_dir));

    /* Active state has animations and sounds in reversed order — sort must normalize hash */
    profile_state_t active_ps = make_sample_state(true);
    applied_state_t loaded = {0};
    char active_hash[65] = {0};

    int rc = state_compare(&active_ps, &loaded, active_hash, g_tmp_dir);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, rc);
    TEST_ASSERT_EQUAL_STRING(loaded.hash, active_hash);

    state_applied_free(&loaded);
    state_free_profile_state(&active_ps);
    state_free_profile_state(&saved_ps);
}

void test_state_compare_detects_mismatch(void) {
    profile_state_t saved_ps = make_sample_state(false);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, state_save(&saved_ps, "Christmas", true, UNIFI_APPLY_IPC, g_tmp_dir));

    profile_state_t active_ps = make_sample_state(false);
    active_ps.custom_sounds[0].volume = 50; /* drift! */

    applied_state_t loaded = {0};
    char active_hash[65] = {0};

    int rc = state_compare(&active_ps, &loaded, active_hash, g_tmp_dir);
    TEST_ASSERT_EQUAL_INT(ERROR_STATE_COMPARE_FAILED, rc);
    TEST_ASSERT_TRUE(strcmp(loaded.hash, active_hash) != 0);

    state_applied_free(&loaded);
    state_free_profile_state(&active_ps);
    state_free_profile_state(&saved_ps);
}

void test_state_compare_legacy_returns_validation_unavailable(void) {
    profile_state_t ps = {0};
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, state_save(&ps, "Christmas", true, UNIFI_APPLY_LEGACY, g_tmp_dir));

    applied_state_t loaded = {0};
    char active_hash[65] = {0};
    int rc = state_compare(&ps, &loaded, active_hash, g_tmp_dir);
    TEST_ASSERT_EQUAL_INT(ERROR_STATE_VALIDATION_UNAVAILABLE, rc);

    state_applied_free(&loaded);
}

void test_state_error_and_null_handling(void) {
    applied_state_t loaded = {0};
    profile_state_t ps = {0};
    char active_hash[65] = {0};

    /* NULL checks */
    TEST_ASSERT_EQUAL_INT(ERROR_CONFIG_INVALID, state_load(NULL, g_tmp_dir));
    TEST_ASSERT_EQUAL_INT(ERROR_CONFIG_INVALID, state_load(&loaded, NULL));
    TEST_ASSERT_EQUAL_INT(ERROR_CONFIG_INVALID, state_save(NULL, "p", true, UNIFI_APPLY_IPC, g_tmp_dir));
    TEST_ASSERT_EQUAL_INT(ERROR_CONFIG_INVALID, state_save(&ps, NULL, true, UNIFI_APPLY_IPC, g_tmp_dir));
    TEST_ASSERT_EQUAL_INT(ERROR_CONFIG_INVALID, state_save(&ps, "p", true, UNIFI_APPLY_IPC, NULL));
    TEST_ASSERT_EQUAL_INT(ERROR_CONFIG_INVALID, state_compare(NULL, &loaded, active_hash, g_tmp_dir));
    TEST_ASSERT_EQUAL_INT(ERROR_CONFIG_INVALID, state_compare(&ps, &loaded, active_hash, NULL));

    /* Missing file */
    TEST_ASSERT_EQUAL_INT(ERROR_STATE_FILE_READ_FAILED, state_load(&loaded, g_tmp_dir));
    TEST_ASSERT_EQUAL_INT(ERROR_STATE_FILE_READ_FAILED, state_compare(&ps, &loaded, active_hash, g_tmp_dir));

    /* Unwritable state dir */
    TEST_ASSERT_EQUAL_INT(ERROR_STATE_FILE_WRITE_FAILED, state_save(&ps, "p", true, UNIFI_APPLY_IPC, "/no/such/parent/state"));

    /* Malformed JSON */
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/last_applied.json", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(path, "{bad json"));
    TEST_ASSERT_EQUAL_INT(ERROR_STATE_PARSE_FAILED, state_load(&loaded, g_tmp_dir));

    /* Empty profileName */
    TEST_ASSERT_TRUE(test_write_text(path, "{\"profileName\":\"\",\"applyMethod\":\"ipc\"}"));
    TEST_ASSERT_EQUAL_INT(ERROR_STATE_PARSE_FAILED, state_load(&loaded, g_tmp_dir));
    state_applied_free(&loaded);

    /* Free NULL safe */
    state_applied_free(NULL);
    state_free_profile_state(NULL);
}

void test_state_default_profile_flag_roundtrip_and_backward_compat(void) {
    profile_state_t ps = make_sample_state(false);

    /* Virtual Default profile saves isDefaultProfile=true and loads it back */
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, state_save_ex(&ps, "Default", true, true, UNIFI_APPLY_IPC, g_tmp_dir));

    applied_state_t loaded = {0};
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, state_load(&loaded, g_tmp_dir));
    TEST_ASSERT_EQUAL_STRING("Default", loaded.profile_name);
    TEST_ASSERT_TRUE(loaded.is_preset);
    TEST_ASSERT_TRUE(loaded.is_default_profile);
    state_applied_free(&loaded);

    /* Existing legacy last_applied.json without isDefaultProfile defaults to false (user-defined Default preset) */
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/last_applied.json", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(
        path,
        "{\"schemaVersion\":1,\"profileName\":\"Default\",\"isPreset\":true,\"applyMethod\":\"ipc\",\"appliedAt\":1700000000,\"hash\":\"abc\"}"
    ));
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, state_load(&loaded, g_tmp_dir));
    TEST_ASSERT_EQUAL_STRING("Default", loaded.profile_name);
    TEST_ASSERT_TRUE(loaded.is_preset);
    TEST_ASSERT_FALSE(loaded.is_default_profile);
    state_applied_free(&loaded);

    state_free_profile_state(&ps);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_state_save_and_load_ipc_roundtrip);
    RUN_TEST(test_state_save_and_load_legacy_roundtrip);
    RUN_TEST(test_state_compare_order_independent_match);
    RUN_TEST(test_state_compare_detects_mismatch);
    RUN_TEST(test_state_compare_legacy_returns_validation_unavailable);
    RUN_TEST(test_state_error_and_null_handling);
    RUN_TEST(test_state_default_profile_flag_roundtrip_and_backward_compat);

    return UNITY_END();
}
