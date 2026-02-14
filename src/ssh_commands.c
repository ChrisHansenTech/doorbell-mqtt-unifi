#include "ssh_commands.h"
#include "logger.h"

#include <ctype.h>
#include <linux/limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool ssh_arg_is_safe_single_quoted(const char *s) {
    if (!s || !*s) return false;

    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '\'') return false;         
        if (*p == '\n' || *p == '\r') return false;
    }
    return true;
}

__attribute__((format(printf, 4, 5)))
static bool cmd_append(char *out, size_t out_sz, size_t *len, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out + *len, out_sz - *len, fmt, ap);
    va_end(ap);

    if (n < 0 || (size_t)n >= out_sz - *len) return false;
    *len += (size_t)n;
    return true;
}

static const char *find_last_substr(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) {
        return NULL;
    }

    const char *last = NULL;
    const char *p = haystack;

    while ((p = strstr(p, needle)) != NULL) {
        last = p;
        p += 1;       
    } 
    return last;
}

bool ssh_cmd_mkdir(char *out, size_t out_sz, const char *path) {
    return (size_t)snprintf(out, out_sz, CMD_MKDIR, path) < out_sz;
}

bool ssh_cmd_mv(char *out, size_t out_sz, const char *src, const char *dst) {
    return (size_t)snprintf(out, out_sz, CMD_MV, src, dst) < out_sz;
}

bool ssh_cmd_rm_rf(char *out, size_t out_sz, const char *path) {
    return (size_t)snprintf(out, out_sz, CMD_RM_RF, path) < out_sz;
}

bool ssh_cmd_restart_lcm(char *out, size_t out_sz) {
    return (size_t)snprintf(out, out_sz, CMD_RESTART_LCM) < out_sz;
}

bool build_apply_profile_command(
    char *out,
    size_t out_sz,
    const char *tmp_dir,
    const char *anim_file,
    const char *sound_file
) {
    if (!*out || !tmp_dir)
        return false;

    if (!ssh_arg_is_safe_single_quoted(tmp_dir)) {
        return false;
    }

    out[0] = '\0';
    size_t len = 0;

    cmd_append(out, out_sz, &len, "%s", SCRIPT_PREAMBLE);

    if (anim_file) {
        cmd_append(out, out_sz, &len,
            "run cleanup_anim rm -f \"$ANIM_DIR\"/*\n"
            "run move_anim mv -f '%s/%s' \"$ANIM_DIR/%s.anim\"\n"
            "run move_anim_md5 mv -f '%s/%s.md5' \"$ANIM_DIR/%s.md5\"\n"
            "run move_anim_conf mv -f '%s/ubnt_lcm_gui.conf.patched' \"$PERSIST_DIR/ubnt_lcm_gui.conf\"\n",
            tmp_dir, anim_file, anim_file,
            tmp_dir, anim_file, anim_file, tmp_dir
        );
    }

    if (sound_file) {
        cmd_append(out, out_sz, &len,
            "run cleanup_snd rm -f \"$SND_DIR\"/*\n"
            "run move_snd mv -f '%s/%s' \"$SND_DIR/%s\"\n"
            "run move_snd_md5 mv -f '%s/%s.md5' \"$SND_DIR/%s.md5\"\n"
            "run move_snd_conf mv -f '%s/ubnt_sounds_leds.conf.patched' \"$PERSIST_DIR/ubnt_sounds_leds.conf\"\n",
            tmp_dir, sound_file, sound_file,
            tmp_dir, sound_file, sound_file, tmp_dir
        );
    }

    cmd_append(out, out_sz, &len, "%s", SCRIPT_RESTART);

    return true;
}

bool ssh_parse_step_error(const char *stderr_text, ssh_step_error_t *out) {
    if (!stderr_text || !out) {
        return false;
    }

    memset(out, 0, sizeof(*out));

    out->rc = 0;

    const char *p = find_last_substr(stderr_text, "ERROR step=");
    if (!p) {
        return false;
    }

    char step[sizeof(out->step)];
    int rc;

    if (sscanf(p, "ERROR step=%63s rc=%d", step, &rc) != 2) {
        return false;
    }

    strcpy(out->step, step);
    out->rc = rc;
    out->has_error = true;

    return true;
}
