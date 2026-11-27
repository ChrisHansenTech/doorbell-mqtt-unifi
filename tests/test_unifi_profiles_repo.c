#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "support/test_config_mock.h"
#include "support/test_helpers.h"
#include "config.h"
#include "config_types.h"
#include "unifi_profiles_repo.h"
#include "utils.h"

#include <linux/limits.h>
#include <stdio.h>
#include <string.h>

static config_t g_cfg;
static char g_tmp_dir[128];

void setUp(void) {
    memset(&g_cfg, 0, sizeof(g_cfg));
    make_mock_config(&g_cfg);
    TEST_ASSERT_TRUE(test_make_temp_dir(g_tmp_dir, sizeof(g_tmp_dir)));
}

void tearDown(void) {
    profiles_repo_shutdown();
    config_free(&g_cfg);
    test_remove_dir_recursive(g_tmp_dir);
}

/* ===== Existing baseline tests (retained) ===== */

void test_unifi_profiles_repo_init_fails_for_missing_dir(void) {
    TEST_ASSERT_FALSE(profiles_repo_init("/no/such/dir", &g_cfg.preset_cfg));
}

void test_unifi_profiles_repo_init_succeeds_for_valid_dir(void) {
    TEST_ASSERT_TRUE(profiles_repo_init("tests/fixtures/profiles", &g_cfg.preset_cfg));
}

void test_unifi_profiles_repo_resolve_preset_returns_valid_profile_directory(void) {
    TEST_ASSERT_TRUE(profiles_repo_init("tests/fixtures/profiles", &g_cfg.preset_cfg));

    char profile_dir[PATH_MAX];
    bool result = profiles_repo_resolve_preset("Christmas", profile_dir, sizeof(profile_dir));

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("tests/fixtures/profiles/christmas", profile_dir);
}

void test_unifi_profiles_repo_resolve_custom_returns_valid_profile_directory(void) {
    TEST_ASSERT_TRUE(profiles_repo_init("tests/fixtures/profiles", &g_cfg.preset_cfg));

    char profile_dir[PATH_MAX];
    bool result = profiles_repo_resolve_custom("custom", profile_dir, sizeof(profile_dir));

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("tests/fixtures/profiles/custom", profile_dir);
}

/* ===== Expanded tests ===== */

void test_unifi_profiles_repo_double_init_fails(void) {
    TEST_ASSERT_TRUE(profiles_repo_init(g_tmp_dir, &g_cfg.preset_cfg));
    TEST_ASSERT_FALSE(profiles_repo_init(g_tmp_dir, &g_cfg.preset_cfg));
}

void test_unifi_profiles_repo_operations_fail_before_init(void) {
    char out[PATH_MAX];
    char out2[PATH_MAX];
    time_t now = 1700000000;

    TEST_ASSERT_FALSE(profiles_repo_resolve_preset("Christmas", out, sizeof(out)));
    TEST_ASSERT_FALSE(profiles_repo_resolve_custom("custom", out, sizeof(out)));
    TEST_ASSERT_FALSE(profiles_repo_create_temp_profile_dir(out, sizeof(out)));
    TEST_ASSERT_FALSE(profiles_repo_rename_temp_profile_dir("/tmp/x", &now, false, out, sizeof(out), out2, sizeof(out2)));
}

void test_unifi_profiles_repo_resolve_preset_case_insensitive_unknown_and_errors(void) {
    TEST_ASSERT_TRUE(profiles_repo_init(g_tmp_dir, &g_cfg.preset_cfg));

    char out[PATH_MAX];
    TEST_ASSERT_TRUE(profiles_repo_resolve_preset("labor day", out, sizeof(out)));

    char expected[PATH_MAX];
    snprintf(expected, sizeof(expected), "%s/labor_day", g_tmp_dir);
    TEST_ASSERT_EQUAL_STRING(expected, out);

    TEST_ASSERT_FALSE(profiles_repo_resolve_preset("Unknown Preset", out, sizeof(out)));
    TEST_ASSERT_FALSE(profiles_repo_resolve_preset(NULL, out, sizeof(out)));
    TEST_ASSERT_FALSE(profiles_repo_resolve_preset("Christmas", NULL, sizeof(out)));

    char tiny[8];
    TEST_ASSERT_FALSE(profiles_repo_resolve_preset("Christmas", tiny, sizeof(tiny)));
}

void test_unifi_profiles_repo_resolve_custom_errors(void) {
    TEST_ASSERT_TRUE(profiles_repo_init(g_tmp_dir, &g_cfg.preset_cfg));

    char out[PATH_MAX];
    TEST_ASSERT_FALSE(profiles_repo_resolve_custom(NULL, out, sizeof(out)));
    TEST_ASSERT_FALSE(profiles_repo_resolve_custom("custom", NULL, sizeof(out)));

    char tiny[4];
    TEST_ASSERT_FALSE(profiles_repo_resolve_custom("custom", tiny, sizeof(tiny)));
}

void test_unifi_profiles_repo_temp_dir_create_and_rename_complete(void) {
    TEST_ASSERT_TRUE(profiles_repo_init(g_tmp_dir, &g_cfg.preset_cfg));

    TEST_ASSERT_FALSE(profiles_repo_create_temp_profile_dir(NULL, 64));

    char temp_dir[256];
    TEST_ASSERT_TRUE(profiles_repo_create_temp_profile_dir(temp_dir, sizeof(temp_dir)));
    TEST_ASSERT_TRUE(utils_directory_exists(temp_dir));

    /* Write a marker file inside temp_dir to verify move preserves contents */
    char marker[PATH_MAX];
    snprintf(marker, sizeof(marker), "%s/profile.json", temp_dir);
    TEST_ASSERT_TRUE(test_write_text(marker, "{}"));

    time_t ts = 1700000000; /* 20231114_221320 */
    char final_dir[64];
    char final_path[512];

    TEST_ASSERT_TRUE(profiles_repo_rename_temp_profile_dir(
        temp_dir, &ts, false, final_dir, sizeof(final_dir), final_path, sizeof(final_path)
    ));

    TEST_ASSERT_EQUAL_STRING("20231114_221320", final_dir);
    char expected_path[PATH_MAX];
    snprintf(expected_path, sizeof(expected_path), "%s/downloads/20231114_221320", g_tmp_dir);
    TEST_ASSERT_EQUAL_STRING(expected_path, final_path);
    TEST_ASSERT_TRUE(utils_directory_exists(final_path));
    TEST_ASSERT_FALSE(utils_directory_exists(temp_dir));

    char moved_marker[PATH_MAX];
    snprintf(moved_marker, sizeof(moved_marker), "%s/profile.json", final_path);
    TEST_ASSERT_TRUE(utils_file_exists(moved_marker));
}

void test_unifi_profiles_repo_temp_dir_rename_partial_and_errors(void) {
    TEST_ASSERT_TRUE(profiles_repo_init(g_tmp_dir, &g_cfg.preset_cfg));

    char temp_dir[PATH_MAX];
    TEST_ASSERT_TRUE(profiles_repo_create_temp_profile_dir(temp_dir, sizeof(temp_dir)));

    time_t ts = 1700000001; /* 20231114_221321 */
    char final_dir[64];
    char final_path[PATH_MAX];

    TEST_ASSERT_FALSE(profiles_repo_rename_temp_profile_dir(
        NULL, &ts, true, final_dir, sizeof(final_dir), final_path, sizeof(final_path)
    ));
    TEST_ASSERT_FALSE(profiles_repo_rename_temp_profile_dir(
        "/no/such/temp/dir", &ts, true, final_dir, sizeof(final_dir), final_path, sizeof(final_path)
    ));

    TEST_ASSERT_TRUE(profiles_repo_rename_temp_profile_dir(
        temp_dir, &ts, true, final_dir, sizeof(final_dir), final_path, sizeof(final_path)
    ));

    char expected_path[PATH_MAX];
    snprintf(expected_path, sizeof(expected_path), "%s/partial/20231114_221321", g_tmp_dir);
    TEST_ASSERT_EQUAL_STRING(expected_path, final_path);
    TEST_ASSERT_TRUE(utils_directory_exists(final_path));
}

int main(void) {
    UNITY_BEGIN();

    /* Existing baseline tests */
    RUN_TEST(test_unifi_profiles_repo_init_fails_for_missing_dir);
    RUN_TEST(test_unifi_profiles_repo_init_succeeds_for_valid_dir);
    RUN_TEST(test_unifi_profiles_repo_resolve_preset_returns_valid_profile_directory);
    RUN_TEST(test_unifi_profiles_repo_resolve_custom_returns_valid_profile_directory);

    /* Expanded tests */
    RUN_TEST(test_unifi_profiles_repo_double_init_fails);
    RUN_TEST(test_unifi_profiles_repo_operations_fail_before_init);
    RUN_TEST(test_unifi_profiles_repo_resolve_preset_case_insensitive_unknown_and_errors);
    RUN_TEST(test_unifi_profiles_repo_resolve_custom_errors);
    RUN_TEST(test_unifi_profiles_repo_temp_dir_create_and_rename_complete);
    RUN_TEST(test_unifi_profiles_repo_temp_dir_rename_partial_and_errors);

    return UNITY_END();
}
