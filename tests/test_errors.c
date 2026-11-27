#include "third_party/unity/unity.h"
#include "errors.h"

#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

void test_error_code_lookup_known_codes(void) {
    TEST_ASSERT_EQUAL_INT(1, error_code_is_known(ERROR_NONE));
    TEST_ASSERT_EQUAL_STRING("ERROR_NONE", error_code_name(ERROR_NONE));
    TEST_ASSERT_EQUAL_STRING("No error", error_code_default_message(ERROR_NONE));

    TEST_ASSERT_EQUAL_INT(1, error_code_is_known(ERROR_CONFIG_INVALID));
    TEST_ASSERT_EQUAL_STRING("ERROR_CONFIG_INVALID", error_code_name(ERROR_CONFIG_INVALID));
    TEST_ASSERT_EQUAL_STRING("Configuration is invalid", error_code_default_message(ERROR_CONFIG_INVALID));

    TEST_ASSERT_EQUAL_INT(1, error_code_is_known(ERROR_SSH_AUTH_FAILED));
    TEST_ASSERT_EQUAL_STRING("ERROR_SSH_AUTH_FAILED", error_code_name(ERROR_SSH_AUTH_FAILED));

    TEST_ASSERT_EQUAL_INT(1, error_code_is_known(ERROR_SSH_CONNECTION_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("ERROR_SSH_CONNECTION_TIMEOUT", error_code_name(ERROR_SSH_CONNECTION_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("SSH connection timed out", error_code_default_message(ERROR_SSH_CONNECTION_TIMEOUT));

    TEST_ASSERT_EQUAL_INT(1, error_code_is_known(ERROR_SFX_PRESET_NOT_FOUND));
    TEST_ASSERT_EQUAL_STRING("ERROR_SFX_PRESET_NOT_FOUND", error_code_name(ERROR_SFX_PRESET_NOT_FOUND));

    TEST_ASSERT_EQUAL_INT(1, error_code_is_known(ERROR_STATE_COMPARE_FAILED));
    TEST_ASSERT_EQUAL_STRING("ERROR_STATE_COMPARE_FAILED", error_code_name(ERROR_STATE_COMPARE_FAILED));
}

void test_error_code_lookup_unknown_codes(void) {
    TEST_ASSERT_EQUAL_INT(0, error_code_is_known(-1));
    TEST_ASSERT_EQUAL_INT(0, error_code_is_known(99999));
    TEST_ASSERT_EQUAL_STRING("ERR_UNKNOWN", error_code_name((error_code_t)99999));
    TEST_ASSERT_EQUAL_STRING("Unknown error", error_code_default_message((error_code_t)99999));
}

void test_error_create_and_none(void) {
    error_return_t none = error_none();
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, none.error_code);
    TEST_ASSERT_EQUAL_STRING("", none.message);

    error_return_t e1 = error_create(ERROR_FILE_NOT_FOUND, "missing asset");
    TEST_ASSERT_EQUAL_INT(ERROR_FILE_NOT_FOUND, e1.error_code);
    TEST_ASSERT_EQUAL_STRING("missing asset", e1.message);

    error_return_t e2 = error_create(ERROR_FILE_NOT_FOUND, NULL);
    TEST_ASSERT_EQUAL_INT(ERROR_FILE_NOT_FOUND, e2.error_code);
    TEST_ASSERT_EQUAL_STRING("", e2.message);
}

void test_error_createf_and_truncation(void) {
    error_return_t e1 = error_createf(ERROR_SSH_COMMAND_FAILED, "exit=%d cmd=%s", 42, "ls");
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_COMMAND_FAILED, e1.error_code);
    TEST_ASSERT_EQUAL_STRING("exit=42 cmd=ls", e1.message);

    error_return_t e2 = error_createf(ERROR_SSH_COMMAND_FAILED, NULL);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_COMMAND_FAILED, e2.error_code);
    TEST_ASSERT_EQUAL_STRING("", e2.message);

    char big[200];
    memset(big, 'Z', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    error_return_t e3 = error_createf(ERROR_VALIDATION_FAILED, "%s", big);
    TEST_ASSERT_EQUAL_UINT(sizeof(e3.message) - 1, strlen(e3.message));
}

void test_error_wrap(void) {
    error_return_t inner = error_create(ERROR_FILE_TYPE_INVALID, "bad header");
    error_return_t wrapped = error_wrap(inner, "Asset '%s' failed", "foo.png");

    TEST_ASSERT_EQUAL_INT(ERROR_FILE_TYPE_INVALID, wrapped.error_code);
    TEST_ASSERT_EQUAL_STRING("Asset 'foo.png' failed: bad header", wrapped.message);

    error_return_t passthrough = error_wrap(inner, NULL);
    TEST_ASSERT_EQUAL_INT(ERROR_FILE_TYPE_INVALID, passthrough.error_code);
    TEST_ASSERT_EQUAL_STRING("bad header", passthrough.message);

    /* Outer message fills buffer completely */
    char long_outer[150];
    memset(long_outer, 'X', sizeof(long_outer) - 1);
    long_outer[sizeof(long_outer) - 1] = '\0';
    error_return_t trunc = error_wrap(inner, "%s", long_outer);
    TEST_ASSERT_EQUAL_INT(ERROR_FILE_TYPE_INVALID, trunc.error_code);
    TEST_ASSERT_EQUAL_UINT(sizeof(trunc.message) - 1, strlen(trunc.message));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_error_code_lookup_known_codes);
    RUN_TEST(test_error_code_lookup_unknown_codes);
    RUN_TEST(test_error_create_and_none);
    RUN_TEST(test_error_createf_and_truncation);
    RUN_TEST(test_error_wrap);

    return UNITY_END();
}
