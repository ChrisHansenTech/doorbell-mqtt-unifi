#define _POSIX_C_SOURCE 200809L

#include "errors.h"
#include "third_party/unity/unity.h"
#include "support/test_helpers.h"
#include "utils.h"

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_tmp_dir[128];

void setUp(void) {
    TEST_ASSERT_TRUE(test_make_temp_dir(g_tmp_dir, sizeof(g_tmp_dir)));
}

void tearDown(void) {
    test_remove_dir_recursive(g_tmp_dir);
}

/* ===== Existing baseline tests (retained) ===== */

void test_is_valid_filename_image_file(void) {
    error_return_t err = utils_is_valid_filename("test.png", UTILS_FILE_CLASS_ANIMATION);
    TEST_ASSERT_EQUAL(ERROR_NONE, err.error_code);
}

void test_is_valid_filename_sound_file(void) {
    error_return_t err = utils_is_valid_filename("test.wav", UTILS_FILE_CLASS_SOUND);
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

/* ===== Expanded tests ===== */

void test_file_and_directory_exists(void) {
    char fpath[PATH_MAX];
    snprintf(fpath, sizeof(fpath), "%s/sample.txt", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(fpath, "hello"));

    TEST_ASSERT_FALSE(utils_file_exists(NULL));
    TEST_ASSERT_FALSE(utils_file_exists(""));
    TEST_ASSERT_TRUE(utils_file_exists(fpath));
    TEST_ASSERT_FALSE(utils_file_exists(g_tmp_dir));

    TEST_ASSERT_FALSE(utils_directory_exists(NULL));
    TEST_ASSERT_FALSE(utils_directory_exists(""));
    TEST_ASSERT_TRUE(utils_directory_exists(g_tmp_dir));
    TEST_ASSERT_FALSE(utils_directory_exists(fpath));
}

void test_build_path_normal_trailing_slashes_and_truncation(void) {
    char out[64];

    TEST_ASSERT_FALSE(utils_build_path(NULL, sizeof(out), "/base", "child"));
    TEST_ASSERT_FALSE(utils_build_path(out, sizeof(out), NULL, "child"));
    TEST_ASSERT_FALSE(utils_build_path(out, sizeof(out), "/base", NULL));

    TEST_ASSERT_TRUE(utils_build_path(out, sizeof(out), "/base///", "child.txt"));
    TEST_ASSERT_EQUAL_STRING("/base/child.txt", out);

    TEST_ASSERT_TRUE(utils_build_path(out, sizeof(out), "/", "etc"));
    TEST_ASSERT_EQUAL_STRING("/etc", out);

    char small[8];
    TEST_ASSERT_FALSE(utils_build_path(small, sizeof(small), "/base", "toolongname"));
}

void test_read_and_write_file_roundtrip_and_errors(void) {
    char fpath[PATH_MAX];
    snprintf(fpath, sizeof(fpath), "%s/roundtrip.txt", g_tmp_dir);

    TEST_ASSERT_FALSE(utils_write_file(NULL, "abc"));
    TEST_ASSERT_FALSE(utils_write_file(fpath, NULL));
    TEST_ASSERT_FALSE(utils_write_file("/no/such/parent/file.txt", "abc"));

    TEST_ASSERT_TRUE(utils_write_file(fpath, "line1\nline2"));

    char *content = NULL;
    size_t len = 0;
    TEST_ASSERT_FALSE(utils_read_file(NULL, &content, &len));
    TEST_ASSERT_FALSE(utils_read_file(fpath, NULL, &len));
    TEST_ASSERT_FALSE(utils_read_file("/no/such/file.txt", &content, &len));

    TEST_ASSERT_TRUE(utils_read_file(fpath, &content, &len));
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_EQUAL_UINT(11, len);
    TEST_ASSERT_EQUAL_STRING("line1\nline2", content);
    free(content);
}

void test_create_and_delete_directory_recursive(void) {
    char sub[PATH_MAX];
    char nested[PATH_MAX];
    char file_in_nested[PATH_MAX];

    snprintf(sub, sizeof(sub), "%s/sub", g_tmp_dir);
    snprintf(nested, sizeof(nested), "%s/sub/inner", g_tmp_dir);
    snprintf(file_in_nested, sizeof(file_in_nested), "%s/sub/inner/a.txt", g_tmp_dir);

    TEST_ASSERT_FALSE(utils_create_directory(NULL));
    TEST_ASSERT_FALSE(utils_create_directory(""));
    TEST_ASSERT_FALSE(utils_create_directory("/no/such/parent/dir"));

    TEST_ASSERT_TRUE(utils_create_directory(sub));
    TEST_ASSERT_TRUE(utils_create_directory(sub)); /* EEXIST returns true */
    TEST_ASSERT_TRUE(utils_create_directory(nested));
    TEST_ASSERT_TRUE(test_write_text(file_in_nested, "data"));

    TEST_ASSERT_FALSE(utils_delete_directory(NULL));
    TEST_ASSERT_FALSE(utils_delete_directory(""));
    TEST_ASSERT_FALSE(utils_delete_directory("/no/such/dir/to/delete"));

    TEST_ASSERT_TRUE(utils_delete_directory(sub));
    TEST_ASSERT_FALSE(utils_directory_exists(sub));
}

void test_timestamps_and_to_human_readable(void) {
    time_t epoch = 1700000000; /* 2023-11-14T22:13:20Z */
    char dir_ts[32];
    char iso_ts[32];

    utils_build_timestamp_dir(&epoch, dir_ts, sizeof(dir_ts));
    utils_build_iso_timestamp(&epoch, iso_ts, sizeof(iso_ts));

    TEST_ASSERT_EQUAL_STRING("20231114_221320", dir_ts);
    TEST_ASSERT_EQUAL_STRING("2023-11-14T22:13:20Z", iso_ts);

    char out[64];
    to_human_readable("front_door-main", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Front Door Main", out);

    char tiny[6];
    to_human_readable("front_door", tiny, sizeof(tiny));
    TEST_ASSERT_EQUAL_STRING("Front", tiny);
}

void test_md5_file_hex_vectors_and_errors(void) {
    char hex[33];
    char fpath[PATH_MAX];

    TEST_ASSERT_FALSE(utils_md5_file_hex(NULL, hex));
    TEST_ASSERT_FALSE(utils_md5_file_hex("", hex));
    TEST_ASSERT_FALSE(utils_md5_file_hex("test-profile/test.png", NULL));
    TEST_ASSERT_FALSE(utils_md5_file_hex("/no/such/file", hex));

    /* RFC 1321 empty string */
    snprintf(fpath, sizeof(fpath), "%s/empty.bin", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_bytes(fpath, (const unsigned char *)"", 0));
    TEST_ASSERT_TRUE(utils_md5_file_hex(fpath, hex));
    TEST_ASSERT_EQUAL_STRING("d41d8cd98f00b204e9800998ecf8427e", hex);

    /* RFC 1321 "abc" */
    snprintf(fpath, sizeof(fpath), "%s/abc.bin", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(fpath, "abc"));
    TEST_ASSERT_TRUE(utils_md5_file_hex(fpath, hex));
    TEST_ASSERT_EQUAL_STRING("900150983cd24fb0d6963f7d28e17f72", hex);

    /* Multi-chunk > 4096 bytes */
    unsigned char buf[5000];
    memset(buf, 'A', sizeof(buf));
    snprintf(fpath, sizeof(fpath), "%s/big.bin", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_bytes(fpath, buf, sizeof(buf)));
    TEST_ASSERT_TRUE(utils_md5_file_hex(fpath, hex));
    TEST_ASSERT_EQUAL_UINT(32, strlen(hex));
}

void test_is_valid_directory_name_boundaries(void) {
    TEST_ASSERT_TRUE(utils_is_valid_directory_name("christmas"));
    TEST_ASSERT_TRUE(utils_is_valid_directory_name("new_years-2026"));

    TEST_ASSERT_FALSE(utils_is_valid_directory_name(NULL));
    TEST_ASSERT_FALSE(utils_is_valid_directory_name(""));
    TEST_ASSERT_FALSE(utils_is_valid_directory_name(".hidden"));
    TEST_ASSERT_FALSE(utils_is_valid_directory_name("../escape"));
    TEST_ASSERT_FALSE(utils_is_valid_directory_name("a/b"));
    TEST_ASSERT_FALSE(utils_is_valid_directory_name("a\\b"));
    TEST_ASSERT_FALSE(utils_is_valid_directory_name("bad\nname"));
    TEST_ASSERT_FALSE(utils_is_valid_directory_name("trailing "));
    TEST_ASSERT_FALSE(utils_is_valid_directory_name("trailing."));

    char too_long[260];
    memset(too_long, 'x', 256);
    too_long[256] = '\0';
    TEST_ASSERT_FALSE(utils_is_valid_directory_name(too_long));
}

void test_is_valid_filename_edge_cases(void) {
    TEST_ASSERT_EQUAL(ERROR_INVALID_PARAMETERS, utils_is_valid_filename(NULL, UTILS_FILE_CLASS_ANIMATION).error_code);
    TEST_ASSERT_EQUAL(ERROR_FILENAME_INVALID, utils_is_valid_filename("../test.png", UTILS_FILE_CLASS_ANIMATION).error_code);
    TEST_ASSERT_EQUAL(ERROR_FILENAME_INVALID, utils_is_valid_filename(".secret.ogg", UTILS_FILE_CLASS_SOUND).error_code);
    TEST_ASSERT_EQUAL(ERROR_FILE_EXTENSION_INVALID, utils_is_valid_filename("noext", UTILS_FILE_CLASS_SOUND).error_code);
    TEST_ASSERT_EQUAL(ERROR_FILE_EXTENSION_INVALID, utils_is_valid_filename("test.png", UTILS_FILE_CLASS_SOUND).error_code);
    TEST_ASSERT_EQUAL(ERROR_FILE_EXTENSION_INVALID, utils_is_valid_filename("test.ogg", UTILS_FILE_CLASS_ANIMATION).error_code);
    TEST_ASSERT_EQUAL(ERROR_FILE_EXTENSION_INVALID, utils_is_valid_filename("test.png", (utils_file_class_t)99).error_code);
    TEST_ASSERT_EQUAL(ERROR_NONE, utils_is_valid_filename("UPPER.PNG", UTILS_FILE_CLASS_ANIMATION).error_code);
    TEST_ASSERT_EQUAL(ERROR_NONE, utils_is_valid_filename("UPPER.WAV", UTILS_FILE_CLASS_SOUND).error_code);
}

void test_is_valid_file_magic_headers_size_and_permissions(void) {
    char fpath[PATH_MAX];

    TEST_ASSERT_EQUAL(ERROR_INVALID_PARAMETERS, utils_is_valid_file(NULL, UTILS_FILE_CLASS_ANIMATION).error_code);
    TEST_ASSERT_EQUAL(ERROR_INVALID_PARAMETERS, utils_is_valid_file("", UTILS_FILE_CLASS_ANIMATION).error_code);

    /* Non-existent */
    TEST_ASSERT_EQUAL(ERROR_FILE_TYPE_INVALID, utils_is_valid_file("/no/such/file.png", UTILS_FILE_CLASS_ANIMATION).error_code);

    /* Directory is not regular file */
    TEST_ASSERT_EQUAL(ERROR_FILE_TYPE_INVALID, utils_is_valid_file(g_tmp_dir, UTILS_FILE_CLASS_ANIMATION).error_code);

    /* Empty file */
    snprintf(fpath, sizeof(fpath), "%s/empty.png", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_bytes(fpath, (const unsigned char *)"", 0));
    TEST_ASSERT_EQUAL(ERROR_FILE_TYPE_INVALID, utils_is_valid_file(fpath, UTILS_FILE_CLASS_ANIMATION).error_code);

    /* Unsupported extension */
    snprintf(fpath, sizeof(fpath), "%s/sound.mp3", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(fpath, "ID3dummy"));
    TEST_ASSERT_EQUAL(ERROR_FILE_TYPE_INVALID, utils_is_valid_file(fpath, UTILS_FILE_CLASS_SOUND).error_code);

    /* Valid PNG, OGG, WAV */
    snprintf(fpath, sizeof(fpath), "%s/good.png", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_minimal_png(fpath));
    TEST_ASSERT_EQUAL(ERROR_NONE, utils_is_valid_file(fpath, UTILS_FILE_CLASS_ANIMATION).error_code);
    /* Wrong class for PNG */
    TEST_ASSERT_EQUAL(ERROR_FILE_TYPE_INVALID, utils_is_valid_file(fpath, UTILS_FILE_CLASS_SOUND).error_code);
    TEST_ASSERT_EQUAL(ERROR_FILE_TYPE_INVALID, utils_is_valid_file(fpath, (utils_file_class_t)99).error_code);

    snprintf(fpath, sizeof(fpath), "%s/good.ogg", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_minimal_ogg(fpath));
    TEST_ASSERT_EQUAL(ERROR_NONE, utils_is_valid_file(fpath, UTILS_FILE_CLASS_SOUND).error_code);
    TEST_ASSERT_EQUAL(ERROR_FILE_TYPE_INVALID, utils_is_valid_file(fpath, UTILS_FILE_CLASS_ANIMATION).error_code);

    snprintf(fpath, sizeof(fpath), "%s/good.wav", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_minimal_wav(fpath));
    TEST_ASSERT_EQUAL(ERROR_NONE, utils_is_valid_file(fpath, UTILS_FILE_CLASS_SOUND).error_code);

    /* Spoofed magic headers (small files < 1MB so magic check actually executes!) */
    snprintf(fpath, sizeof(fpath), "%s/fake.png", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(fpath, "GIF89a_not_a_png"));
    TEST_ASSERT_EQUAL(ERROR_FILE_TYPE_INVALID, utils_is_valid_file(fpath, UTILS_FILE_CLASS_ANIMATION).error_code);

    snprintf(fpath, sizeof(fpath), "%s/fake.ogg", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(fpath, "NOT_OGG_DATA"));
    TEST_ASSERT_EQUAL(ERROR_FILE_TYPE_INVALID, utils_is_valid_file(fpath, UTILS_FILE_CLASS_SOUND).error_code);

    snprintf(fpath, sizeof(fpath), "%s/fake.wav", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(fpath, "RIFF1234NOTW"));
    TEST_ASSERT_EQUAL(ERROR_FILE_TYPE_INVALID, utils_is_valid_file(fpath, UTILS_FILE_CLASS_SOUND).error_code);

    /* Permission error on unreadable file (skip chmod assertion if running as root) */
    if (geteuid() != 0) {
        snprintf(fpath, sizeof(fpath), "%s/unreadable.png", g_tmp_dir);
        TEST_ASSERT_TRUE(test_write_minimal_png(fpath));
        TEST_ASSERT_EQUAL_INT(0, chmod(fpath, 0000));
        TEST_ASSERT_EQUAL(ERROR_FILE_TYPE_INVALID, utils_is_valid_file(fpath, UTILS_FILE_CLASS_ANIMATION).error_code);
        (void)chmod(fpath, 0644);
    }
}

void test_copy_and_move_file(void) {
    char src[PATH_MAX];
    char dst[PATH_MAX];
    char moved[PATH_MAX];

    snprintf(src, sizeof(src), "%s/src.bin", g_tmp_dir);
    snprintf(dst, sizeof(dst), "%s/dst.bin", g_tmp_dir);
    snprintf(moved, sizeof(moved), "%s/moved.bin", g_tmp_dir);

    /* 70KB payload to test multiple read/write loops in copy */
    size_t sz = 70000;
    unsigned char *payload = malloc(sz);
    TEST_ASSERT_NOT_NULL(payload);
    for (size_t i = 0; i < sz; i++) {
        payload[i] = (unsigned char)(i & 0xFF);
    }
    TEST_ASSERT_TRUE(test_write_bytes(src, payload, sz));

    TEST_ASSERT_FALSE(utils_copy_file_contents("/no/such/src", dst));
    TEST_ASSERT_FALSE(utils_copy_file_contents(src, "/no/such/dir/dst"));
    TEST_ASSERT_TRUE(utils_copy_file_contents(src, dst));

    char *copied = NULL;
    size_t copied_len = 0;
    TEST_ASSERT_TRUE(utils_read_file(dst, &copied, &copied_len));
    TEST_ASSERT_EQUAL_UINT(sz, copied_len);
    TEST_ASSERT_EQUAL_MEMORY(payload, copied, sz);
    free(copied);
    free(payload);

    TEST_ASSERT_FALSE(utils_move_file_cross_fs("/no/such/file", moved));
    TEST_ASSERT_TRUE(utils_move_file_cross_fs(dst, moved));
    TEST_ASSERT_FALSE(utils_file_exists(dst));
    TEST_ASSERT_TRUE(utils_file_exists(moved));
}

int main(void) {
    UNITY_BEGIN();

    /* Existing baseline tests */
    RUN_TEST(test_is_valid_filename_image_file);
    RUN_TEST(test_is_valid_filename_sound_file);
    RUN_TEST(test_is_invalid_filename_image_file);
    RUN_TEST(test_is_invalid_filename_sound_file);
    RUN_TEST(test_is_valid_file_image);
    RUN_TEST(test_is_valid_file_sound);
    RUN_TEST(test_is_invalid_file_image);

    /* Expanded tests */
    RUN_TEST(test_file_and_directory_exists);
    RUN_TEST(test_build_path_normal_trailing_slashes_and_truncation);
    RUN_TEST(test_read_and_write_file_roundtrip_and_errors);
    RUN_TEST(test_create_and_delete_directory_recursive);
    RUN_TEST(test_timestamps_and_to_human_readable);
    RUN_TEST(test_md5_file_hex_vectors_and_errors);
    RUN_TEST(test_is_valid_directory_name_boundaries);
    RUN_TEST(test_is_valid_filename_edge_cases);
    RUN_TEST(test_is_valid_file_magic_headers_size_and_permissions);
    RUN_TEST(test_copy_and_move_file);

    return UNITY_END();
}
