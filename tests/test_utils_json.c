#include "cJSON.h"
#include "third_party/unity/unity.h"
#include "utils_json.h"

#include <stdlib.h>

void setUp(void) {
}

void tearDown(void) {
}

/* ===== Existing baseline tests (retained) ===== */

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

/* ===== Expanded tests ===== */

void test_utils_json_strdup_normalized_trims_leading_and_rejects_null_or_non_string(void) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "padded", " \t New Years \n");
    cJSON_AddNumberToObject(obj, "num", 42);

    char *s = json_strdup_normalized(obj, "padded");
    TEST_ASSERT_EQUAL_STRING("new years", s);
    free(s);

    TEST_ASSERT_NULL(json_strdup_normalized(obj, "num"));
    TEST_ASSERT_NULL(json_strdup_normalized(NULL, "padded"));
    TEST_ASSERT_NULL(json_strdup_normalized(obj, NULL));

    cJSON_Delete(obj);
}

void test_utils_json_get_string(void) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "key", "value");
    cJSON_AddNumberToObject(obj, "num", 7);

    TEST_ASSERT_EQUAL_STRING("value", json_get_string(obj, "key"));
    TEST_ASSERT_NULL(json_get_string(obj, "Key")); /* case-sensitive */
    TEST_ASSERT_NULL(json_get_string(obj, "num"));
    TEST_ASSERT_NULL(json_get_string(obj, "missing"));
    TEST_ASSERT_NULL(json_get_string(NULL, "key"));
    TEST_ASSERT_NULL(json_get_string(obj, NULL));

    cJSON_Delete(obj);
}

void test_utils_json_get_bool_int_double(void) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(obj, "flag_t", 1);
    cJSON_AddBoolToObject(obj, "flag_f", 0);
    cJSON_AddNumberToObject(obj, "count", 123);
    cJSON_AddNumberToObject(obj, "pi", 3.14159);
    cJSON_AddStringToObject(obj, "str", "123");

    bool b = false;
    TEST_ASSERT_TRUE(json_get_bool(obj, "flag_t", &b));
    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_TRUE(json_get_bool(obj, "flag_f", &b));
    TEST_ASSERT_FALSE(b);
    TEST_ASSERT_FALSE(json_get_bool(obj, "count", &b));
    TEST_ASSERT_FALSE(json_get_bool(NULL, "flag_t", &b));
    TEST_ASSERT_FALSE(json_get_bool(obj, NULL, &b));
    TEST_ASSERT_FALSE(json_get_bool(obj, "flag_t", NULL));

    int iv = 0;
    TEST_ASSERT_TRUE(json_get_int(obj, "count", &iv));
    TEST_ASSERT_EQUAL_INT(123, iv);
    TEST_ASSERT_FALSE(json_get_int(obj, "str", &iv));
    TEST_ASSERT_FALSE(json_get_int(NULL, "count", &iv));
    TEST_ASSERT_FALSE(json_get_int(obj, NULL, &iv));
    TEST_ASSERT_FALSE(json_get_int(obj, "count", NULL));

    double dv = 0.0;
    TEST_ASSERT_TRUE(json_get_double(obj, "pi", &dv));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 3.14159, dv);
    TEST_ASSERT_FALSE(json_get_double(obj, "str", &dv));
    TEST_ASSERT_FALSE(json_get_double(NULL, "pi", &dv));
    TEST_ASSERT_FALSE(json_get_double(obj, NULL, &dv));
    TEST_ASSERT_FALSE(json_get_double(obj, "pi", NULL));

    cJSON_Delete(obj);
}

void test_utils_json_upserts(void) {
    cJSON *obj = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();

    /* Invalid inputs clean up passed item without leaking */
    TEST_ASSERT_FALSE(json_upsert_item_case_sensitive(NULL, "a", cJSON_CreateNumber(1)));
    TEST_ASSERT_FALSE(json_upsert_item_case_sensitive(arr, "a", cJSON_CreateNumber(1)));
    TEST_ASSERT_FALSE(json_upsert_item_case_sensitive(obj, NULL, cJSON_CreateNumber(1)));
    TEST_ASSERT_FALSE(json_upsert_item_case_sensitive(obj, "a", NULL));

    /* Insert */
    TEST_ASSERT_TRUE(json_upsert_number_cs(obj, "num", 10.5));
    TEST_ASSERT_TRUE(json_upsert_bool_cs(obj, "enabled", true));
    TEST_ASSERT_TRUE(json_upsert_string_cs(obj, "label", "first"));
    TEST_ASSERT_TRUE(json_upsert_string_cs(obj, "empty_null", NULL));

    double dv = 0.0;
    bool bv = false;
    TEST_ASSERT_TRUE(json_get_double(obj, "num", &dv));
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 10.5, dv);
    TEST_ASSERT_TRUE(json_get_bool(obj, "enabled", &bv));
    TEST_ASSERT_TRUE(bv);
    TEST_ASSERT_EQUAL_STRING("first", json_get_string(obj, "label"));
    TEST_ASSERT_EQUAL_STRING("", json_get_string(obj, "empty_null"));

    /* Replace existing & verify case-sensitivity */
    TEST_ASSERT_TRUE(json_upsert_number_cs(obj, "num", 99.0));
    TEST_ASSERT_TRUE(json_upsert_bool_cs(obj, "enabled", false));
    TEST_ASSERT_TRUE(json_upsert_string_cs(obj, "label", "second"));
    TEST_ASSERT_TRUE(json_upsert_string_cs(obj, "Label", "different_case"));

    TEST_ASSERT_TRUE(json_get_double(obj, "num", &dv));
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 99.0, dv);
    TEST_ASSERT_TRUE(json_get_bool(obj, "enabled", &bv));
    TEST_ASSERT_FALSE(bv);
    TEST_ASSERT_EQUAL_STRING("second", json_get_string(obj, "label"));
    TEST_ASSERT_EQUAL_STRING("different_case", json_get_string(obj, "Label"));

    cJSON_Delete(arr);
    cJSON_Delete(obj);
}

int main(void) {
    UNITY_BEGIN();

    /* Existing baseline tests */
    RUN_TEST(test_utils_json_strdup_normalized_returns_normalized_string);
    RUN_TEST(test_utils_json_strdup_normalized_returns_null_for_empty_string);
    RUN_TEST(test_utils_json_strdup_normalized_returns_null_for_whitespace_only);
    RUN_TEST(test_utils_json_strdup_normalized_returns_null_for_missing_key);

    /* Expanded tests */
    RUN_TEST(test_utils_json_strdup_normalized_trims_leading_and_rejects_null_or_non_string);
    RUN_TEST(test_utils_json_get_string);
    RUN_TEST(test_utils_json_get_bool_int_double);
    RUN_TEST(test_utils_json_upserts);

    return UNITY_END();
}
