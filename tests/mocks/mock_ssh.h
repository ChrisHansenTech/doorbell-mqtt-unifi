#pragma once

#include "ssh.h"
#include <stdbool.h>
#include <stddef.h>

#define MOCK_SSH_MAX_CALLS 64
#define MOCK_SSH_MAX_CMD_LEN 2048
#define MOCK_SSH_MAX_PATH_LEN 512

typedef bool (*mock_ssh_exec_cb)(
    const char *command,
    int *exit_status_out,
    char **stdout_data,
    size_t *stdout_len,
    char **stderr_data,
    size_t *stderr_len,
    size_t call_index
);

typedef bool (*mock_ssh_upload_cb)(
    const char *local_path,
    const char *remote_dir,
    unsigned long remote_mode,
    size_t call_index
);

typedef bool (*mock_ssh_download_cb)(
    const char *remote_path,
    const char *local_path,
    size_t call_index
);

typedef struct {
    bool create_succeeds;
    size_t create_calls;
    size_t destroy_calls;
    config_ssh_t last_cfg;

    size_t exec_calls;
    char exec_cmds[MOCK_SSH_MAX_CALLS][MOCK_SSH_MAX_CMD_LEN];
    mock_ssh_exec_cb exec_cb;

    size_t upload_calls;
    char upload_locals[MOCK_SSH_MAX_CALLS][MOCK_SSH_MAX_PATH_LEN];
    char upload_remotes[MOCK_SSH_MAX_CALLS][MOCK_SSH_MAX_PATH_LEN];
    mock_ssh_upload_cb upload_cb;

    size_t download_calls;
    char download_remotes[MOCK_SSH_MAX_CALLS][MOCK_SSH_MAX_PATH_LEN];
    char download_locals[MOCK_SSH_MAX_CALLS][MOCK_SSH_MAX_PATH_LEN];
    mock_ssh_download_cb download_cb;
} mock_ssh_state_t;

extern mock_ssh_state_t g_mock_ssh;

void mock_ssh_reset(void);
ssh_session_t *mock_ssh_fake_session(void);
