#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "support/test_helpers.h"
#include "config.h"
#include "config_types.h"

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_tmp_dir[128];

static const char *const k_config_env_vars[] = {
    "MQTT_HOST",
    "MQTT_PORT",
    "MQTT_TLS_ENABLED",
    "MQTT_PREFIX",
    "MQTT_INSTANCE",
    "MQTT_CLIENT_ID",
    "MQTT_USERNAME",
    "MQTT_PASSWORD",
    "MQTT_QOS",
    "MQTT_KEEPALIVE",
    "MQTT_CLEAN_SESSION",
    "MQTT_RETAINED_ONLINE",
    "MQTT_CAFILE",
    "MQTT_CERT_FILE",
    "MQTT_KEY_FILE",
    "MQTT_KEYPASS",
    "SSH_HOST",
    "SSH_PORT",
    "SSH_USERNAME",
    "UNIFI_APPLY_METHOD",
    "SFX_DEFAULT_VOLUME"
};

static void clear_config_env(void) {
    for (size_t i = 0; i < sizeof(k_config_env_vars) / sizeof(k_config_env_vars[0]); i++) {
        unsetenv(k_config_env_vars[i]);
    }
}

void setUp(void) {
    clear_config_env();
    TEST_ASSERT_TRUE(test_make_temp_dir(g_tmp_dir, sizeof(g_tmp_dir)));
}

void tearDown(void) {
    clear_config_env();
    test_remove_dir_recursive(g_tmp_dir);
}

/* ===== Existing baseline tests (retained) ===== */

void test_config_loads_valid_json(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config/config_valid.json", &cfg));
    TEST_ASSERT_EQUAL_INT(UNIFI_APPLY_IPC, cfg.unifi_cfg.apply_method);
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
    TEST_ASSERT_EQUAL_UINT(3, cfg.preset_cfg.count);
    config_free(&cfg);
}

void test_config_skips_invalid_preset(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config/config_invalid_presets.json", &cfg));
    TEST_ASSERT_EQUAL_UINT(2, cfg.preset_cfg.count);
    config_free(&cfg);
}

void test_config_loads_first_entry_when_duplicates(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config/config_duplicate_presets.json", &cfg));
    TEST_ASSERT_EQUAL_UINT(2, cfg.preset_cfg.count);
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
    TEST_ASSERT_EQUAL_UINT(2, cfg.sfx_preset_cfg.count);
    TEST_ASSERT_EQUAL_STRING("Hello", cfg.sfx_preset_cfg.items[0].name);
    TEST_ASSERT_EQUAL_STRING("Goodbye", cfg.sfx_preset_cfg.items[1].name);
    config_free(&cfg);
}

/* ===== Expanded tests ===== */

void test_config_null_parameters_rejected(void) {
    config_t cfg = {0};
    TEST_ASSERT_FALSE(config_load(NULL, &cfg));
    TEST_ASSERT_FALSE(config_load("tests/fixtures/config/config_valid.json", NULL));
    config_free(NULL);
}

void test_config_defaults_applied_for_minimal_config(void) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/minimal.json", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(path, "{\"mqtt\":{},\"ssh\":{},\"unifi\":{}}"));

    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load(path, &cfg));

    TEST_ASSERT_EQUAL_STRING("tcp://localhost:1883", cfg.mqtt_cfg.address);
    TEST_ASSERT_EQUAL_STRING("chrishansentech", cfg.mqtt_cfg.prefix);
    TEST_ASSERT_EQUAL_STRING("default", cfg.mqtt_cfg.instance);
    TEST_ASSERT_EQUAL_STRING("Default", cfg.mqtt_cfg.instance_human);
    TEST_ASSERT_EQUAL_STRING("chrishansentech-doorbell-mqtt-unifi-default", cfg.mqtt_cfg.client_id);
    TEST_ASSERT_EQUAL_STRING("", cfg.mqtt_cfg.username);
    TEST_ASSERT_EQUAL_STRING("", cfg.mqtt_cfg.password);
    TEST_ASSERT_EQUAL_INT(1, cfg.mqtt_cfg.qos);
    TEST_ASSERT_EQUAL_INT(30, cfg.mqtt_cfg.keepalive);
    TEST_ASSERT_EQUAL_INT(1, cfg.mqtt_cfg.clean_session);
    TEST_ASSERT_EQUAL_INT(1, cfg.mqtt_cfg.retained_online);
    TEST_ASSERT_EQUAL_INT(0, cfg.mqtt_cfg.tls_enabled);
    TEST_ASSERT_EQUAL_STRING("", cfg.mqtt_cfg.cafile);

    TEST_ASSERT_EQUAL_STRING("localhost", cfg.ssh_cfg.host);
    TEST_ASSERT_EQUAL_INT(22, cfg.ssh_cfg.port);
    TEST_ASSERT_EQUAL_STRING("ubnt", cfg.ssh_cfg.user);
    TEST_ASSERT_EQUAL_STRING("UNIFI_PROTECT_RECOVERY_CODE", cfg.ssh_cfg.password_env);

    TEST_ASSERT_EQUAL_INT(UNIFI_APPLY_IPC, cfg.unifi_cfg.apply_method);
    TEST_ASSERT_EQUAL_UINT(0, cfg.preset_cfg.count);
    TEST_ASSERT_EQUAL_UINT(0, cfg.sfx_preset_cfg.count);
    TEST_ASSERT_EQUAL_INT(100, cfg.sfx_preset_cfg.default_volume);

    config_free(&cfg);
}

void test_config_env_overrides_json_values(void) {
    setenv("MQTT_HOST", "10.0.0.50", 1);
    setenv("MQTT_PORT", "8883", 1);
    setenv("MQTT_TLS_ENABLED", "1", 1);
    setenv("MQTT_PREFIX", "custompfx", 1);
    setenv("MQTT_INSTANCE", "front_door", 1);
    setenv("MQTT_CLIENT_ID", "explicit-client", 1);
    setenv("MQTT_USERNAME", "mqttuser", 1);
    setenv("MQTT_PASSWORD", "mqttsecret", 1);
    setenv("MQTT_QOS", "2", 1);
    setenv("MQTT_KEEPALIVE", "60", 1);
    setenv("MQTT_CLEAN_SESSION", "0", 1);
    setenv("MQTT_RETAINED_ONLINE", "0", 1);
    setenv("MQTT_CAFILE", "/certs/ca.crt", 1);
    setenv("MQTT_CERT_FILE", "/certs/client.crt", 1);
    setenv("MQTT_KEY_FILE", "/certs/client.key", 1);
    setenv("MQTT_KEYPASS", "keysecret", 1);
    setenv("SSH_HOST", "10.0.0.99", 1);
    setenv("SSH_PORT", "2222", 1);
    setenv("SSH_USERNAME", "admin", 1);
    setenv("UNIFI_APPLY_METHOD", "legacy", 1);
    setenv("SFX_DEFAULT_VOLUME", "65", 1);

    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config/config_valid.json", &cfg));

    TEST_ASSERT_EQUAL_STRING("ssl://10.0.0.50:8883", cfg.mqtt_cfg.address);
    TEST_ASSERT_EQUAL_STRING("custompfx", cfg.mqtt_cfg.prefix);
    TEST_ASSERT_EQUAL_STRING("front_door", cfg.mqtt_cfg.instance);
    TEST_ASSERT_EQUAL_STRING("Front Door", cfg.mqtt_cfg.instance_human);
    TEST_ASSERT_EQUAL_STRING("explicit-client", cfg.mqtt_cfg.client_id);
    TEST_ASSERT_EQUAL_STRING("mqttuser", cfg.mqtt_cfg.username);
    TEST_ASSERT_EQUAL_STRING("mqttsecret", cfg.mqtt_cfg.password);
    TEST_ASSERT_EQUAL_INT(2, cfg.mqtt_cfg.qos);
    TEST_ASSERT_EQUAL_INT(60, cfg.mqtt_cfg.keepalive);
    TEST_ASSERT_EQUAL_INT(0, cfg.mqtt_cfg.clean_session);
    TEST_ASSERT_EQUAL_INT(0, cfg.mqtt_cfg.retained_online);
    TEST_ASSERT_EQUAL_INT(1, cfg.mqtt_cfg.tls_enabled);
    TEST_ASSERT_EQUAL_STRING("/certs/ca.crt", cfg.mqtt_cfg.cafile);
    TEST_ASSERT_EQUAL_STRING("/certs/client.crt", cfg.mqtt_cfg.certfile);
    TEST_ASSERT_EQUAL_STRING("/certs/client.key", cfg.mqtt_cfg.keyfile);
    TEST_ASSERT_EQUAL_STRING("keysecret", cfg.mqtt_cfg.keypass);

    TEST_ASSERT_EQUAL_STRING("10.0.0.99", cfg.ssh_cfg.host);
    TEST_ASSERT_EQUAL_INT(2222, cfg.ssh_cfg.port);
    TEST_ASSERT_EQUAL_STRING("admin", cfg.ssh_cfg.user);

    TEST_ASSERT_EQUAL_INT(UNIFI_APPLY_LEGACY, cfg.unifi_cfg.apply_method);
    TEST_ASSERT_EQUAL_INT(65, cfg.sfx_preset_cfg.default_volume);

    config_free(&cfg);
}

void test_config_invalid_int_env_falls_back_to_json(void) {
    setenv("MQTT_PORT", "not_a_number", 1);
    setenv("SSH_PORT", "22abc", 1);

    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config/config_valid.json", &cfg));

    TEST_ASSERT_EQUAL_STRING("tcp://192.168.1.x:1883", cfg.mqtt_cfg.address);
    TEST_ASSERT_EQUAL_INT(22, cfg.ssh_cfg.port);
    config_free(&cfg);
}

void test_config_env_overrides_missing_unifi_section_to_ipc(void) {
    setenv("UNIFI_APPLY_METHOD", "IPC", 1);

    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("tests/fixtures/config/config_unifi_object_does_not_exist.json", &cfg));
    TEST_ASSERT_EQUAL_INT(UNIFI_APPLY_IPC, cfg.unifi_cfg.apply_method);
    config_free(&cfg);
}

void test_config_fails_on_invalid_apply_method(void) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/bad_apply.json", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(path, "{\"mqtt\":{},\"ssh\":{},\"unifi\":{\"apply_method\":\"magic\"}}"));

    config_t cfg = {0};
    TEST_ASSERT_FALSE(config_load(path, &cfg));
    config_free(&cfg);
}

void test_config_fails_on_missing_or_invalid_mqtt_section(void) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/no_mqtt.json", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(path, "{\"ssh\":{}}"));

    config_t cfg = {0};
    TEST_ASSERT_FALSE(config_load(path, &cfg));

    TEST_ASSERT_TRUE(test_write_text(path, "{\"mqtt\":\"bad\",\"ssh\":{}}"));
    TEST_ASSERT_FALSE(config_load(path, &cfg));
    config_free(&cfg);
}

void test_config_fails_on_missing_or_invalid_ssh_section(void) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/no_ssh.json", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(path, "{\"mqtt\":{}}"));

    config_t cfg = {0};
    TEST_ASSERT_FALSE(config_load(path, &cfg));

    TEST_ASSERT_TRUE(test_write_text(path, "{\"mqtt\":{},\"ssh\":123}"));
    TEST_ASSERT_FALSE(config_load(path, &cfg));
    config_free(&cfg);
}

void test_config_fails_when_ssh_user_exceeds_buffer(void) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/long_ssh_user.json", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(
        path,
        "{\"mqtt\":{},\"ssh\":{\"username\":\"this_username_is_definitely_longer_than_thirty_characters\"}}"
    ));

    config_t cfg = {0};
    TEST_ASSERT_FALSE(config_load(path, &cfg));
    config_free(&cfg);
}

void test_config_fails_when_mqtt_host_or_prefix_or_cafile_exceeds_buffer(void) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/minimal.json", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(path, "{\"mqtt\":{},\"ssh\":{}}"));

    config_t cfg = {0};

    setenv("MQTT_HOST", "this-hostname-is-way-longer-than-forty-characters.example.com", 1);
    TEST_ASSERT_FALSE(config_load(path, &cfg));
    unsetenv("MQTT_HOST");

    setenv("MQTT_PREFIX", "this_prefix_is_longer_than_thirty_chars", 1);
    TEST_ASSERT_FALSE(config_load(path, &cfg));
    unsetenv("MQTT_PREFIX");

    char long_path[300];
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';
    setenv("MQTT_CAFILE", long_path, 1);
    TEST_ASSERT_FALSE(config_load(path, &cfg));
    unsetenv("MQTT_CAFILE");

    config_free(&cfg);
}

void test_config_skips_preset_with_invalid_directory_name(void) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/bad_preset_dir.json", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(
        path,
        "{\"mqtt\":{},\"ssh\":{},\"presets\":["
        "{\"name\":\"Good\",\"directory\":\"good_dir\"},"
        "{\"name\":\"Traversal\",\"directory\":\"../escape\"},"
        "{\"name\":\"Hidden\",\"directory\":\".hidden\"}"
        "]}"
    ));

    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load(path, &cfg));
    TEST_ASSERT_EQUAL_UINT(1, cfg.preset_cfg.count);
    TEST_ASSERT_EQUAL_STRING("Good", cfg.preset_cfg.items[0].display_name);
    TEST_ASSERT_EQUAL_STRING("good", cfg.preset_cfg.items[0].key_name);
    TEST_ASSERT_EQUAL_STRING("good_dir", cfg.preset_cfg.items[0].directory);
    config_free(&cfg);
}

void test_config_sfx_volume_rules_and_duplicate_skipping(void) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/sfx_rules.json", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(
        path,
        "{\"mqtt\":{},\"ssh\":{},\"sfx\":{"
        "\"defaultVolume\":75,"
        "\"presets\":["
        "{\"name\":\"Explicit\",\"file\":\"a.ogg\",\"volume\":40},"
        "{\"name\":\"ZeroVol\",\"file\":\"b.wav\",\"volume\":0},"
        "{\"name\":\"OverVol\",\"file\":\"c.ogg\",\"volume\":150},"
        "{\"name\":\"NegVol\",\"file\":\"d.ogg\",\"volume\":-5},"
        "{\"name\":\"DefaultVol\",\"file\":\"e.ogg\"},"
        "{\"name\":\"explicit\",\"file\":\"dup.ogg\",\"volume\":90},"
        "{\"name\":\"MissingFile\"}"
        "]}}"
    ));

    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load(path, &cfg));
    TEST_ASSERT_EQUAL_INT(75, cfg.sfx_preset_cfg.default_volume);
    TEST_ASSERT_EQUAL_UINT(5, cfg.sfx_preset_cfg.count);

    TEST_ASSERT_EQUAL_STRING("Explicit", cfg.sfx_preset_cfg.items[0].name);
    TEST_ASSERT_EQUAL_INT(40, cfg.sfx_preset_cfg.items[0].volume);

    TEST_ASSERT_EQUAL_STRING("ZeroVol", cfg.sfx_preset_cfg.items[1].name);
    TEST_ASSERT_EQUAL_INT(75, cfg.sfx_preset_cfg.items[1].volume);

    TEST_ASSERT_EQUAL_STRING("OverVol", cfg.sfx_preset_cfg.items[2].name);
    TEST_ASSERT_EQUAL_INT(75, cfg.sfx_preset_cfg.items[2].volume);

    TEST_ASSERT_EQUAL_STRING("NegVol", cfg.sfx_preset_cfg.items[3].name);
    TEST_ASSERT_EQUAL_INT(75, cfg.sfx_preset_cfg.items[3].volume);

    TEST_ASSERT_EQUAL_STRING("DefaultVol", cfg.sfx_preset_cfg.items[4].name);
    TEST_ASSERT_EQUAL_INT(75, cfg.sfx_preset_cfg.items[4].volume);

    config_free(&cfg);
}

void test_config_loads_example_config_cleanly(void) {
    config_t cfg = {0};
    TEST_ASSERT_TRUE(config_load("config.example.json", &cfg));
    TEST_ASSERT_EQUAL_INT(UNIFI_APPLY_IPC, cfg.unifi_cfg.apply_method);
    TEST_ASSERT_EQUAL_UINT(4, cfg.preset_cfg.count);
    TEST_ASSERT_EQUAL_INT(100, cfg.sfx_preset_cfg.default_volume);
    config_free(&cfg);
}

int main(void) {
    UNITY_BEGIN();

    /* Existing baseline tests */
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

    /* Expanded tests */
    RUN_TEST(test_config_null_parameters_rejected);
    RUN_TEST(test_config_defaults_applied_for_minimal_config);
    RUN_TEST(test_config_env_overrides_json_values);
    RUN_TEST(test_config_invalid_int_env_falls_back_to_json);
    RUN_TEST(test_config_env_overrides_missing_unifi_section_to_ipc);
    RUN_TEST(test_config_fails_on_invalid_apply_method);
    RUN_TEST(test_config_fails_on_missing_or_invalid_mqtt_section);
    RUN_TEST(test_config_fails_on_missing_or_invalid_ssh_section);
    RUN_TEST(test_config_fails_when_ssh_user_exceeds_buffer);
    RUN_TEST(test_config_fails_when_mqtt_host_or_prefix_or_cafile_exceeds_buffer);
    RUN_TEST(test_config_skips_preset_with_invalid_directory_name);
    RUN_TEST(test_config_sfx_volume_rules_and_duplicate_skipping);
    RUN_TEST(test_config_loads_example_config_cleanly);

    return UNITY_END();
}
