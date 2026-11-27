#define _POSIX_C_SOURCE 200809L

#include "mock_ssh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

mock_ssh_state_t g_mock_ssh;
static int g_fake_session_sentinel = 42;

void mock_ssh_reset(void) {
    memset(&g_mock_ssh, 0, sizeof(g_mock_ssh));
    g_mock_ssh.create_succeeds = true;
    g_mock_ssh.create_error = error_create(ERROR_SSH_CONNECTION_FAILED, "Failed to create SSH session");
}

ssh_session_t *mock_ssh_fake_session(void) {
    return (ssh_session_t *)&g_fake_session_sentinel;
}

ssh_session_t *__wrap_ssh_session_create_ex(const config_ssh_t *cfg, error_return_t *err_out) {
    g_mock_ssh.create_calls++;
    if (cfg) {
        g_mock_ssh.last_cfg = *cfg;
    }
    if (!g_mock_ssh.create_succeeds) {
        if (err_out) {
            *err_out = g_mock_ssh.create_error;
        }
        return NULL;
    }
    if (err_out) {
        *err_out = error_none();
    }
    return mock_ssh_fake_session();
}

ssh_session_t *__wrap_ssh_session_create(const config_ssh_t *cfg) {
    return __wrap_ssh_session_create_ex(cfg, NULL);
}

void __wrap_ssh_session_destroy(ssh_session_t *session) {
    if (session) {
        g_mock_ssh.destroy_calls++;
    }
}

bool __wrap_ssh_exec_command(
    ssh_session_t *session,
    const char *command,
    int *exit_status_out,
    char **stdout_data,
    size_t *stdout_len,
    char **stderr_data,
    size_t *stderr_len
) {
    (void)session;
    size_t idx = g_mock_ssh.exec_calls++;
    if (idx < MOCK_SSH_MAX_CALLS) {
        snprintf(g_mock_ssh.exec_cmds[idx], MOCK_SSH_MAX_CMD_LEN, "%s", command ? command : "");
    }

    if (stdout_data) *stdout_data = NULL;
    if (stdout_len) *stdout_len = 0;
    if (stderr_data) *stderr_data = NULL;
    if (stderr_len) *stderr_len = 0;
    if (exit_status_out) *exit_status_out = 0;

    if (g_mock_ssh.exec_cb) {
        return g_mock_ssh.exec_cb(command, exit_status_out, stdout_data, stdout_len, stderr_data, stderr_len, idx);
    }

    return true;
}

bool __wrap_ssh_scp_upload_file(ssh_session_t *session, const char *local_path, const char *remote_dir, unsigned long remote_mode) {
    (void)session;
    size_t idx = g_mock_ssh.upload_calls++;
    if (idx < MOCK_SSH_MAX_CALLS) {
        snprintf(g_mock_ssh.upload_locals[idx], MOCK_SSH_MAX_PATH_LEN, "%s", local_path ? local_path : "");
        snprintf(g_mock_ssh.upload_remotes[idx], MOCK_SSH_MAX_PATH_LEN, "%s", remote_dir ? remote_dir : "");
    }

    if (g_mock_ssh.upload_cb) {
        return g_mock_ssh.upload_cb(local_path, remote_dir, remote_mode, idx);
    }

    return true;
}

bool __wrap_ssh_scp_download_file(ssh_session_t *session, const char *remote_path, const char *local_path) {
    (void)session;
    size_t idx = g_mock_ssh.download_calls++;
    if (idx < MOCK_SSH_MAX_CALLS) {
        snprintf(g_mock_ssh.download_remotes[idx], MOCK_SSH_MAX_PATH_LEN, "%s", remote_path ? remote_path : "");
        snprintf(g_mock_ssh.download_locals[idx], MOCK_SSH_MAX_PATH_LEN, "%s", local_path ? local_path : "");
    }

    if (g_mock_ssh.download_cb) {
        return g_mock_ssh.download_cb(remote_path, local_path, idx);
    }

    return true;
}
