#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "support/test_helpers.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static char g_tmp_dir[128];

static const char *get_app_binary(void) {
    const char *env_bin = getenv("DOORBELL_TEST_BIN");
    return (env_bin && env_bin[0] != '\0') ? env_bin : "./bin/doorbell-mqtt-unifi";
}

static int run_app_to_exit(
    const char *config_path,
    const char *profiles_dir,
    const char *sounds_dir,
    const char *state_dir,
    char *output_buf,
    size_t output_size
) {
    int pipefd[2];
    TEST_ASSERT_EQUAL_INT(0, pipe(pipefd));

    pid_t pid = fork();
    TEST_ASSERT_TRUE(pid >= 0);

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        setenv("DOORBELL_CONFIG_PATH", config_path, 1);
        setenv("DOORBELL_PROFILES_DIR", profiles_dir, 1);
        setenv("DOORBELL_SOUNDS_DIR", sounds_dir, 1);
        setenv("DOORBELL_STATE_DIR", state_dir, 1);
        setenv("LOG_LEVEL", "INFO", 1);

        const char *bin = get_app_binary();
        execl(bin, bin, (char *)NULL);
        _exit(127);
    }

    close(pipefd[1]);
    size_t total = 0;
    if (output_buf && output_size > 0) {
        output_buf[0] = '\0';
        while (total + 1 < output_size) {
            ssize_t n = read(pipefd[0], output_buf + total, output_size - 1 - total);
            if (n <= 0) break;
            total += (size_t)n;
        }
        output_buf[total] = '\0';
    }
    close(pipefd[0]);

    int status = 0;
    TEST_ASSERT_EQUAL_INT(pid, waitpid(pid, &status, 0));
    TEST_ASSERT_TRUE(WIFEXITED(status));
    return WEXITSTATUS(status);
}

void setUp(void) {
    TEST_ASSERT_TRUE(test_make_temp_dir(g_tmp_dir, sizeof(g_tmp_dir)));
}

void tearDown(void) {
    test_remove_dir_recursive(g_tmp_dir);
}

void test_smoke_missing_config_exits_nonzero(void) {
    char cfg[PATH_MAX];
    char output[4096];
    snprintf(cfg, sizeof(cfg), "%s/nonexistent.json", g_tmp_dir);

    int rc = run_app_to_exit(cfg, g_tmp_dir, g_tmp_dir, g_tmp_dir, output, sizeof(output));
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_NOT_NULL(strstr(output, "Configuration load failed"));
}

void test_smoke_invalid_config_exits_nonzero(void) {
    char cfg[PATH_MAX];
    char output[4096];
    snprintf(cfg, sizeof(cfg), "%s/bad.json", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(cfg, "{invalid json"));

    int rc = run_app_to_exit(cfg, g_tmp_dir, g_tmp_dir, g_tmp_dir, output, sizeof(output));
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_NOT_NULL(strstr(output, "Configuration load failed"));
}

void test_smoke_missing_profiles_dir_exits_nonzero(void) {
    char cfg[PATH_MAX];
    char missing_profiles[PATH_MAX];
    char output[4096];
    snprintf(cfg, sizeof(cfg), "%s/valid.json", g_tmp_dir);
    snprintf(missing_profiles, sizeof(missing_profiles), "%s/no_such_profiles", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(
        cfg,
        "{\"mqtt\":{\"host\":\"127.0.0.1\",\"port\":1},\"ssh\":{\"host\":\"127.0.0.1\"}}"
    ));

    int rc = run_app_to_exit(cfg, missing_profiles, g_tmp_dir, g_tmp_dir, output, sizeof(output));
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_NOT_NULL(strstr(output, "Profiles initialization failed"));
}

void test_smoke_valid_startup_and_sigterm_graceful_shutdown(void) {
    char cfg[PATH_MAX];
    char profiles[PATH_MAX];
    char sounds[PATH_MAX];
    char state[PATH_MAX];

    snprintf(cfg, sizeof(cfg), "%s/valid.json", g_tmp_dir);
    snprintf(profiles, sizeof(profiles), "%s/profiles", g_tmp_dir);
    snprintf(sounds, sizeof(sounds), "%s/sounds", g_tmp_dir);
    snprintf(state, sizeof(state), "%s/state", g_tmp_dir);

    TEST_ASSERT_TRUE(utils_create_directory(profiles));
    TEST_ASSERT_TRUE(utils_create_directory(sounds));
    TEST_ASSERT_TRUE(utils_create_directory(state));
    TEST_ASSERT_TRUE(test_write_text(
        cfg,
        "{\"mqtt\":{\"host\":\"127.0.0.1\",\"port\":1},\"ssh\":{\"host\":\"127.0.0.1\"},\"unifi\":{\"apply_method\":\"ipc\"}}"
    ));

    int pipefd[2];
    TEST_ASSERT_EQUAL_INT(0, pipe(pipefd));

    pid_t pid = fork();
    TEST_ASSERT_TRUE(pid >= 0);

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        setenv("DOORBELL_CONFIG_PATH", cfg, 1);
        setenv("DOORBELL_PROFILES_DIR", profiles, 1);
        setenv("DOORBELL_SOUNDS_DIR", sounds, 1);
        setenv("DOORBELL_STATE_DIR", state, 1);
        setenv("LOG_LEVEL", "INFO", 1);

        const char *bin = get_app_binary();
        execl(bin, bin, (char *)NULL);
        _exit(127);
    }

    close(pipefd[1]);

    char output[8192];
    size_t total = 0;
    output[0] = '\0';
    bool sigterm_sent = false;

    while (total + 1 < sizeof(output)) {
        ssize_t n = read(pipefd[0], output + total, sizeof(output) - 1 - total);
        if (n <= 0) break;
        total += (size_t)n;
        output[total] = '\0';

        if (!sigterm_sent && strstr(output, "Profile repository initialized")) {
            kill(pid, SIGTERM);
            sigterm_sent = true;
        }
    }
    close(pipefd[0]);

    int status = 0;
    TEST_ASSERT_EQUAL_INT(pid, waitpid(pid, &status, 0));
    TEST_ASSERT_TRUE(sigterm_sent);
    TEST_ASSERT_TRUE(WIFEXITED(status));
    TEST_ASSERT_EQUAL_INT(0, WEXITSTATUS(status));
    TEST_ASSERT_NOT_NULL(strstr(output, "UniFi Doorbell MQTT Service"));
    TEST_ASSERT_NOT_NULL(strstr(output, "Service stopped cleanly."));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_smoke_missing_config_exits_nonzero);
    RUN_TEST(test_smoke_invalid_config_exits_nonzero);
    RUN_TEST(test_smoke_missing_profiles_dir_exits_nonzero);
    RUN_TEST(test_smoke_valid_startup_and_sigterm_graceful_shutdown);

    return UNITY_END();
}
