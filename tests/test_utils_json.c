#include "cJSON.h"
#include "third_party/unity/unity.h"
#include "third_party/unity/unity_internals.h"
#include "utils_json.h"
#include <stdlib.h>


void setUp(void) {
}

void tearDown(void) {
}

void test_utils_json_strdup_normalized_returns_normalized_string(void) {
    cJSON *obj = cJSON_CreateObject();

    cJSON_AddStringToObject(obj, "name", "Christmas    ");

    char *s = json_strdup_normalized(obj, "name");

    TEST_ASSERT_EQUAL_STRING("christmas", s);

    free(s);
    cJSON_Delete(obj);
}

void test_utils_json_strdup_normalized_returns_null_for_empty_string(void) {
    cJSON *obj = cJSON_CreateObject();

    cJSON_AddStringToObject(obj, "name", "");

    char *s = json_strdup_normalized(obj, "name");

    TEST_ASSERT_NULL(s);

    free(s);
    cJSON_Delete(obj);
}

void test_utils_json_strdup_normalized_returns_null_for_whitespace_only(void) {
    cJSON *obj = cJSON_CreateObject();

    cJSON_AddStringToObject(obj, "name", "  ");

    char *s = json_strdup_normalized(obj, "name");

    TEST_ASSERT_NULL(s);

    free(s);
    cJSON_Delete(obj);
}

void test_utils_json_strdup_normalized_returns_null_for_missing_key(void) {
    cJSON *obj = cJSON_CreateObject();

    char *s = json_strdup_normalized(obj, "name");

    TEST_ASSERT_NULL(s);

    free(s);
    cJSON_Delete(obj);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_utils_json_strdup_normalized_returns_normalized_string);
    RUN_TEST(test_utils_json_strdup_normalized_returns_null_for_empty_string);
    RUN_TEST(test_utils_json_strdup_normalized_returns_null_for_whitespace_only);
    RUN_TEST(test_utils_json_strdup_normalized_returns_null_for_missing_key);

    UNITY_END();
}