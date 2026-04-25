#include "errors.h"
#include "third_party/unity/unity.h"
#include "third_party/unity/unity_internals.h"
#include "utils.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_is_valid_filename_image_file(void) {
    error_return_t err = utils_is_valid_filename("test.png", UTILS_FILE_CLASS_ANIMATION);
    
    TEST_ASSERT_EQUAL(ERROR_NONE, err.error_code);
}

void test_is_valid_filename_sound_file(void) {
    error_return_t err;

    err = utils_is_valid_filename("test.wav", UTILS_FILE_CLASS_SOUND);

    TEST_ASSERT_EQUAL(ERROR_NONE, err.error_code);

    err = utils_is_valid_filename("test.ogg", UTILS_FILE_CLASS_SOUND);

    TEST_ASSERT_EQUAL(ERROR_NONE, err.error_code);
}

void test_is_invalid_filename_image_file(void) {
    error_return_t err = utils_is_valid_filename("test.pdf", UTILS_FILE_CLASS_ANIMATION);

    TEST_ASSERT_EQUAL(ERROR_FILE_EXTENSION_INVALID, err.error_code);
}

void test_is_invalid_filename_sound_file(void) {
    error_return_t err = utils_is_valid_filename("test.pdf", UTILS_FILE_CLASS_SOUND);

    TEST_ASSERT_EQUAL(ERROR_FILE_EXTENSION_INVALID, err.error_code);
}

void test_is_valid_file_image(void) {
    error_return_t err = utils_is_valid_file("test-profile/test.png", UTILS_FILE_CLASS_ANIMATION);

    TEST_ASSERT_EQUAL(ERROR_NONE, err.error_code);
}

void test_is_valid_file_sound(void) {
    error_return_t err = utils_is_valid_file("test-profile/test.ogg", UTILS_FILE_CLASS_SOUND);

    TEST_ASSERT_EQUAL(ERROR_NONE, err.error_code);
}

void test_is_invalid_file_image(void) {
    error_return_t err = utils_is_valid_file("tests/fixtures/invalid_assets/gif_with_png_extension.png", UTILS_FILE_CLASS_ANIMATION);

    TEST_ASSERT_EQUAL(ERROR_FILE_TYPE_INVALID, err.error_code);
}



int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_is_valid_filename_image_file);
    RUN_TEST(test_is_valid_filename_sound_file);
    RUN_TEST(test_is_invalid_filename_image_file);
    RUN_TEST(test_is_invalid_filename_sound_file);
    RUN_TEST(test_is_valid_file_image);
    RUN_TEST(test_is_valid_file_sound);
    RUN_TEST(test_is_invalid_file_image);

    UNITY_END();
}