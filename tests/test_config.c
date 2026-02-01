#include "unity.h"
#include "config.h"

extern config_t g_cfg;

void test_config_loads_valid_json(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config_valid.json", &cfg));
}

void test_config_fails_on_missing_file(void) {
    config_t cfg = {0};
    TEST_ASSERT_FALSE(config_load("tests/fixtures/does_not_exist.json", &cfg));
}

void test_config_fails_on_invalid_json(void) {
    config_t cfg = {0};
    TEST_ASSERT_FALSE(config_load("tests/fixtures/config_invalid.json", &cfg));
}

void test_config_get_holiday_directory_valid(void) {
    const char *directory = config_get_holiday_directory(&g_cfg, "Christmas");
    TEST_ASSERT_NOT_NULL(directory);
    TEST_ASSERT_EQUAL_STRING("christmas", directory);
}

void test_config_get_holiday_directory_invalid(void) {
    const char *directory = config_get_holiday_directory(&g_cfg, "St Bob's Day");
    TEST_ASSERT_NULL(directory);
}
