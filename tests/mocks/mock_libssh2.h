#pragma once

#include <libssh2.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>

#define MOCK_LIBSSH2_FAKE_FD 9999
#define MOCK_LIBSSH2_MAX_CHUNKS 16
#define MOCK_LIBSSH2_UPLOAD_BUF_SIZE 65536

typedef struct {
    const char *data;
    size_t len;
    ssize_t custom_rc; /* if < 0, return this rc (e.g. LIBSSH2_ERROR_EAGAIN or -1) */
} mock_libssh2_chunk_t;

typedef struct {
    /* Return value controls */
    int init_rc;
    int getaddrinfo_rc;
    int addrinfo_count;
    int socket_fail_on_call; /* 0 = never fail; 1-based index to fail */
    int connect_fail_count;  /* number of initial connect() calls that fail */
    bool session_init_returns_null;
    int handshake_rc;
    int userauth_rc;

    bool channel_open_returns_null;
    int channel_exec_rc;
    int channel_exit_status;

    /* Read chunks for stdout (stream 0) and stderr (stream 1) */
    mock_libssh2_chunk_t stdout_chunks[MOCK_LIBSSH2_MAX_CHUNKS];
    size_t stdout_chunk_count;
    size_t stdout_chunk_idx;

    mock_libssh2_chunk_t stderr_chunks[MOCK_LIBSSH2_MAX_CHUNKS];
    size_t stderr_chunk_count;
    size_t stderr_chunk_idx;

    /* EOF control for exec: after how many eof checks to return 1 (0 = when both streams drained) */
    int eof_delay_checks;
    int eof_checks;

    /* SCP send controls */
    bool scp_send_returns_null;
    ssize_t scp_write_fail_on_call; /* 0 = never fail; 1-based call index */
    size_t scp_write_max_chunk;     /* 0 = write full requested len; >0 = cap per write for partial write testing */

    /* SCP recv controls */
    bool scp_recv_returns_null;
    off_t scp_recv_size;
    mock_libssh2_chunk_t recv_chunks[MOCK_LIBSSH2_MAX_CHUNKS];
    size_t recv_chunk_count;
    size_t recv_chunk_idx;

    /* Call counters & captured state */
    size_t init_calls;
    size_t exit_calls;
    size_t getaddrinfo_calls;
    size_t freeaddrinfo_calls;
    size_t socket_calls;
    size_t connect_calls;
    size_t close_calls;
    size_t session_init_calls;
    size_t session_blocking_calls;
    size_t handshake_calls;
    size_t userauth_calls;
    size_t session_disconnect_calls;
    size_t session_free_calls;
    size_t channel_open_calls;
    size_t channel_exec_calls;
    size_t channel_close_calls;
    size_t channel_wait_closed_calls;
    size_t channel_free_calls;
    size_t channel_send_eof_calls;
    size_t channel_wait_eof_calls;
    size_t scp_send_calls;
    size_t scp_write_calls;
    size_t scp_recv_calls;
    size_t nanosleep_calls;

    char last_user[64];
    char last_password[128];
    char last_command[512];
    char last_scp_path[512];
    int last_scp_mode;
    libssh2_int64_t last_scp_size;

    unsigned char uploaded_data[MOCK_LIBSSH2_UPLOAD_BUF_SIZE];
    size_t uploaded_len;
} mock_libssh2_state_t;

extern mock_libssh2_state_t g_mock_libssh2;

void mock_libssh2_reset(void);
void mock_libssh2_push_stdout(const char *data, size_t len);
void mock_libssh2_push_stdout_rc(ssize_t rc);
void mock_libssh2_push_stderr(const char *data, size_t len);
void mock_libssh2_push_stderr_rc(ssize_t rc);
void mock_libssh2_push_recv(const char *data, size_t len);
void mock_libssh2_push_recv_rc(ssize_t rc);
