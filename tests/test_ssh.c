#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "mocks/mock_libssh2.h"
#include "support/test_helpers.h"
#include "config_types.h"
#include "ssh.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <netdb.h>
#include <poll.h>
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

void test_ssh_tcp_connect_success_immediate_and_async_restores_blocking(void) {
    config_ssh_t cfg = make_ssh_cfg();
    error_return_t err = {0};

    /* 1. Immediate connect() == 0 still uses non-blocking mode and restores blocking before handshake */
    mock_libssh2_push_connect(0, 0);
    ssh_session_t *s = ssh_session_create_ex(&cfg, &err);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_TRUE(g_mock_libssh2.connect_saw_nonblock);
    TEST_ASSERT_FALSE(g_mock_libssh2.handshake_saw_nonblock);
    TEST_ASSERT_EQUAL_UINT(0, g_mock_libssh2.poll_calls);
    TEST_ASSERT_EQUAL_UINT(2, g_mock_libssh2.fcntl_setfl_calls);
    TEST_ASSERT_TRUE((g_mock_libssh2.fcntl_setfl_history[0] & O_NONBLOCK) != 0);
    TEST_ASSERT_TRUE((g_mock_libssh2.fcntl_setfl_history[1] & O_NONBLOCK) == 0);
    ssh_session_destroy(s);

    /* 2. Asynchronous EINPROGRESS -> poll(10000ms) POLLOUT -> SO_ERROR == 0 -> blocking restored */
    mock_libssh2_reset();
    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(1, 0, POLLOUT, 50);
    g_mock_libssh2.getsockopt_so_error = 0;

    s = ssh_session_create_ex(&cfg, &err);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_TRUE(g_mock_libssh2.connect_saw_nonblock);
    TEST_ASSERT_FALSE(g_mock_libssh2.handshake_saw_nonblock);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.poll_calls);
    TEST_ASSERT_EQUAL_INT(SSH_TCP_CONNECT_TIMEOUT_MS, g_mock_libssh2.poll_timeouts[0]);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.getsockopt_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.session_blocking_calls);
    ssh_session_destroy(s);
}

void test_ssh_tcp_connect_timeout_silently_dropped_packets(void) {
    config_ssh_t cfg = make_ssh_cfg();
    error_return_t err = {0};

    /* Simulate firewall silently dropping SYN packets: EINPROGRESS then poll() returns 0 */
    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(0, 0, 0, SSH_TCP_CONNECT_TIMEOUT_MS);

    ssh_session_t *s = ssh_session_create_ex(&cfg, &err);
    TEST_ASSERT_NULL(s);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.connect_calls);
    TEST_ASSERT_TRUE(g_mock_libssh2.connect_saw_nonblock);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.poll_calls);
    TEST_ASSERT_EQUAL_INT(SSH_TCP_CONNECT_TIMEOUT_MS, g_mock_libssh2.poll_timeouts[0]);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_TIMEOUT, err.error_code);
    TEST_ASSERT_NOT_NULL(strstr(err.message, "timed out"));
    TEST_ASSERT_NOT_NULL(strstr(err.message, "192.168.1.50:22"));
    TEST_ASSERT_NOT_NULL(strstr(err.message, "10s"));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.freeaddrinfo_calls);
    TEST_ASSERT_EQUAL_UINT(0, g_mock_libssh2.session_init_calls);
}

void test_ssh_tcp_connect_immediate_and_async_connection_refused(void) {
    config_ssh_t cfg = make_ssh_cfg();
    error_return_t err = {0};

    /* Immediate ECONNREFUSED: fails immediately without polling */
    mock_libssh2_push_connect(-1, ECONNREFUSED);
    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_UINT(0, g_mock_libssh2.poll_calls);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_FAILED, err.error_code);
    TEST_ASSERT_NOT_NULL(strstr(err.message, "192.168.1.50:22"));
    TEST_ASSERT_NOT_NULL(strstr(err.message, strerror(ECONNREFUSED)));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);

    /* Asynchronous ECONNREFUSED reported via SO_ERROR after poll() */
    mock_libssh2_reset();
    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(1, 0, POLLOUT | POLLERR, 5);
    g_mock_libssh2.getsockopt_so_error = ECONNREFUSED;

    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.poll_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.getsockopt_calls);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_FAILED, err.error_code);
    TEST_ASSERT_NOT_NULL(strstr(err.message, strerror(ECONNREFUSED)));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);
}

void test_ssh_tcp_connect_host_and_network_unreachable(void) {
    config_ssh_t cfg = make_ssh_cfg();
    error_return_t err = {0};

    /* Immediate EHOSTUNREACH */
    mock_libssh2_push_connect(-1, EHOSTUNREACH);
    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_UINT(0, g_mock_libssh2.poll_calls);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_FAILED, err.error_code);
    TEST_ASSERT_NOT_NULL(strstr(err.message, strerror(EHOSTUNREACH)));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);

    /* Immediate ENETUNREACH */
    mock_libssh2_reset();
    mock_libssh2_push_connect(-1, ENETUNREACH);
    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_UINT(0, g_mock_libssh2.poll_calls);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_FAILED, err.error_code);
    TEST_ASSERT_NOT_NULL(strstr(err.message, strerror(ENETUNREACH)));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);

    /* Async EHOSTUNREACH via SO_ERROR */
    mock_libssh2_reset();
    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(1, 0, POLLOUT | POLLERR, 10);
    g_mock_libssh2.getsockopt_so_error = EHOSTUNREACH;
    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_FAILED, err.error_code);
    TEST_ASSERT_NOT_NULL(strstr(err.message, strerror(EHOSTUNREACH)));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);
}

void test_ssh_tcp_connect_interrupted_attempts_eintr(void) {
    config_ssh_t cfg = make_ssh_cfg();
    error_return_t err = {0};

    /* 1. connect() returns EINTR -> per POSIX, wait asynchronously via poll() without retrying connect() */
    mock_libssh2_push_connect(-1, EINTR);
    mock_libssh2_push_poll(1, 0, POLLOUT, 10);
    g_mock_libssh2.getsockopt_so_error = 0;

    ssh_session_t *s = ssh_session_create_ex(&cfg, &err);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.connect_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.poll_calls);
    TEST_ASSERT_FALSE(g_mock_libssh2.handshake_saw_nonblock);
    ssh_session_destroy(s);

    /* 2. poll() returns EINTR after 3000ms -> retries poll() with 7000ms remaining and succeeds */
    mock_libssh2_reset();
    g_mock_libssh2.use_mock_clock = true;
    g_mock_libssh2.mock_clock_ms = 1000;
    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(-1, EINTR, 0, 3000);
    mock_libssh2_push_poll(1, 0, POLLOUT, 50);
    g_mock_libssh2.getsockopt_so_error = 0;

    s = ssh_session_create_ex(&cfg, &err);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_EQUAL_UINT(2, g_mock_libssh2.poll_calls);
    TEST_ASSERT_EQUAL_INT(10000, g_mock_libssh2.poll_timeouts[0]);
    TEST_ASSERT_EQUAL_INT(7000, g_mock_libssh2.poll_timeouts[1]);
    TEST_ASSERT_FALSE(g_mock_libssh2.handshake_saw_nonblock);
    ssh_session_destroy(s);

    /* 3. Repeated poll() EINTR exhausts monotonic 10s deadline -> returns ERROR_SSH_CONNECTION_TIMEOUT */
    mock_libssh2_reset();
    g_mock_libssh2.use_mock_clock = true;
    g_mock_libssh2.mock_clock_ms = 5000;
    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(-1, EINTR, 0, 6000);
    mock_libssh2_push_poll(-1, EINTR, 0, 4500);

    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_UINT(2, g_mock_libssh2.poll_calls);
    TEST_ASSERT_EQUAL_INT(10000, g_mock_libssh2.poll_timeouts[0]);
    TEST_ASSERT_EQUAL_INT(4000, g_mock_libssh2.poll_timeouts[1]);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_TIMEOUT, err.error_code);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);
}

void test_ssh_tcp_connect_socket_cleanup_on_failures_and_multi_addr(void) {
    config_ssh_t cfg = make_ssh_cfg();
    error_return_t err = {0};

    /* Multi-addrinfo: first times out, second is refused -> both sockets closed */
    g_mock_libssh2.addrinfo_count = 2;
    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(0, 0, 0, SSH_TCP_CONNECT_TIMEOUT_MS);
    mock_libssh2_push_connect(-1, ECONNREFUSED);

    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_UINT(2, g_mock_libssh2.socket_calls);
    TEST_ASSERT_EQUAL_UINT(2, g_mock_libssh2.close_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.freeaddrinfo_calls);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_FAILED, err.error_code);

    /* Multi-addrinfo: first times out, second succeeds -> first socket closed, second kept until destroy */
    mock_libssh2_reset();
    g_mock_libssh2.addrinfo_count = 2;
    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(0, 0, 0, SSH_TCP_CONNECT_TIMEOUT_MS);
    mock_libssh2_push_connect(0, 0);

    ssh_session_t *s = ssh_session_create_ex(&cfg, &err);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_EQUAL_UINT(2, g_mock_libssh2.socket_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);
    ssh_session_destroy(s);
    TEST_ASSERT_EQUAL_UINT(2, g_mock_libssh2.close_calls);

    /* fcntl F_GETFL failure closes socket */
    mock_libssh2_reset();
    g_mock_libssh2.fcntl_getfl_fail = true;
    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_FAILED, err.error_code);

    /* fcntl F_SETFL failure when enabling O_NONBLOCK closes socket */
    mock_libssh2_reset();
    g_mock_libssh2.fcntl_setfl_fail_on_call = 1;
    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);

    /* fcntl F_SETFL failure when restoring blocking flags after connect closes socket */
    mock_libssh2_reset();
    g_mock_libssh2.fcntl_setfl_fail_on_call = 2;
    mock_libssh2_push_connect(0, 0);
    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);

    /* getsockopt(SO_ERROR) failure closes socket */
    mock_libssh2_reset();
    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(1, 0, POLLOUT, 5);
    g_mock_libssh2.getsockopt_fail = true;
    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);

    /* poll() non-EINTR error closes socket */
    mock_libssh2_reset();
    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(-1, ENOMEM, 0, 5);
    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);

    /* poll() reports POLLERR/POLLHUP even when SO_ERROR == 0 -> closes socket */
    mock_libssh2_reset();
    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(1, 0, POLLERR, 5);
    g_mock_libssh2.getsockopt_so_error = 0;
    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);
}

void test_ssh_tcp_connect_retry_succeeds_after_timeout(void) {
    config_ssh_t cfg = make_ssh_cfg();
    error_return_t err = {0};

    /* Attempt 1: timeout; Attempt 2: async connect succeeds without restarting anything */
    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(0, 0, 0, SSH_TCP_CONNECT_TIMEOUT_MS);
    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(1, 0, POLLOUT, 20);
    g_mock_libssh2.getsockopt_so_error = 0;

    TEST_ASSERT_NULL(ssh_session_create_ex(&cfg, &err));
    TEST_ASSERT_EQUAL_INT(ERROR_SSH_CONNECTION_TIMEOUT, err.error_code);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.close_calls);

    ssh_session_t *s = ssh_session_create_ex(&cfg, &err);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_FALSE(g_mock_libssh2.handshake_saw_nonblock);

    mock_libssh2_push_stdout("ok", 2);
    char *out_buf = NULL;
    TEST_ASSERT_TRUE(ssh_exec_command(s, "echo ok", NULL, &out_buf, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_STRING("ok", out_buf);
    free(out_buf);

    ssh_session_destroy(s);
    TEST_ASSERT_EQUAL_UINT(2, g_mock_libssh2.close_calls);
}

void test_ssh_established_session_preserved_after_nonblock_connect(void) {
    config_ssh_t cfg = make_ssh_cfg();
    error_return_t err = {0};

    mock_libssh2_push_connect(-1, EINPROGRESS);
    mock_libssh2_push_poll(1, 0, POLLOUT, 15);
    g_mock_libssh2.getsockopt_so_error = 0;

    ssh_session_t *s = ssh_session_create_ex(&cfg, &err);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(ERROR_NONE, err.error_code);
    TEST_ASSERT_FALSE(g_mock_libssh2.handshake_saw_nonblock);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_libssh2.session_blocking_calls);

    /* Exec works on established session */
    mock_libssh2_push_stdout("alive", 5);
    char *out_buf = NULL;
    TEST_ASSERT_TRUE(ssh_exec_command(s, "uptime", NULL, &out_buf, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_STRING("alive", out_buf);
    free(out_buf);

    /* SCP upload works on established session */
    char local_up[PATH_MAX];
    snprintf(local_up, sizeof(local_up), "%s/clip.ogg", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(local_up, "payload"));
    TEST_ASSERT_TRUE(ssh_scp_upload_file(s, local_up, "/tmp/dst", 0644));
    TEST_ASSERT_EQUAL_UINT(7, g_mock_libssh2.uploaded_len);

    /* SCP download works on established session */
    char local_dn[PATH_MAX];
    snprintf(local_dn, sizeof(local_dn), "%s/conf.json", g_tmp_dir);
    g_mock_libssh2.scp_recv_size = 4;
    mock_libssh2_push_recv("data", 4);
    TEST_ASSERT_TRUE(ssh_scp_download_file(s, "/etc/conf.json", local_dn));

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

    /* Issue #47: TCP connect timeout regression tests */
    RUN_TEST(test_ssh_tcp_connect_success_immediate_and_async_restores_blocking);
    RUN_TEST(test_ssh_tcp_connect_timeout_silently_dropped_packets);
    RUN_TEST(test_ssh_tcp_connect_immediate_and_async_connection_refused);
    RUN_TEST(test_ssh_tcp_connect_host_and_network_unreachable);
    RUN_TEST(test_ssh_tcp_connect_interrupted_attempts_eintr);
    RUN_TEST(test_ssh_tcp_connect_socket_cleanup_on_failures_and_multi_addr);
    RUN_TEST(test_ssh_tcp_connect_retry_succeeds_after_timeout);
    RUN_TEST(test_ssh_established_session_preserved_after_nonblock_connect);

    return UNITY_END();
}
