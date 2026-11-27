#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "mocks/mock_libssh2.h"
#include "support/test_helpers.h"
#include "config_types.h"
#include "ssh.h"
#include "utils.h"

#include <linux/limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_tmp_dir[128];

static config_ssh_t make_ssh_cfg(void) {
    config_ssh_t cfg = {0};
    snprintf(cfg.host, sizeof(cfg.host), "192.168.1.50");
    cfg.port = 22;
    snprintf(cfg.user, sizeof(cfg.user), "ubnt");
    snprintf(cfg.password_env, sizeof(cfg.password_env), "TEST_SSH_PASS");
    return cfg;
}

void setUp(void) {
    mock_libssh2_reset();
    setenv("TEST_SSH_PASS", "recovery-secret", 1);
    TEST_ASSERT_TRUE(test_make_temp_dir(g_tmp_dir, sizeof(g_tmp_dir)));
}

void tearDown(void) {
    unsetenv("TEST_SSH_PASS");
    test_remove_dir_recursive(g_tmp_dir);
}

void test_ssh_global_init_and_cleanup(void) {
    g_mock_libssh2.init_rc = 0;
    TEST_ASSERT_TRUE(ssh_global_init());
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.init_calls);

    g_mock_libssh2.init_rc = -1;
    TEST_ASSERT_FALSE(ssh_global_init());

    ssh_global_cleanup();
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.exit_calls);
}

void test_ssh_session_create_validation_and_connection_failures(void) {
    config_ssh_t cfg = make_ssh_cfg();

    /* Invalid configs */
    TEST_ASSERT_NULL(ssh_session_create(NULL));
    cfg.host[0] = '\0';
    TEST_ASSERT_NULL(ssh_session_create(&cfg));
    cfg = make_ssh_cfg();
    cfg.user[0] = '\0';
    TEST_ASSERT_NULL(ssh_session_create(&cfg));

    /* DNS lookup failure */
    cfg = make_ssh_cfg();
    g_mock_libssh2.getaddrinfo_rc = EAI_NONAME;
    TEST_ASSERT_NULL(ssh_session_create(&cfg));
    TEST_ASSERT_EQUAL_UINT(0, g_mock_libssh2.socket_calls);

    /* Socket/connect failures across multiple addrinfo records */
    mock_libssh2_reset();
    g_mock_libssh2.addrinfo_count = 2;
    g_mock_libssh2.socket_fail_on_call = 1;
    g_mock_libssh2.connect_fail_count = 1;
    TEST_ASSERT_NULL(ssh_session_create(&cfg));
    TEST_ASSERT_EQUAL_UINT(2, g_mock_libssh2.socket_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.connect_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.freeaddrinfo_calls);

    /* libssh2_session_init fails -> socket closed */
    mock_libssh2_reset();
    g_mock_libssh2.session_init_returns_null = true;
    TEST_ASSERT_NULL(ssh_session_create(&cfg));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);

    /* Handshake fails -> session freed & socket closed */
    mock_libssh2_reset();
    g_mock_libssh2.handshake_rc = -1;
    TEST_ASSERT_NULL(ssh_session_create(&cfg));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.session_free_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);
}

void test_ssh_session_auth_failures_and_recovery(void) {
    config_ssh_t cfg = make_ssh_cfg();

    /* Missing password env var -> disconnect, free, close */
    unsetenv("TEST_SSH_PASS");
    TEST_ASSERT_NULL(ssh_session_create(&cfg));
    TEST_ASSERT_EQUAL_UINT(0, g_mock_libssh2.userauth_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.session_disconnect_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.session_free_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);

    /* Wrong password -> userauth fails -> disconnect, free, close */
    mock_libssh2_reset();
    setenv("TEST_SSH_PASS", "wrong", 1);
    g_mock_libssh2.userauth_rc = LIBSSH2_ERROR_AUTHENTICATION_FAILED;
    TEST_ASSERT_NULL(ssh_session_create(&cfg));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.userauth_calls);
    TEST_ASSERT_EQUAL_STRING("ubnt", g_mock_libssh2.last_user);
    TEST_ASSERT_EQUAL_STRING("wrong", g_mock_libssh2.last_password);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.session_disconnect_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.session_free_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);

    /* Recovery: subsequent valid auth succeeds and cleans up cleanly */
    mock_libssh2_reset();
    setenv("TEST_SSH_PASS", "recovery-secret", 1);
    ssh_session_t *s = ssh_session_create(&cfg);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.session_blocking_calls);
    ssh_session_destroy(s);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.session_disconnect_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.session_free_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);

    ssh_session_destroy(NULL);
}

void test_ssh_exec_command_stdout_stderr_eagain_and_failures(void) {
    config_ssh_t cfg = make_ssh_cfg();
    ssh_session_t *s = ssh_session_create(&cfg);
    TEST_ASSERT_NOT_NULL(s);

    /* Invalid args */
    TEST_ASSERT_FALSE(ssh_exec_command(NULL, "ls", NULL, NULL, NULL, NULL, NULL));
    TEST_ASSERT_FALSE(ssh_exec_command(s, NULL, NULL, NULL, NULL, NULL, NULL));

    /* Channel open failure */
    g_mock_libssh2.channel_open_returns_null = true;
    TEST_ASSERT_FALSE(ssh_exec_command(s, "ls", NULL, NULL, NULL, NULL, NULL));
    g_mock_libssh2.channel_open_returns_null = false;

    /* Channel exec failure frees channel */
    g_mock_libssh2.channel_exec_rc = -1;
    TEST_ASSERT_FALSE(ssh_exec_command(s, "ls", NULL, NULL, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.channel_free_calls);
    g_mock_libssh2.channel_exec_rc = 0;

    /* Multi-chunk stdout + stderr + EAGAIN + backoff nanosleep + exit code */
    mock_libssh2_push_stdout("hello ", 6);
    mock_libssh2_push_stdout_rc(LIBSSH2_ERROR_EAGAIN);
    mock_libssh2_push_stdout("world", 5);
    mock_libssh2_push_stderr("warn1\n", 6);
    mock_libssh2_push_stderr_rc(LIBSSH2_ERROR_EAGAIN);
    g_mock_libssh2.eof_delay_checks = 3; /* triggers nanosleep backoff once */
    g_mock_libssh2.channel_exit_status = 7;

    int exit_rc = 0;
    char *out_buf = NULL;
    size_t out_len = 0;
    char *err_buf = NULL;
    size_t err_len = 0;

    TEST_ASSERT_TRUE(ssh_exec_command(s, "echo hello", &exit_rc, &out_buf, &out_len, &err_buf, &err_len));
    TEST_ASSERT_EQUAL_STRING("echo hello", g_mock_libssh2.last_command);
    TEST_ASSERT_EQUAL_INT(7, exit_rc);
    TEST_ASSERT_EQUAL_UINT(11, out_len);
    TEST_ASSERT_EQUAL_STRING("hello world", out_buf);
    TEST_ASSERT_EQUAL_UINT(6, err_len);
    TEST_ASSERT_EQUAL_STRING("warn1\n", err_buf);
    TEST_ASSERT_TRUE(g_mock_libssh2.nanosleep_calls >= 1);
    free(out_buf);
    free(err_buf);

    /* Calling with NULL output pointers and non-zero exit logs error and frees buffers */
    g_mock_libssh2.stdout_chunk_count = 0;
    g_mock_libssh2.stdout_chunk_idx = 0;
    g_mock_libssh2.stderr_chunk_count = 0;
    g_mock_libssh2.stderr_chunk_idx = 0;
    g_mock_libssh2.eof_delay_checks = 0;
    g_mock_libssh2.eof_checks = 0;
    mock_libssh2_push_stdout("discarded", 9);
    mock_libssh2_push_stderr("err", 3);
    g_mock_libssh2.channel_exit_status = 2;
    TEST_ASSERT_TRUE(ssh_exec_command(s, "false", NULL, NULL, NULL, NULL, NULL));

    ssh_session_destroy(s);
}

void test_ssh_scp_upload_success_partial_writes_and_errors(void) {
    config_ssh_t cfg = make_ssh_cfg();
    ssh_session_t *s = ssh_session_create(&cfg);
    TEST_ASSERT_NOT_NULL(s);

    char local_file[PATH_MAX];
    snprintf(local_file, sizeof(local_file), "%s/upload.ogg", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(local_file, "0123456789ABCDEF"));

    /* Invalid args & missing local file */
    TEST_ASSERT_FALSE(ssh_scp_upload_file(NULL, local_file, "/tmp", 0644));
    TEST_ASSERT_FALSE(ssh_scp_upload_file(s, NULL, "/tmp", 0644));
    TEST_ASSERT_FALSE(ssh_scp_upload_file(s, local_file, NULL, 0644));
    TEST_ASSERT_FALSE(ssh_scp_upload_file(s, "/no/such/local/file", "/tmp", 0644));

    /* libssh2_scp_send64 returns NULL */
    g_mock_libssh2.scp_send_returns_null = true;
    TEST_ASSERT_FALSE(ssh_scp_upload_file(s, local_file, "/tmp/dst", 0644));
    g_mock_libssh2.scp_send_returns_null = false;

    /* Write fails mid-transfer -> channel freed */
    g_mock_libssh2.scp_write_max_chunk = 4;
    g_mock_libssh2.scp_write_fail_on_call = 2;
    TEST_ASSERT_FALSE(ssh_scp_upload_file(s, local_file, "/tmp/dst", 0644));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.channel_free_calls);

    /* Recovery: successful upload with partial writes (5 bytes at a time) */
    g_mock_libssh2.scp_write_fail_on_call = 0;
    g_mock_libssh2.scp_write_calls = 0;
    g_mock_libssh2.scp_write_max_chunk = 5;
    g_mock_libssh2.uploaded_len = 0;

    TEST_ASSERT_TRUE(ssh_scp_upload_file(s, local_file, "/tmp/dst", 0644));
    TEST_ASSERT_EQUAL_STRING("/tmp/dst/upload.ogg", g_mock_libssh2.last_scp_path);
    TEST_ASSERT_EQUAL_INT(0644, g_mock_libssh2.last_scp_mode);
    TEST_ASSERT_EQUAL_INT64(16, g_mock_libssh2.last_scp_size);
    TEST_ASSERT_EQUAL_UINT(16, g_mock_libssh2.uploaded_len);
    TEST_ASSERT_EQUAL_MEMORY("0123456789ABCDEF", g_mock_libssh2.uploaded_data, 16);
    TEST_ASSERT_TRUE(g_mock_libssh2.scp_write_calls >= 4);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.channel_send_eof_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.channel_wait_eof_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.channel_wait_closed_calls);

    ssh_session_destroy(s);
}

void test_ssh_scp_download_success_trailing_nul_and_errors(void) {
    config_ssh_t cfg = make_ssh_cfg();
    ssh_session_t *s = ssh_session_create(&cfg);
    TEST_ASSERT_NOT_NULL(s);

    char out_file[PATH_MAX];
    snprintf(out_file, sizeof(out_file), "%s/downloaded.conf", g_tmp_dir);

    /* Invalid args */
    TEST_ASSERT_FALSE(ssh_scp_download_file(NULL, "/etc/a", out_file));
    TEST_ASSERT_FALSE(ssh_scp_download_file(s, NULL, out_file));
    TEST_ASSERT_FALSE(ssh_scp_download_file(s, "/etc/a", NULL));

    /* scp_recv2 returns NULL */
    g_mock_libssh2.scp_recv_returns_null = true;
    TEST_ASSERT_FALSE(ssh_scp_download_file(s, "/etc/a", out_file));
    g_mock_libssh2.scp_recv_returns_null = false;

    /* Unwritable local path frees channel */
    TEST_ASSERT_FALSE(ssh_scp_download_file(s, "/etc/a", "/no/such/dir/file"));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.channel_free_calls);

    /* Read error (< 0) */
    g_mock_libssh2.scp_recv_size = 10;
    g_mock_libssh2.recv_chunk_count = 0;
    g_mock_libssh2.recv_chunk_idx = 0;
    mock_libssh2_push_recv_rc(-5);
    TEST_ASSERT_FALSE(ssh_scp_download_file(s, "/etc/a", out_file));

    /* Unexpected EOF (0 before remaining reaches 0) */
    g_mock_libssh2.scp_recv_size = 10;
    g_mock_libssh2.recv_chunk_count = 0;
    g_mock_libssh2.recv_chunk_idx = 0;
    mock_libssh2_push_recv("abc", 3);
    mock_libssh2_push_recv("", 0);
    TEST_ASSERT_FALSE(ssh_scp_download_file(s, "/etc/a", out_file));

    /* Success with EAGAIN retry + trailing NUL stripping */
    g_mock_libssh2.scp_recv_size = 10; /* 9 bytes text + 1 trailing NUL */
    g_mock_libssh2.recv_chunk_count = 0;
    g_mock_libssh2.recv_chunk_idx = 0;
    mock_libssh2_push_recv_rc(LIBSSH2_ERROR_EAGAIN);
    mock_libssh2_push_recv("{\"ok\":", 6);
    mock_libssh2_push_recv("1}\0\0", 4); /* 3 bytes "1}\0" -> wait: let's pass exact 4 bytes "12}\0" */

    /* Reset with exact 10 bytes: "{\"ok\":12}\0" */
    g_mock_libssh2.recv_chunk_count = 0;
    g_mock_libssh2.recv_chunk_idx = 0;
    mock_libssh2_push_recv_rc(LIBSSH2_ERROR_EAGAIN);
    mock_libssh2_push_recv("{\"ok\":", 6);
    mock_libssh2_push_recv("12}\0", 4);

    TEST_ASSERT_TRUE(ssh_scp_download_file(s, "/etc/persistent/ubnt_lcm_gui.conf", out_file));

    char *content = NULL;
    size_t len = 0;
    TEST_ASSERT_TRUE(utils_read_file(out_file, &content, &len));
    TEST_ASSERT_EQUAL_UINT(9, len);
    TEST_ASSERT_EQUAL_STRING("{\"ok\":12}", content);
    free(content);

    ssh_session_destroy(s);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_ssh_global_init_and_cleanup);
    RUN_TEST(test_ssh_session_create_validation_and_connection_failures);
    RUN_TEST(test_ssh_session_auth_failures_and_recovery);
    RUN_TEST(test_ssh_exec_command_stdout_stderr_eagain_and_failures);
    RUN_TEST(test_ssh_scp_upload_success_partial_writes_and_errors);
    RUN_TEST(test_ssh_scp_download_success_trailing_nul_and_errors);

    return UNITY_END();
}
