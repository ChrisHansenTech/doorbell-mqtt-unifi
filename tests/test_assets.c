#include "unity.h"
#include "config.h"

extern config_t g_cfg;

void test_assets_init_fails_for_missing_dir(void) {
    TEST_ASSERT_FALSE(assets_init("/no/such/dir", &g_cfg));
}

void test_assets_init_succeeds_for_valid_dir(void) {
    TEST_ASSERT_TRUE(assets_init("tests/fixtures/assets_ok", &g_cfg));
}

void test_assets_resolve_holiday_returns_valid_asset_set(void) {    
    assets_init("tests/fixtures/assets_ok", &g_cfg);
    
    asset_set_t s = {0};

    int result = assets_resolve_holiday("Christmas", &s);

    TEST_ASSERT_EQUAL_INT(ASSETS_OK, result);

    TEST_ASSERT_EQUAL_STRING("tests/fixtures/assets_ok/christmas/christmas1.png", s.image_path);
    TEST_ASSERT_EQUAL_STRING("tests/fixtures/assets_ok/christmas/christmas.ogg", s.sound_path);
    TEST_ASSERT_EQUAL_STRING("tests/fixtures/assets_ok/christmas/christmas.json", s.json_path);

    assets_free(&s);
    assets_shutdown();
}

void test_assets_resolve_holiday_returns_image_notfound(void) {
    TEST_ASSERT_TRUE(assets_init("tests/fixtures/assets_ok", &g_cfg));

    asset_set_t s = {0};

    assets_result_t result = assets_resolve_holiday("Labor Day", &s);

    TEST_ASSERT_EQUAL_INT(ASSETS_ERR_IMG_NOT_FOUND, result);
}