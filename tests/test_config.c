#include "third_party/unity/unity.h"
#include "config.h"


void setUp(void) {
}

void tearDown(void) {
}

void test_config_loads_valid_json(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config_valid.json", &cfg));
    config_free(&cfg);
}

void test_config_fails_on_missing_file(void) {
    config_t cfg = {0};
    TEST_ASSERT_FALSE(config_load("tests/fixtures/does_not_exist.json", &cfg));
    config_free(&cfg);
}

void test_config_fails_on_invalid_json(void) {
    config_t cfg = {0};
    TEST_ASSERT_FALSE(config_load("tests/fixtures/config_invalid.json", &cfg));
    config_free(&cfg);
}

void test_config_loads_presets(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config_valid.json", &cfg));
    TEST_ASSERT_EQUAL_INT(3, cfg.preset_cfg.count);
    config_free(&cfg);
}

void test_config_does_not_load_presets_when_invalid_preset(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config_invalid_presets.json", &cfg));
    TEST_ASSERT_EQUAL_INT(0, cfg.preset_cfg.count);
    config_free(&cfg);
}

void test_config_does_not_load_presets_when_duplicates(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config_duplicate_presets.json", &cfg));
    TEST_ASSERT_EQUAL_INT(0, cfg.preset_cfg.count);
    config_free(&cfg);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_config_loads_valid_json);
    RUN_TEST(test_config_fails_on_missing_file);
    RUN_TEST(test_config_fails_on_invalid_json);
    RUN_TEST(test_config_loads_presets);
    RUN_TEST(test_config_does_not_load_presets_when_invalid_preset);
    RUN_TEST(test_config_does_not_load_presets_when_duplicates);

    return UNITY_END();
}

