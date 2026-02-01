#include "utils.h"
#include "logger.h"

#include "md5.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <linux/limits.h>
#include <time.h>

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

void utils_build_timestamp(char *out, size_t out_size) {
    time_t now = time(NULL);

    struct tm tm_now;

    localtime_r(&now, &tm_now);

    strftime(out, out_size, "%Y%m%d-%H%M%S", &tm_now);
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
        LOG_ERROR("Failed to open '%s': %s", path, strerror(errno));
        return false;
    }

    struct dirent *entry;
    char child_path[PATH_MAX];

    while ((entry = readdir(dir)) != NULL) {
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

        errno = 0;
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
