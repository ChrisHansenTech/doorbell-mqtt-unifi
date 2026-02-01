#include "unity.h"
#include "config.h"
#include "test_config_mock.h"

// Declare test functions that live in other files
void test_assets_init_fails_for_missing_dir(void);
void test_assets_init_succeeds_for_valid_dir(void);
void test_assets_resolve_holiday_returns_valid_asset_set(void);
void test_assets_resolve_holiday_returns_image_notfound(void);

void test_config_loads_valid_json(void);
void test_config_fails_on_missing_file(void);
void test_config_fails_on_invalid_json(void);
void test_config_get_holiday_directory_valid(void);
void test_config_get_holiday_directory_invalid(void);


config_t g_cfg;

void setUp(void) {
    make_mock_config(&g_cfg);
}

void tearDown(void) {
    config_free(&g_cfg);
}

int main(void) {
    UNITY_BEGIN();

    // assets.c tests
    RUN_TEST(test_assets_init_fails_for_missing_dir);
    RUN_TEST(test_assets_init_succeeds_for_valid_dir);
    RUN_TEST(test_assets_resolve_holiday_returns_valid_asset_set);
    RUN_TEST(test_assets_resolve_holiday_returns_image_notfound);

    // config.c tests
    RUN_TEST(test_config_loads_valid_json);
    RUN_TEST(test_config_fails_on_missing_file);
    RUN_TEST(test_config_fails_on_invalid_json);
    RUN_TEST(test_config_get_holiday_directory_valid);
    RUN_TEST(test_config_get_holiday_directory_invalid);

    return UNITY_END();
}
