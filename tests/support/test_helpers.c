#define _POSIX_C_SOURCE 200809L

#include "test_helpers.h"
#include "utils.h"

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool test_make_temp_dir(char *out_path, size_t out_size) {
    if (!out_path || out_size < 32) {
        return false;
    }

    char tmpl[] = "/tmp/dmu-test-XXXXXX";
    char *dir = mkdtemp(tmpl);
    if (!dir) {
        return false;
    }

    int written = snprintf(out_path, out_size, "%s", dir);
    return written > 0 && (size_t)written < out_size;
}

bool test_remove_dir_recursive(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }
    return utils_delete_directory(path);
}

bool test_write_bytes(const char *path, const unsigned char *data, size_t len) {
    if (!path || (!data && len > 0)) {
        return false;
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        return false;
    }

    size_t written = len > 0 ? fwrite(data, 1, len, fp) : 0;
    int close_rc = fclose(fp);
    return written == len && close_rc == 0;
}

bool test_write_text(const char *path, const char *text) {
    if (!path || !text) {
        return false;
    }
    return test_write_bytes(path, (const unsigned char *)text, strlen(text));
}

bool test_write_minimal_png(const char *path) {
    static const unsigned char png_bytes[] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D, 'I', 'H', 'D', 'R'
    };
    return test_write_bytes(path, png_bytes, sizeof(png_bytes));
}

bool test_write_minimal_ogg(const char *path) {
    static const unsigned char ogg_bytes[] = {
        'O', 'g', 'g', 'S', 0x00, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    return test_write_bytes(path, ogg_bytes, sizeof(ogg_bytes));
}

bool test_write_minimal_wav(const char *path) {
    static const unsigned char wav_bytes[] = {
        'R', 'I', 'F', 'F', 0x24, 0x00, 0x00, 0x00,
        'W', 'A', 'V', 'E', 'f', 'm', 't', ' '
    };
    return test_write_bytes(path, wav_bytes, sizeof(wav_bytes));
}

bool test_create_valid_profile_dir(const char *dir_path, const char *anim_file, const char *sound_file, bool welcome_enabled, bool ring_enabled) {
    if (!dir_path || !anim_file || !sound_file) {
        return false;
    }

    if (!utils_create_directory(dir_path)) {
        return false;
    }

    char path[PATH_MAX];
    char json[1024];
    snprintf(
        json,
        sizeof(json),
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"welcome\": {\n"
        "    \"enabled\": %s,\n"
        "    \"file\": \"%s\",\n"
        "    \"count\": 10,\n"
        "    \"durationMs\": 1000,\n"
        "    \"loop\": true,\n"
        "    \"guiId\": \"WELCOME\"\n"
        "  },\n"
        "  \"ringButton\": {\n"
        "    \"enabled\": %s,\n"
        "    \"file\": \"%s\",\n"
        "    \"repeatTimes\": 1,\n"
        "    \"volume\": 85,\n"
        "    \"soundStateName\": \"RING_BUTTON_PRESSED\"\n"
        "  }\n"
        "}\n",
        welcome_enabled ? "true" : "false",
        anim_file,
        ring_enabled ? "true" : "false",
        sound_file
    );

    if (!utils_build_path(path, sizeof(path), dir_path, "profile.json")) {
        return false;
    }
    if (!test_write_text(path, json)) {
        return false;
    }

    if (welcome_enabled && anim_file[0] != '\0') {
        if (!utils_build_path(path, sizeof(path), dir_path, anim_file)) {
            return false;
        }
        if (!test_write_minimal_png(path)) {
            return false;
        }
    }

    if (ring_enabled && sound_file[0] != '\0') {
        if (!utils_build_path(path, sizeof(path), dir_path, sound_file)) {
            return false;
        }
        const char *dot = strrchr(sound_file, '.');
        if (dot && strcmp(dot, ".wav") == 0) {
            if (!test_write_minimal_wav(path)) {
                return false;
            }
        } else {
            if (!test_write_minimal_ogg(path)) {
                return false;
            }
        }
    }

    return true;
}
