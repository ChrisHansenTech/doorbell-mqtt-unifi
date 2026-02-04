#include "third_party/unity/unity.h"
#include "support/test_config_mock.h"
#include "config.h"
#include "config_types.h"
#include "unifi_profiles_repo.h"

static config_t g_cfg;

void setUp(void) { 
    make_mock_config(&g_cfg); 
} 

void tearDown(void) { 
    config_free(&g_cfg); 
}

void test_unifi_profiles_repo_init_fails_for_missing_dir(void) {
    TEST_ASSERT_FALSE(profiles_repo_init("/no/such/dir", &g_cfg.preset_cfg));
}

void test_unifi_profiles_repo_init_succeeds_for_valid_dir(void) {
    TEST_ASSERT_TRUE(profiles_repo_init("tests/fixtures/profiles", &g_cfg.preset_cfg));
}

void test_unifi_profiles_repo_resolve_preset_returns_valid_profile_directory(void) {    
    profiles_repo_init("tests/fixtures/profiles", &g_cfg.preset_cfg);
    
    char profile_dir[PATH_MAX];

    bool result = profiles_repo_resolve_preset("Christmas", profile_dir, sizeof(profile_dir));

    TEST_ASSERT_TRUE(result);

    TEST_ASSERT_EQUAL_STRING("tests/fixtures/profiles/christmas", profile_dir);

    profiles_repo_shutdown();
}

void test_unifi_profiles_repo_resolve_custom_returns_valid_profile_directory(void) {
    profiles_repo_init("tests/fixtures/profiles", &g_cfg.preset_cfg);

    char profile_dir[PATH_MAX];

    bool result = profiles_repo_resolve_custom("custom", profile_dir, sizeof(profile_dir));

    TEST_ASSERT_TRUE(result);

    TEST_ASSERT_EQUAL_STRING("tests/fixtures/profiles/custom", profile_dir);

    profiles_repo_shutdown();
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_unifi_profiles_repo_init_fails_for_missing_dir);
    RUN_TEST(test_unifi_profiles_repo_init_succeeds_for_valid_dir);
    RUN_TEST(test_unifi_profiles_repo_resolve_preset_returns_valid_profile_directory);
    RUN_TEST(test_unifi_profiles_repo_resolve_custom_returns_valid_profile_directory);

    return UNITY_END();
}