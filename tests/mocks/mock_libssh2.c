#define _POSIX_C_SOURCE 200809L

#include "mock_libssh2.h"

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

mock_libssh2_state_t g_mock_libssh2;

static int g_fake_session_obj = 1;
static int g_fake_channel_obj = 2;
static int g_fake_scp_recv_channel_obj = 3;

extern int __real_close(int fd);

void mock_libssh2_reset(void) {
    memset(&g_mock_libssh2, 0, sizeof(g_mock_libssh2));
    g_mock_libssh2.addrinfo_count = 1;
}

void mock_libssh2_push_stdout(const char *data, size_t len) {
    if (g_mock_libssh2.stdout_chunk_count < MOCK_LIBSSH2_MAX_CHUNKS) {
        g_mock_libssh2.stdout_chunks[g_mock_libssh2.stdout_chunk_count++] = (mock_libssh2_chunk_t){ data, len, 0 };
    }
}

void mock_libssh2_push_stdout_rc(ssize_t rc) {
    if (g_mock_libssh2.stdout_chunk_count < MOCK_LIBSSH2_MAX_CHUNKS) {
        g_mock_libssh2.stdout_chunks[g_mock_libssh2.stdout_chunk_count++] = (mock_libssh2_chunk_t){ NULL, 0, rc };
    }
}

void mock_libssh2_push_stderr(const char *data, size_t len) {
    if (g_mock_libssh2.stderr_chunk_count < MOCK_LIBSSH2_MAX_CHUNKS) {
        g_mock_libssh2.stderr_chunks[g_mock_libssh2.stderr_chunk_count++] = (mock_libssh2_chunk_t){ data, len, 0 };
    }
}

void mock_libssh2_push_stderr_rc(ssize_t rc) {
    if (g_mock_libssh2.stderr_chunk_count < MOCK_LIBSSH2_MAX_CHUNKS) {
        g_mock_libssh2.stderr_chunks[g_mock_libssh2.stderr_chunk_count++] = (mock_libssh2_chunk_t){ NULL, 0, rc };
    }
}

void mock_libssh2_push_recv(const char *data, size_t len) {
    if (g_mock_libssh2.recv_chunk_count < MOCK_LIBSSH2_MAX_CHUNKS) {
        g_mock_libssh2.recv_chunks[g_mock_libssh2.recv_chunk_count++] = (mock_libssh2_chunk_t){ data, len, 0 };
    }
}

void mock_libssh2_push_recv_rc(ssize_t rc) {
    if (g_mock_libssh2.recv_chunk_count < MOCK_LIBSSH2_MAX_CHUNKS) {
        g_mock_libssh2.recv_chunks[g_mock_libssh2.recv_chunk_count++] = (mock_libssh2_chunk_t){ NULL, 0, rc };
    }
}

int __wrap_libssh2_init(int flags) {
    (void)flags;
    g_mock_libssh2.init_calls++;
    return g_mock_libssh2.init_rc;
}

void __wrap_libssh2_exit(void) {
    g_mock_libssh2.exit_calls++;
}

int __wrap_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    (void)node;
    (void)service;
    (void)hints;
    g_mock_libssh2.getaddrinfo_calls++;
    if (g_mock_libssh2.getaddrinfo_rc != 0) {
        return g_mock_libssh2.getaddrinfo_rc;
    }

    int count = g_mock_libssh2.addrinfo_count > 0 ? g_mock_libssh2.addrinfo_count : 1;
    struct addrinfo *head = NULL;
    struct addrinfo *prev = NULL;

    for (int i = 0; i < count; i++) {
        struct addrinfo *ai = calloc(1, sizeof(*ai));
        struct sockaddr_in *sa = calloc(1, sizeof(*sa));
        sa->sin_family = AF_INET;
        ai->ai_family = AF_INET;
        ai->ai_socktype = SOCK_STREAM;
        ai->ai_protocol = IPPROTO_TCP;
        ai->ai_addr = (struct sockaddr *)sa;
        ai->ai_addrlen = sizeof(*sa);

        if (!head) {
            head = ai;
        } else {
            prev->ai_next = ai;
        }
        prev = ai;
    }

    *res = head;
    return 0;
}

void __wrap_freeaddrinfo(struct addrinfo *res) {
    g_mock_libssh2.freeaddrinfo_calls++;
    while (res) {
        struct addrinfo *next = res->ai_next;
        free(res->ai_addr);
        free(res);
        res = next;
    }
}

int __wrap_socket(int domain, int type, int protocol) {
    (void)domain;
    (void)type;
    (void)protocol;
    g_mock_libssh2.socket_calls++;
    if (g_mock_libssh2.socket_fail_on_call > 0 && (int)g_mock_libssh2.socket_calls == g_mock_libssh2.socket_fail_on_call) {
        errno = EMFILE;
        return -1;
    }
    return MOCK_LIBSSH2_FAKE_FD;
}

int __wrap_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    (void)sockfd;
    (void)addr;
    (void)addrlen;
    g_mock_libssh2.connect_calls++;
    if ((int)g_mock_libssh2.connect_calls <= g_mock_libssh2.connect_fail_count) {
        errno = ECONNREFUSED;
        return -1;
    }
    return 0;
}

int __wrap_close(int fd) {
    if (fd == MOCK_LIBSSH2_FAKE_FD) {
        g_mock_libssh2.close_calls++;
        return 0;
    }
    return __real_close(fd);
}

LIBSSH2_SESSION *__wrap_libssh2_session_init_ex(
    LIBSSH2_ALLOC_FUNC((*my_alloc)),
    LIBSSH2_FREE_FUNC((*my_free)),
    LIBSSH2_REALLOC_FUNC((*my_realloc)),
    void *abstract
) {
    (void)my_alloc;
    (void)my_free;
    (void)my_realloc;
    (void)abstract;
    g_mock_libssh2.session_init_calls++;
    if (g_mock_libssh2.session_init_returns_null) {
        return NULL;
    }
    return (LIBSSH2_SESSION *)&g_fake_session_obj;
}

void __wrap_libssh2_session_set_blocking(LIBSSH2_SESSION *session, int blocking) {
    (void)session;
    (void)blocking;
    g_mock_libssh2.session_blocking_calls++;
}

int __wrap_libssh2_session_handshake(LIBSSH2_SESSION *session, libssh2_socket_t sock) {
    (void)session;
    (void)sock;
    g_mock_libssh2.handshake_calls++;
    return g_mock_libssh2.handshake_rc;
}

int __wrap_libssh2_userauth_password_ex(
    LIBSSH2_SESSION *session,
    const char *username,
    unsigned int username_len,
    const char *password,
    unsigned int password_len,
    LIBSSH2_PASSWD_CHANGEREQ_FUNC((*passwd_change_cb))
) {
    (void)session;
    (void)passwd_change_cb;
    g_mock_libssh2.userauth_calls++;
    snprintf(g_mock_libssh2.last_user, sizeof(g_mock_libssh2.last_user), "%.*s", (int)username_len, username ? username : "");
    snprintf(g_mock_libssh2.last_password, sizeof(g_mock_libssh2.last_password), "%.*s", (int)password_len, password ? password : "");
    return g_mock_libssh2.userauth_rc;
}

int __wrap_libssh2_session_disconnect_ex(LIBSSH2_SESSION *session, int reason, const char *description, const char *lang) {
    (void)session;
    (void)reason;
    (void)description;
    (void)lang;
    g_mock_libssh2.session_disconnect_calls++;
    return 0;
}

int __wrap_libssh2_session_free(LIBSSH2_SESSION *session) {
    (void)session;
    g_mock_libssh2.session_free_calls++;
    return 0;
}

LIBSSH2_CHANNEL *__wrap_libssh2_channel_open_ex(
    LIBSSH2_SESSION *session,
    const char *channel_type,
    unsigned int channel_type_len,
    unsigned int window_size,
    unsigned int packet_size,
    const char *message,
    unsigned int message_len
) {
    (void)session;
    (void)channel_type;
    (void)channel_type_len;
    (void)window_size;
    (void)packet_size;
    (void)message;
    (void)message_len;
    g_mock_libssh2.channel_open_calls++;
    if (g_mock_libssh2.channel_open_returns_null) {
        return NULL;
    }
    return (LIBSSH2_CHANNEL *)&g_fake_channel_obj;
}

int __wrap_libssh2_channel_process_startup(
    LIBSSH2_CHANNEL *channel,
    const char *request,
    unsigned int request_len,
    const char *message,
    unsigned int message_len
) {
    (void)channel;
    (void)request;
    (void)request_len;
    g_mock_libssh2.channel_exec_calls++;
    snprintf(g_mock_libssh2.last_command, sizeof(g_mock_libssh2.last_command), "%.*s", (int)message_len, message ? message : "");
    return g_mock_libssh2.channel_exec_rc;
}

static ssize_t read_from_chunks(mock_libssh2_chunk_t *chunks, size_t count, size_t *idx, char *buf, size_t buflen) {
    if (*idx >= count) {
        return 0;
    }
    mock_libssh2_chunk_t *c = &chunks[(*idx)++];
    if (c->custom_rc < 0) {
        return c->custom_rc;
    }
    if (!c->data || c->len == 0) {
        return 0;
    }
    size_t to_copy = c->len < buflen ? c->len : buflen;
    memcpy(buf, c->data, to_copy);
    return (ssize_t)to_copy;
}

ssize_t __wrap_libssh2_channel_read_ex(LIBSSH2_CHANNEL *channel, int stream_id, char *buf, size_t buflen) {
    if (channel == (LIBSSH2_CHANNEL *)&g_fake_scp_recv_channel_obj) {
        return read_from_chunks(g_mock_libssh2.recv_chunks, g_mock_libssh2.recv_chunk_count, &g_mock_libssh2.recv_chunk_idx, buf, buflen);
    }
    if (stream_id == SSH_EXTENDED_DATA_STDERR) {
        return read_from_chunks(g_mock_libssh2.stderr_chunks, g_mock_libssh2.stderr_chunk_count, &g_mock_libssh2.stderr_chunk_idx, buf, buflen);
    }
    return read_from_chunks(g_mock_libssh2.stdout_chunks, g_mock_libssh2.stdout_chunk_count, &g_mock_libssh2.stdout_chunk_idx, buf, buflen);
}

ssize_t __wrap_libssh2_channel_write_ex(LIBSSH2_CHANNEL *channel, int stream_id, const char *buf, size_t buflen) {
    (void)channel;
    (void)stream_id;
    g_mock_libssh2.scp_write_calls++;
    if (g_mock_libssh2.scp_write_fail_on_call > 0 && (ssize_t)g_mock_libssh2.scp_write_calls == g_mock_libssh2.scp_write_fail_on_call) {
        return -1;
    }

    size_t n = buflen;
    if (g_mock_libssh2.scp_write_max_chunk > 0 && n > g_mock_libssh2.scp_write_max_chunk) {
        n = g_mock_libssh2.scp_write_max_chunk;
    }

    if (g_mock_libssh2.uploaded_len + n <= sizeof(g_mock_libssh2.uploaded_data)) {
        memcpy(g_mock_libssh2.uploaded_data + g_mock_libssh2.uploaded_len, buf, n);
        g_mock_libssh2.uploaded_len += n;
    }
    return (ssize_t)n;
}

int __wrap_libssh2_channel_eof(LIBSSH2_CHANNEL *channel) {
    (void)channel;
    g_mock_libssh2.eof_checks++;
    if (g_mock_libssh2.eof_delay_checks > 0 && g_mock_libssh2.eof_checks <= g_mock_libssh2.eof_delay_checks) {
        return 0;
    }
    if (g_mock_libssh2.stdout_chunk_idx < g_mock_libssh2.stdout_chunk_count ||
        g_mock_libssh2.stderr_chunk_idx < g_mock_libssh2.stderr_chunk_count) {
        return 0;
    }
    return 1;
}

int __wrap_libssh2_channel_close(LIBSSH2_CHANNEL *channel) {
    (void)channel;
    g_mock_libssh2.channel_close_calls++;
    return 0;
}

int __wrap_libssh2_channel_wait_closed(LIBSSH2_CHANNEL *channel) {
    (void)channel;
    g_mock_libssh2.channel_wait_closed_calls++;
    return 0;
}

int __wrap_libssh2_channel_get_exit_status(LIBSSH2_CHANNEL *channel) {
    (void)channel;
    return g_mock_libssh2.channel_exit_status;
}

int __wrap_libssh2_channel_free(LIBSSH2_CHANNEL *channel) {
    (void)channel;
    g_mock_libssh2.channel_free_calls++;
    return 0;
}

int __wrap_libssh2_channel_send_eof(LIBSSH2_CHANNEL *channel) {
    (void)channel;
    g_mock_libssh2.channel_send_eof_calls++;
    return 0;
}

int __wrap_libssh2_channel_wait_eof(LIBSSH2_CHANNEL *channel) {
    (void)channel;
    g_mock_libssh2.channel_wait_eof_calls++;
    return 0;
}

LIBSSH2_CHANNEL *__wrap_libssh2_scp_send64(
    LIBSSH2_SESSION *session,
    const char *path,
    int mode,
    libssh2_int64_t size,
    time_t mtime,
    time_t atime
) {
    (void)session;
    (void)mtime;
    (void)atime;
    g_mock_libssh2.scp_send_calls++;
    snprintf(g_mock_libssh2.last_scp_path, sizeof(g_mock_libssh2.last_scp_path), "%s", path ? path : "");
    g_mock_libssh2.last_scp_mode = mode;
    g_mock_libssh2.last_scp_size = size;
    if (g_mock_libssh2.scp_send_returns_null) {
        return NULL;
    }
    return (LIBSSH2_CHANNEL *)&g_fake_channel_obj;
}

LIBSSH2_CHANNEL *__wrap_libssh2_scp_recv2(LIBSSH2_SESSION *session, const char *path, libssh2_struct_stat *sb) {
    (void)session;
    g_mock_libssh2.scp_recv_calls++;
    snprintf(g_mock_libssh2.last_scp_path, sizeof(g_mock_libssh2.last_scp_path), "%s", path ? path : "");
    if (g_mock_libssh2.scp_recv_returns_null) {
        return NULL;
    }
    if (sb) {
        sb->st_size = g_mock_libssh2.scp_recv_size;
    }
    return (LIBSSH2_CHANNEL *)&g_fake_scp_recv_channel_obj;
}

int __wrap_nanosleep(const struct timespec *req, struct timespec *rem) {
    (void)req;
    (void)rem;
    g_mock_libssh2.nanosleep_calls++;
    return 0;
}
