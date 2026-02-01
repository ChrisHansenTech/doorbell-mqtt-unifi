#pragma once

#include "config_types.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct ssh_session ssh_session_t;

bool ssh_global_init(void);

void ssh_global_cleanup(void);

ssh_session_t *ssh_session_create(const config_ssh_t *ssh_cfg);

void ssh_session_destroy(ssh_session_t *session);

bool ssh_exec_command(ssh_session_t *session, const char *command, char **stdout_data, size_t *stdout_len, char **stderr_data, size_t *stderr_len);

bool ssh_scp_upload_file(ssh_session_t *session, const char *local_path, const char *remote_dir, unsigned long remote_mode);

bool ssh_scp_download_file(ssh_session_t *session, const char *remote_path, const char *local_path);