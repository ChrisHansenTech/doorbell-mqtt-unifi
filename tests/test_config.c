#include "third_party/unity/unity.h"
#include "config.h"
#include "config_types.h"


void setUp(void) {
}

void tearDown(void) {
}

void test_config_loads_valid_json(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config/config_valid.json", &cfg));
    TEST_ASSERT_EQUAL_INT(cfg.unifi_cfg.apply_method, UNIFI_APPLY_IPC);
    config_free(&cfg);
}

void test_config_fails_on_missing_file(void) {
    config_t cfg = {0};
    TEST_ASSERT_FALSE(config_load("tests/fixtures/config/does_not_exist.json", &cfg));
    config_free(&cfg);
}

void test_config_fails_on_invalid_json(void) {
    config_t cfg = {0};
    TEST_ASSERT_FALSE(config_load("tests/fixtures/config/config_invalid.json", &cfg));
    config_free(&cfg);
}

void test_config_loads_presets(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config/config_valid.json", &cfg));
    TEST_ASSERT_EQUAL_INT(3, cfg.preset_cfg.count);
    config_free(&cfg);
}

void test_config_skips_invalid_preset(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config/config_invalid_presets.json", &cfg));
    TEST_ASSERT_EQUAL_INT(2, cfg.preset_cfg.count);
    config_free(&cfg);
}

void test_config_loads_first_entry_when_duplicates(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config/config_duplicate_presets.json", &cfg));
    TEST_ASSERT_EQUAL_INT(2, cfg.preset_cfg.count);
    TEST_ASSERT_EQUAL_STRING("Christmas", cfg.preset_cfg.items[0].display_name);
    config_free(&cfg);
}

void test_config_fails_when_long_string_truncated(void) {
    config_t cfg = {0};
    TEST_ASSERT_FALSE(config_load("tests/fixtures/config/config_invalid_long_strings.json", &cfg));
    config_free(&cfg);
}

void test_config_fails_when_unifi_exists_but_not_an_object(void) {
    config_t cfg = {0};
    TEST_ASSERT_FALSE(config_load("tests/fixtures/config/config_unifi_not_object.json", &cfg));
    config_free(&cfg);
}

void test_config_default_unifi_apply_method_legacy_when_unifi_does_not_exits(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config/config_unifi_object_does_not_exist.json", &cfg));
    TEST_ASSERT_EQUAL_INT(UNIFI_APPLY_LEGACY, cfg.unifi_cfg.apply_method);
    config_free(&cfg);
}

void test_config_skips_sfx_invalid_filenames(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config/config_invalid_presets.json", &cfg));
    TEST_ASSERT_EQUAL_INT(cfg.sfx_preset_cfg.count, 2);
    TEST_ASSERT_EQUAL_STRING("Hello", cfg.sfx_preset_cfg.items[0].name);
    TEST_ASSERT_EQUAL_STRING("Goodbye", cfg.sfx_preset_cfg.items[1].name);
    config_free(&cfg);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_config_loads_valid_json);
    RUN_TEST(test_config_fails_on_missing_file);
    RUN_TEST(test_config_fails_on_invalid_json);
    RUN_TEST(test_config_loads_presets);
    RUN_TEST(test_config_skips_invalid_preset);
    RUN_TEST(test_config_loads_first_entry_when_duplicates);
    RUN_TEST(test_config_fails_when_long_string_truncated);
    RUN_TEST(test_config_fails_when_unifi_exists_but_not_an_object);
    RUN_TEST(test_config_default_unifi_apply_method_legacy_when_unifi_does_not_exits);
    RUN_TEST(test_config_skips_sfx_invalid_filenames);

    return UNITY_END();
}

