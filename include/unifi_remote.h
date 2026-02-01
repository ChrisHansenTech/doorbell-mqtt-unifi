#pragma  once

#include "ssh.h"
#include "unifi_profile.h"
#include <stdbool.h>

bool unifi_conf_download_and_load(ssh_session_t *session, const char *tmp_dir, unifi_profile_t *out);

bool unifi_profile_download_and_load(ssh_session_t *session, const char *tmp_dir, unifi_profile_t *out);

bool unifi_profile_upload_and_apply(ssh_session_t *session, const char *profile_dir, const unifi_profile_t *profile);
