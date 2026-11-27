#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "support/test_helpers.h"
#include "banner.h"
#include "logger.h"
#include "utils.h"
#include "version.h"

#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char g_tmp_dir[128];

void setUp(void) {
    unsetenv("LOG_LEVEL");
    log_set_level(LOG_LEVEL_INFO);
    TEST_ASSERT_TRUE(test_make_temp_dir(g_tmp_dir, sizeof(g_tmp_dir)));
}

void tearDown(void) {
    unsetenv("LOG_LEVEL");
    log_set_level(LOG_LEVEL_INFO);
    test_remove_dir_recursive(g_tmp_dir);
}

static int redirect_stderr_start(const char *capture_path) {
    fflush(stderr);
    int saved = dup(STDERR_FILENO);
    int fd = open(capture_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    TEST_ASSERT_TRUE(fd >= 0);
    TEST_ASSERT_TRUE(dup2(fd, STDERR_FILENO) >= 0);
    close(fd);
    return saved;
}

static void redirect_stderr_end(int saved_fd) {
    fflush(stderr);
    TEST_ASSERT_TRUE(dup2(saved_fd, STDERR_FILENO) >= 0);
    close(saved_fd);
}

void test_log_init_levels_and_env(void) {
    char cap[PATH_MAX];
    snprintf(cap, sizeof(cap), "%s/log1.txt", g_tmp_dir);

    int saved = redirect_stderr_start(cap);
    log_init("DEBUG");
    LOG_DEBUG("dbg_visible");
    log_init("INFO");
    LOG_DEBUG("dbg_hidden");
    LOG_INFO("info_visible");
    log_init("WARN");
    LOG_INFO("info_hidden");
    LOG_WARN("warn_visible");
    log_init("ERROR");
    LOG_WARN("warn_hidden");
    LOG_ERROR("err_visible");
    log_init("BOGUS");
    log_init(NULL);
    setenv("LOG_LEVEL", "warn", 1);
    log_init(NULL);
    LOG_INFO("env_info_hidden");
    LOG_WARN("env_warn_visible");
    redirect_stderr_end(saved);

    char *out = NULL;
    TEST_ASSERT_TRUE(utils_read_file(cap, &out, NULL));
    TEST_ASSERT_NOT_NULL(strstr(out, "dbg_visible"));
    TEST_ASSERT_NULL(strstr(out, "dbg_hidden"));
    TEST_ASSERT_NOT_NULL(strstr(out, "info_visible"));
    TEST_ASSERT_NULL(strstr(out, "info_hidden"));
    TEST_ASSERT_NOT_NULL(strstr(out, "warn_visible"));
    TEST_ASSERT_NULL(strstr(out, "warn_hidden"));
    TEST_ASSERT_NOT_NULL(strstr(out, "err_visible"));
    TEST_ASSERT_NOT_NULL(strstr(out, "Unknown LOG_LEVEL='BOGUS'"));
    TEST_ASSERT_NULL(strstr(out, "env_info_hidden"));
    TEST_ASSERT_NOT_NULL(strstr(out, "env_warn_visible"));
    free(out);
}

void test_log_fatal_and_unknown_level_formatting(void) {
    char cap[PATH_MAX];
    snprintf(cap, sizeof(cap), "%s/log2.txt", g_tmp_dir);

    int saved = redirect_stderr_start(cap);
    log_set_level(LOG_LEVEL_DEBUG);
    log_log_impl(LOG_LEVEL_FATAL, "test.c", 10, "fn", "fatal_msg");
    log_log_impl((log_level_t)99, "test.c", 11, "fn", "weird_msg");
    redirect_stderr_end(saved);

    char *out = NULL;
    TEST_ASSERT_TRUE(utils_read_file(cap, &out, NULL));
    TEST_ASSERT_NOT_NULL(strstr(out, "[FATAL] test.c:10 fn: fatal_msg"));
    TEST_ASSERT_NOT_NULL(strstr(out, "[WEIRD] weird_msg"));
    free(out);
}

void test_print_banner_outputs_version(void) {
    char cap[PATH_MAX];
    snprintf(cap, sizeof(cap), "%s/banner.txt", g_tmp_dir);

    int saved = redirect_stderr_start(cap);
    log_set_level(LOG_LEVEL_INFO);
    print_banner();
    redirect_stderr_end(saved);

    char *out = NULL;
    TEST_ASSERT_TRUE(utils_read_file(cap, &out, NULL));
    TEST_ASSERT_NOT_NULL(strstr(out, "UniFi Doorbell MQTT Service v" APP_VERSION));
    TEST_ASSERT_NOT_NULL(strstr(out, "License: MIT"));
    free(out);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_log_init_levels_and_env);
    RUN_TEST(test_log_fatal_and_unknown_level_formatting);
    RUN_TEST(test_print_banner_outputs_version);

    return UNITY_END();
}
