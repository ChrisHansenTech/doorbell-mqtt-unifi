#include "utils.h"
#include "logger.h"

#include "md5.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <linux/limits.h>
#include <time.h>

#define MAX_ASSET_SIZE_BYTES 1048576

typedef enum {
    DETECTED_TYPE_NONE,
    DETECTED_TYPE_OGG,
    DETECTED_TYPE_WAV,
    DETECTED_TYPE_PNG
} detected_type_t;

static char *utils_file_type_to_string(detected_type_t type) {
    switch (type) {
        case DETECTED_TYPE_OGG:
            return "ogg";
        case DETECTED_TYPE_WAV:
            return "wav";
        case DETECTED_TYPE_PNG:
            return "png";
        default:
            return "unknown";
    }
}

static char *utils_file_class_to_string(utils_file_class_t cls) {
    switch (cls) {
        case UTILS_FILE_CLASS_ANIMATION:
            return "animation (png)";
        case UTILS_FILE_CLASS_SOUND:
            return "sound (ogg/wav)";
        default:
            return "unkown";
    }
}

static bool utils_has_ext_ci(const char *name, const char *ext) {
    if (!name || !ext) {
        return false;
    }

    const char *dot = strrchr(name, '.');
    if (!dot) {
        return false;
    }

    return strcasecmp(dot, ext) == 0;
}

static bool utils_has_any_ext_ci(const char *name, const char *const *exts, size_t n) {
    if (!name || !exts) {
        return false;
    }

    for (size_t i = 0; i < n; i++) {
        if (utils_has_ext_ci(name, exts[i])) {
            return true;
        }
    }

    return false;
}

static bool utils_is_safe_single_segment(const char *name) {
    if (!name || name[0] == '\0') {
        return false;
    }

    size_t len = strlen(name);
    if (len > 255) {
        return false;
    }

    // Reject hidden files like ".env" or ".something"
    if (name[0] == '.') {
        return false;
    }

    // Reject path traversal in name
    if (strstr(name, "..") || strchr(name, '/') || strchr(name, '\\')) {
        return false;
    }

    // Reject control chars / non-printables
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        if (!isprint(*p)) {
            return false;
        } 
    }

    // Reject trailing space/dot
    if (name[len - 1] == ' ' || name[len - 1] == '.') {
        return false;
    }

    return true;
}

static bool utils_ext_allowed_for_class(const char *name, utils_file_class_t cls) {
    static const char *sound_exts[] = { ".ogg", ".wav" };
    static const char *anim_exts[]  = { ".png" };

    switch (cls) {
        case UTILS_FILE_CLASS_SOUND:
            return utils_has_any_ext_ci(name, sound_exts, sizeof(sound_exts) / sizeof(sound_exts[0]));
        case UTILS_FILE_CLASS_ANIMATION:
            return utils_has_any_ext_ci(name, anim_exts, sizeof(anim_exts) / sizeof(anim_exts[0]));
        default:
            return false;
    }
}

static bool validate_ogg_magic(FILE *f) {
    char h[4];
    return fread(h, 1, 4, f) == 4 && memcmp(h, "OggS", 4) == 0;
}

static bool validate_wav_magic(FILE *f) {
    char h[12];
    return fread(h, 1, 12, f) == 12 &&
           memcmp(h, "RIFF", 4) == 0 &&
           memcmp(h + 8, "WAVE", 4) == 0;
}

static bool validate_png_magic(FILE *f) {
    unsigned char h[8];
    const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    return fread(h, 1, 8, f) == 8 && memcmp(h, sig, 8) == 0;
}

static detected_type_t detect_type_from_ext(const char *path) {
    if (utils_has_ext_ci(path, ".ogg")) {
        return DETECTED_TYPE_OGG;
    }

    if (utils_has_ext_ci(path, ".wav")) {
        return DETECTED_TYPE_WAV;
    }

    if (utils_has_ext_ci(path, ".png")) {
        return DETECTED_TYPE_PNG;
    }

    return DETECTED_TYPE_NONE;
}

static bool detected_type_allowed_for_class(detected_type_t t, utils_file_class_t cls) {
    switch (cls) {
        case UTILS_FILE_CLASS_SOUND:
            return (t == DETECTED_TYPE_OGG || t == DETECTED_TYPE_WAV);
        case UTILS_FILE_CLASS_ANIMATION:
            return (t == DETECTED_TYPE_PNG);
        default:
            return false;
    }
}

bool utils_file_exists(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return false;
    }

    struct stat st;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool utils_directory_exists(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return false;
    }

    struct stat st;

    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool utils_build_path(char *out, size_t out_len, const char *base, const char *child) {
    if (!out || !base || !child) {
        LOG_ERROR("Invalid parameters: out=%p out_len=%ld, base=%p, child=%p", (void*)out, out_len, (void*)base, (void*)child);
        return false;
    }
    
    size_t base_len = strlen(base);

    while (base_len > 0 && base[base_len - 1] == '/') {
        base_len--;
    }

    size_t required = base_len + 1 + strlen(child) + 1;

    if (required > out_len) {
        LOG_ERROR("utils_build_path: buffer too small (required=%zu, available=%zu)",
                  required, out_len);
        return false;
    }

    int written = snprintf(out, out_len, "%.*s/%s", (int)base_len, base, child);

    if (written < 0 || (size_t)written >= out_len) {
        LOG_ERROR("utils_build_path: snprintf failed or truncated");
        return false;
    }

    return true;
}

bool utils_read_file(const char *path, char **out, size_t *out_len) {
    if (!path || !out) {
        LOG_ERROR("utils_read_file: invalid parameters: path=%p out=%p, out_len=%p", (void*)path, (void*)out, (void*)out_len);
        return false;
    }

    FILE *file = fopen(path, "rb");

    if (!file) {
        LOG_ERROR("File '%s' does not exist or is not accessible.", path);
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        LOG_ERROR("Failed to seek to end of the file: %s", path);
        fclose(file);
        return false;
    }

    long length = ftell(file);

    if (length < 0) {
        LOG_ERROR("Failed to determine file size or file is empty: %s", path);
        fclose(file);
        return false;
    }

    rewind(file);

    char *buffer = malloc(length + 1);
    if (!buffer) {
        LOG_ERROR("Unable to allocate %ld bytes for file buffer %s", length, path);
        fclose(file);
        return false;
    }

    size_t bytes_read = fread(buffer, 1, length, file);
    
    if(bytes_read != (size_t)length) {
        LOG_ERROR("Short read: only %zu of %ld bytes read from file: %s", bytes_read, length, path);
        free(buffer);
        fclose(file);
        return false;
    }
    
    buffer[length] = '\0';
    *out = buffer;

    if (out_len) {
        *out_len = length;
    }
    
    
    fclose(file);
    return true;
}

bool utils_write_file(const char *path, const char *content) {
    if (!path || !content) {
        LOG_ERROR("utils_write_file: invalid parameters: path=%p content=%p", (void*)path, (void*)content);
        return false;
    }

    FILE *file = fopen(path, "w");

    if (!file) {
        LOG_ERROR("utils_write_file: failed to open '%s': %s", path, strerror(errno));
        return false;
    }

    const size_t content_len = strlen(content);

    const size_t bytes_written = fwrite(content, 1, content_len, file);

    if (bytes_written < content_len) {
        LOG_ERROR("Did not write entire file '%s'", path);
        fclose(file);
        return false;
    }

    if (fclose(file) != 0) {
        LOG_ERROR("Failed to close '%s': %s", path, strerror(errno));
        return false;
    }

    return true;

}

bool utils_create_directory(const char *path) {
    if (!path || *path == '\0') {
        LOG_ERROR("utils_create_directory: invalid path");
        return false;
    }

    if (mkdir(path, 0755) == 0) {
        LOG_INFO("Directory '%s' created successfully.", path);
        return true;
    }

    if (errno == EEXIST) {
        LOG_INFO("Directory '%s' already exists.", path);
        return true;
    }

    LOG_ERROR("Failed to create directory '%s': %s", path, strerror(errno));
    return false;
}

void utils_build_timestamp_dir(time_t *t, char *out, size_t out_size) {
    struct tm tm_now;

    gmtime_r(t, &tm_now);

    strftime(out, out_size, "%Y%m%d_%H%M%S", &tm_now);
}

void utils_build_iso_timestamp(time_t *t, char *out, size_t out_size) {
    struct tm tm_now;

    gmtime_r(t, &tm_now);

    strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &tm_now);
}

void to_human_readable(const char *input, char *output, size_t out_size) {
    size_t j = 0;
    int capitalize_next = 1;

    for (size_t i = 0; input[i] != '\0' && j < out_size - 1; i++) {
        char c = input[i];

        if (c == '_' || c == '-') {
            output[j++] = ' ';
            capitalize_next = 1;
        } else {
            if (capitalize_next) {
                output[j++] = toupper((unsigned char)c);
                capitalize_next = 0;
            } else {
                output[j++] = c;
            }
        }
    }

    output[j] = '\0';
}

bool utils_md5_file_hex(const char *path, char out_hex[33]) {
    if (!path || *path == '\0' || !out_hex) {
        return false;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        LOG_ERROR("File '%s' does not exist or is not accessible.", path);
        return false;
    }

    MD5_CTX ctx;
    md5_init(&ctx);

    uint8_t buffer[4096];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        md5_update(&ctx, buffer, bytes_read);
    }

    if (ferror(file)) {
        fclose(file);
        return false;
    }

    fclose(file);

    uint8_t digest[16];
    md5_final(&ctx, digest);

    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < 16; i++) {
        out_hex[i * 2]     = hex[(digest[i] >> 4) & 0x0F];
        out_hex[i * 2 + 1] = hex[digest[i] & 0x0F];
    }

    out_hex[32] = '\0';
    return true;

}

bool utils_delete_directory(const char *path) {
    if (!path || *path == '\0') {
        LOG_ERROR("Invalid path");
        return false;
    }

    DIR *dir = opendir(path);

    if (!dir) {
        LOG_WARN("Failed to open '%s': %s", path, strerror(errno));
        return false;
    }

    struct dirent *entry;
    char child_path[PATH_MAX];

    while ((entry = readdir(dir)) != NULL) {
        errno = 0;
        
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (!utils_build_path(child_path, sizeof(child_path), path, entry->d_name)) {
            LOG_ERROR("Failed to build path for '%s'", entry->d_name);
            closedir(dir);
            return false;
        }

        struct stat st;

        if (lstat(child_path, &st) != 0) {
            LOG_ERROR("Failed to stat '%s': %s", child_path, strerror(errno));
            closedir(dir);
            return false;
        }

        if (S_ISDIR(st.st_mode)) {
            if (!utils_delete_directory(child_path)) {
                closedir(dir);
                return false;
            }
        } else {
            if (unlink(child_path) != 0) {
                LOG_ERROR("Failed to remove file '%s': %s", child_path, strerror(errno));
                closedir(dir);
                return false;
            }
        }
    }

    if (errno != 0) {
        LOG_ERROR("Failed to read directory '%s': %s", path, strerror(errno));
        closedir(dir);
        return false;
    }

    closedir(dir);

    if (rmdir(path) != 0) {
        LOG_ERROR("Failed to remove directory '%s': %s", path, strerror(errno));
        return false;
    }

    return true;
}

bool utils_is_valid_directory_name(const char *name) {
    if (!name) {
        return false;
    }
    
    if (!utils_is_safe_single_segment(name)) {
        return false;
    } 

    return true;
}


bool utils_is_valid_filename(const char *name, utils_file_class_t cls) {
    if (!name) {
        return false;
    }
    
    if (!utils_is_safe_single_segment(name)) {
        return false;
    } 

    if (!utils_ext_allowed_for_class(name, cls)) {
        return false;
    }

    return true;
}

bool utils_is_valid_file(const char *full_path, utils_file_class_t cls) {
    if (!full_path || full_path[0] == '\0') {
        return false;
    }

    // Must exist, be a regular file, and non-empty
    struct stat st;
    if (stat(full_path, &st) != 0) {
        LOG_WARN("stat failed for '%s': %s (errno=%d)", full_path, strerror(errno), errno);
        return false;
    }

    if (!S_ISREG(st.st_mode)) {
        LOG_ERROR("Path '%s' exists but is not a regular file (mode=0%o)", full_path, st.st_mode);
        return false;
    }

    if (st.st_size <= 0) {
        LOG_ERROR("'%s' is an empty file", full_path);
        return false;
    }

    if (st.st_size > MAX_ASSET_SIZE_BYTES) {
    LOG_ERROR("File '%s' exceeds max allowed size (%zu bytes). Actual size=%lld bytes.",
        full_path,
        (size_t)MAX_ASSET_SIZE_BYTES,
        (long long)st.st_size);
    return false;
}

    // Detect by extension, enforce it matches the caller's class
    detected_type_t t = detect_type_from_ext(full_path);
    if (t == DETECTED_TYPE_NONE) {
        LOG_ERROR("File '%s' does not have a valid asset extension", full_path);
        return false;
    }

    if (!detected_type_allowed_for_class(t, cls)) {
        LOG_ERROR("File '%s' is not a valid %s file (detected type=%s)", 
            full_path, 
            utils_file_type_to_string(t), 
            utils_file_class_to_string(cls));
        return false;
    }

    // Validate magic header
    FILE *f = fopen(full_path, "rb");
    if (!f) {
        LOG_ERROR("Failed to open file '%s' : %s (errno=%d)", full_path, strerror(errno), errno);
        return false;
    }

    bool ok = false;
    
    switch (t) {
        case DETECTED_TYPE_OGG: 
            ok = validate_ogg_magic(f); 
            break;
        case DETECTED_TYPE_WAV: 
            ok = validate_wav_magic(f); 
            break;
        case DETECTED_TYPE_PNG: 
            ok = validate_png_magic(f); 
            break;
        default: 
            ok = false; 
            break;
    }

    if (ok == false) {
        LOG_ERROR("File '%s' does not contain a valid %s header.", full_path, utils_file_type_to_string(t));
    }

    fclose(f);
    return ok;
}

bool utils_copy_file_contents(const char *src, const char *dst) {
    int in_fd = -1, out_fd = -1;
    bool ok = false;

    in_fd = open(src, O_RDONLY);
    if (in_fd < 0) return false;

    out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) goto cleanup;

    char buf[64 * 1024];
    for (;;) {
        ssize_t r = read(in_fd, buf, sizeof(buf));
        if (r == 0) break;                 // EOF
        if (r < 0) goto cleanup;

        ssize_t off = 0;
        while (off < r) {
            ssize_t w = write(out_fd, buf + off, (size_t)(r - off));
            if (w < 0) goto cleanup;
            off += w;
        }
    }

    (void)fsync(out_fd);

    ok = true;

cleanup:
    if (out_fd >= 0) close(out_fd);
    if (in_fd >= 0) close(in_fd);
    return ok;
}

bool utils_move_file_cross_fs(const char *old_path, const char *new_path) {
    if (rename(old_path, new_path) == 0) return true;

    if (errno != EXDEV) {
        return false;
    }

    if (!utils_copy_file_contents(old_path, new_path)) return false;

    if (unlink(old_path) != 0) {
        return false;
    }

    return true;
}
