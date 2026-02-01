#include "ssh.h"
#include "config_types.h"
#include "logger.h"

#include <errno.h>
#include <fcntl.h>
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <linux/limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

struct ssh_session {
    int sock;
    LIBSSH2_SESSION *session;
    config_ssh_t cfg;
};

bool ssh_global_init(void) {
    int rc = libssh2_init(0);
    if (rc != 0) {
        LOG_ERROR("libssh2_init failed with code %d", rc);
        return false;
    }

    return true;
}

void ssh_global_cleanup(void) {
    libssh2_exit();
} 

static int ssh_connect_tcp(const char *host, int port) {
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints;
    struct addrinfo *res = NULL;
    
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(host, port_str, &hints, &res);

    if (rc != 0) {
        LOG_ERROR("getaddrinfo failed for %s:%s: %s", host, port_str, gai_strerror(rc));
        return -1;
    }

    int sock = -1;
    struct addrinfo *p;
    for (p = res; p != NULL; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) {
            continue;
        }

        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0 ) {
            break;
        }

        close(sock);
        sock = -1;
    }

    freeaddrinfo(res);

    if (sock < 0) {
        LOG_ERROR("Failed to connect to %s:%d", host, port);
    }

    return sock;
}

static bool ssh_authenticate(LIBSSH2_SESSION *session, const config_ssh_t *cfg) {
    
    const char *password = getenv(cfg->password_env);

    if (!password || !*password) {
        LOG_ERROR("Environment variable '%s' is not set or empty. SSH Authentication cannot proceed.", cfg->password_env);
        return false;
    }

    int rc = libssh2_userauth_password(session, cfg->user, password);

    if (rc != 0) {
        LOG_ERROR("Authentication failed for user '%s' (rc=%d)", cfg->user, rc);
        return false;
    }

    LOG_INFO("Authenticated as '%s'", cfg->user);

    return true;
}

ssh_session_t *ssh_session_create(const config_ssh_t *cfg) {
    if (!cfg || cfg->host[0] == '\0' || cfg->user[0] == '\0') {
        LOG_ERROR("ssh_session_create: invalid configuration");
        return NULL;
    }

    int sock = ssh_connect_tcp(cfg->host, cfg->port);
    if (sock < 0) {
        return NULL;
    }

    LIBSSH2_SESSION *session = libssh2_session_init();
    if (!session) {
        LOG_ERROR("libssh2_session_init failed");
        close(sock);
        return NULL;
    }

    libssh2_session_set_blocking(session, 1);

    int rc = libssh2_session_handshake(session, sock);
    if (rc != 0) {
        LOG_ERROR("libssh2_session_handshake failed: %d", rc);
        libssh2_session_free(session);
        close(sock);
        return NULL;
    }

    if (!ssh_authenticate(session, cfg)) {
        libssh2_session_disconnect(session, "Authentication failed");
        libssh2_session_free(session);
        close(sock);
        return NULL;
    }

    ssh_session_t *s = calloc(1, sizeof(*s));
    if (!s) {
        LOG_ERROR("Out of memory creating ssh_session_t");
        libssh2_session_disconnect(session, "Memory error");
        libssh2_session_free(session);
        close(sock);
        return NULL;
    }

    s->sock = sock;
    s->session = session;
    s->cfg = *cfg;

    LOG_INFO("SSH session established to %s:%d", cfg->host, cfg->port);
    return s;
}

void ssh_session_destroy(ssh_session_t *s) {
    if (!s) {
        return;
    }

    if (s->session) {
        libssh2_session_disconnect(s->session, "Normal shutdown");
        libssh2_session_free(s->session);
    }

    if (s->sock >= 0) {
        close(s->sock);
    }

    free(s);
}

bool ssh_exec_command(ssh_session_t *s, const char *command, char **stdout_data, size_t *stdout_len, char **stderr_data, size_t *stderr_len) {
    if (!s || !s->session || !command) {
        LOG_ERROR("ssh_exec_command: invalid arguments.");
        return false;
    }

    if (stdout_data) *stdout_data = NULL;
    if (stdout_len) *stdout_len = 0;
    if (stderr_data) *stderr_data = NULL;
    if (stderr_len) *stderr_len = 0;

    LIBSSH2_CHANNEL *channel = libssh2_channel_open_session(s->session);
    if (!channel) {
        LOG_ERROR("libssh2_channel_open_session failed.");
        return false;
    }

    int rc = libssh2_channel_exec(channel, command);
    if (rc != 0) {
        LOG_ERROR("libssh2_channel_exec failed for command '%s' (rc=%d)", command, rc);
        libssh2_channel_free(channel);
        return false;
    }

    char buffer[4096];
    char *out_buf = NULL; 
    size_t out_size = 0;
    char *err_buf = NULL; 
    size_t err_size = 0;

    for (;;) {
        bool did_work = false;

        // stdout
        for (;;) {
            ssize_t n = libssh2_channel_read(channel, buffer, sizeof(buffer));
            if (n == LIBSSH2_ERROR_EAGAIN) break;
            if (n <= 0) break;

            char *tmp = realloc(out_buf, out_size + (size_t)n + 1);
            if (!tmp) {
                LOG_ERROR("Out of memory reading stdout.");
                free(out_buf);
                free(err_buf);
                libssh2_channel_close(channel);
                libssh2_channel_free(channel);
                return false;
            }
            out_buf = tmp;
            memcpy(out_buf + out_size, buffer, (size_t)n);
            out_size += (size_t)n;
            out_buf[out_size] = '\0';
            did_work = true;
        }

        // stderr
        for (;;) {
            ssize_t n = libssh2_channel_read_stderr(channel, buffer, sizeof(buffer));
            if (n == LIBSSH2_ERROR_EAGAIN) break;
            if (n <= 0) break;

            char *tmp = realloc(err_buf, err_size + (size_t)n + 1);
            if (!tmp) {
                LOG_ERROR("Out of memory reading stderr.");
                free(out_buf);
                free(err_buf);
                libssh2_channel_close(channel);
                libssh2_channel_free(channel);
                return false;
            }
            err_buf = tmp;
            memcpy(err_buf + err_size, buffer, (size_t)n);
            err_size += (size_t)n;
            err_buf[err_size] = '\0';
            did_work = true;
        }

        // Exit condition: remote closed / EOF and no more data to read.
        if (libssh2_channel_eof(channel)) {
            break;
        }

        // Avoid busy-spin if nothing was read this iteration.
        if (!did_work) {
            // Ideally call your existing libssh2 "wait for socket" helper here.
            // As a minimal improvement you can yield briefly:
            // usleep(1000);
        }
    }

    libssh2_channel_close(channel);
    libssh2_channel_wait_closed(channel);

    int exit_status = libssh2_channel_get_exit_status(channel);
    libssh2_channel_free(channel);

    if (stdout_data) {
        *stdout_data = out_buf;
        if (stdout_len) *stdout_len = out_size;
    } else {
        free(out_buf);
    }

    if (stderr_data) {
        *stderr_data = err_buf;
        if (stderr_len) *stderr_len = err_size;
    } else {
        free(err_buf);
    }

    if (exit_status != 0) {
        LOG_ERROR("SSH command exited non-zero (exit=%d): %s", exit_status, command);
        return false;
    }

    return true;
}

bool ssh_scp_upload_file(ssh_session_t *s, const char *local_path, const char *remote_dir, unsigned long remote_mode) {
    if (!s || !s->session || !local_path || !remote_dir) {
        LOG_ERROR("ssh_scp_upload_file: invalid arguments.");
        return false;
    }

    const char *base = strrchr(local_path, '/');
    base = base ? base + 1 : local_path;

    char remote_path[PATH_MAX];
    int n = snprintf(remote_path, sizeof(remote_path), "%s/%s", remote_dir, base);

    if (n <= 0 || (size_t)n >= sizeof(remote_path)) {
        LOG_ERROR("Remote path truncated.");
        return false;
    }

    FILE *fp = fopen(local_path, "rb");
    if (!fp) {
        LOG_ERROR("Failed to open local file '%s': %s", local_path, strerror(errno));
        return false;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_size < 0) {
        LOG_ERROR("ftell failed for '%s'", local_path);
        fclose(fp);
        return false;
    }

    LIBSSH2_CHANNEL *channel = libssh2_scp_send64(s->session, remote_path, (int)remote_mode, (libssh2_uint64_t)file_size, 0, 0);
    if (!channel) {
        LOG_ERROR("libssh2_scp_send64 failed for remote path '%s'", remote_path);
        fclose(fp);
        return false;
    }

    char buffer[4096];
    size_t nread;
    while ((nread = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        char *ptr = buffer;
        while (nread > 0) {
            ssize_t nwritten = libssh2_channel_write(channel, ptr, nread);
            if (nwritten < 0) {
                LOG_ERROR("Error writing to SCP channel for '%s'", remote_path);
                libssh2_channel_free(channel);
                fclose(fp);
                return false;
            }
            ptr   += nwritten;
            nread -= nwritten;
        }
    }

    fclose(fp);
    libssh2_channel_send_eof(channel);
    libssh2_channel_wait_eof(channel);
    libssh2_channel_wait_closed(channel);
    libssh2_channel_free(channel);

    LOG_INFO("SCP upload complete: %s -> %s", local_path, remote_path);
    return true;
}

bool ssh_scp_download_file(ssh_session_t *s, const char *remote_path, const char *local_path)
{
    if (!s || !s->session || !remote_path || !local_path) {
        LOG_ERROR("ssh_scp_download_file: invalid arguments");
        return false;
    }

    struct stat sb;
    memset(&sb, 0, sizeof(sb));

    LIBSSH2_CHANNEL *channel = libssh2_scp_recv2(s->session, remote_path, &sb);
    if (!channel) {
        LOG_ERROR("libssh2_scp_recv2 failed for '%s'", remote_path);
        return false;
    }

    FILE *fp = fopen(local_path, "wb");
    if (!fp) {
        LOG_ERROR("Failed to open local file '%s' for writing: %s", local_path, strerror(errno));
        libssh2_channel_free(channel);
        return false;
    }

    char buffer[4096];
    off_t remaining = sb.st_size;
    off_t total = 0;

    while (remaining > 0) {
        size_t want = (size_t)((remaining < (off_t)sizeof(buffer)) ? remaining : (off_t)sizeof(buffer));

        ssize_t n = libssh2_channel_read(channel, buffer, want);
        
        if (n == LIBSSH2_ERROR_EAGAIN) {
             continue;
        }

          if (n < 0) {
            LOG_ERROR("libssh2_channel_read failed for '%s' -> '%s': rc=%zd",
                      remote_path, local_path, n);
            fclose(fp);
            libssh2_channel_free(channel);
            return false;
        }

        if (n == 0) {
            LOG_ERROR("Unexpected EOF while reading '%s' (remaining=%lld)",
                      remote_path, (long long)remaining);
            fclose(fp);
            libssh2_channel_free(channel);
            return false;
        }

        size_t to_write = (size_t)n;
        
        if ((off_t)n == remaining && to_write > 0 && buffer[to_write - 1] == '\0') {
            to_write -= 1;
            LOG_DEBUG("Stripped trailing NUL from '%s'", remote_path);
        }

        if (to_write > 0) {
            size_t nwritten = fwrite(buffer, 1, to_write, fp);
            if (nwritten != to_write) {
                LOG_ERROR("Short write to local file '%s': %s", local_path, strerror(errno));
                fclose(fp);
                libssh2_channel_free(channel);
                return false;
            }
            total += (off_t)to_write;
        }   

        remaining -= (off_t)n;
    }

    fclose(fp);
    libssh2_channel_send_eof(channel);
    libssh2_channel_wait_eof(channel);
    libssh2_channel_wait_closed(channel);
    libssh2_channel_free(channel);

    struct stat out;
    if (stat(local_path, &out) == 0) {
        LOG_INFO("SCP download complete: %s -> %s (expected=%lld, read=%lld, saved=%lld)",
             remote_path, local_path,
             (long long)sb.st_size, (long long)total, (long long)out.st_size);
    } else {
        LOG_INFO("SCP download complete: %s -> %s (expected=%lld, read=%lld)",
             remote_path, local_path,
             (long long)sb.st_size, (long long)total);
    }
    
    return true;
}