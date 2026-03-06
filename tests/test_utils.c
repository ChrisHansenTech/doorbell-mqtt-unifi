#include "third_party/unity/unity.h"
#include "third_party/unity/unity_internals.h"
#include "utils.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_is_valid_filename_image_file(void) {
    TEST_ASSERT_TRUE(utils_is_valid_filename("test.png", UTILS_FILE_CLASS_ANIMATION));
}

void test_is_valid_filename_sound_file(void) {
    TEST_ASSERT_TRUE(utils_is_valid_filename("test.wav", UTILS_FILE_CLASS_SOUND));
    TEST_ASSERT_TRUE(utils_is_valid_filename("test.ogg", UTILS_FILE_CLASS_SOUND));
}

void test_is_invalid_filename_image_file(void) {
    TEST_ASSERT_FALSE(utils_is_valid_filename("test.pdf", UTILS_FILE_CLASS_ANIMATION));
}

void test_is_invalid_filename_sound_file(void) {
    TEST_ASSERT_FALSE(utils_is_valid_filename("test.pdf", UTILS_FILE_CLASS_SOUND));
}

void test_is_valid_file_image(void) {
    TEST_ASSERT_TRUE(utils_is_valid_file("test-profile/test.png", UTILS_FILE_CLASS_ANIMATION));
}

void test_is_valid_file_sound(void) {
    TEST_ASSERT_TRUE(utils_is_valid_file("test-profile/test.ogg", UTILS_FILE_CLASS_SOUND));
}

void test_is_invalid_file_image(void) {
    TEST_ASSERT_FALSE(utils_is_valid_file("tests/fixtures/invalid_assets/gif_with_png_extension.png", UTILS_FILE_CLASS_ANIMATION));
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