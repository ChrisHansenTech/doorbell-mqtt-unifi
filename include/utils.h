#pragma once

#include "errors.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

typedef enum {
    UTILS_FILE_CLASS_SOUND,
    UTILS_FILE_CLASS_ANIMATION
} utils_file_class_t;

/**
 * @brief Check if a file exists at the given path.
 * 
 * @param path file path
 * @return true if the file exists
 * @return false if the file does not exist
 */
bool utils_file_exists(const char *path);

/**
 * @brief Check if a directory exists at the given path.
 * 
 * @param path directory path
 * @return true if the directory exists
 * @return false if the directory does not exist
 */
bool utils_directory_exists(const char *path);

/**
 * @brief Build a file path by combining a base path and a child path.
 * 
 * @param out output buffer
 * @param out_len size of the output buffer
 * @param base base path
 * @param child child path
 * @return true on success
 * @return false on failure
 */
bool utils_build_path(char *out, size_t out_len, const char *base, const char *child);

/**
 * @brief Read the contest of a file into a buffer.
 * 
 * @param path source file path
 * @param out_content content buffer
 * @param out_size content buffer size
 * @return true 
 * @return false 
 */
bool utils_read_file(const char *path, char **out_content, size_t *out_size);

/**
 * @brief Write content to a file at the given path.
 * 
 * @param path file path
 * @param content content to write
 * @return true on success
 * @return false on failure
 */
bool utils_write_file(const char *path, const char *content);


/**
 * @brief Create a directory at the given path, including any necessary parent directories.
 * 
 * @param path directory path
 * @return true on success
 * @return false on failure
 */
bool utils_create_directory(const char *path);

/**
 * @brief Build a timestamp directory name in the format "YYYYMMDD_HHMMSS" for the current time or a given time.
 * 
 * @param t 
 * @param out 
 * @param out_size 
 */
void utils_build_timestamp_dir(time_t *t, char *out, size_t out_size);

/**
 * @brief Build a timestamp string in ISO 8601 format (e.g. "2024-06-01T12:34:56Z") for the current time or a given time.
 * 
 * @param t 
 * @param out 
 * @param out_size 
 */
void utils_build_iso_timestamp(time_t *t, char *out, size_t out_size);

/**
 * @brief Convert a string to human readable format.
 * For example, "front_door" becomes "Front Door".
 * 
 * @param input source string 
 * @param output buffer to write human readable string
 * @param out_size size of output buffer
 */
void to_human_readable(const char *input, char *output, size_t out_size);

/**
 * @brief Calculate the MD5 hash of a file and return it as a hex string.
 * 
 * @param path file path
 * @param out_hex output buffer for hex string (must be at least 33 bytes)
 * @return true on success
 * @return false on failure
 */
bool utils_md5_file_hex(const char *path, char out_hex[33]);

/**
 * @brief Delete a directory and all files/subdirectories inside it.
 * 
 * @param path directory path
 * @return true on success
 * @return false on failure
 */
bool utils_delete_directory(const char *path);

/**
 * @brief Check if a directory name is valid (single segment, no path traversal, reasonable length).
 * 
 * @param name directory name to check
 * @return true if the directory name is valid
 * @return false if the directory name is not valid
 */
bool utils_is_valid_directory_name(const char *name);

/**
 * @brief Check if a filename is valid for the given file class.
 *
 * @param name filename to check
 * @param cls file class to check against
 * @return error_t ERROR_NONE if the filename is valid for the file class
 * @return error_t specific error code if invalid
 */
error_return_t utils_is_valid_filename(const char *name, utils_file_class_t cls);

/**
 * @brief Check if the file is valid for the give file class.
 * 
 * @param full_path full path to the file
 * @param cls file class to check against
 * @return error_t ERROR_NONE if the filename is valid for the file class
 * @return error_t specific error code and message if invalid
 */
error_return_t utils_is_valid_file(const char *full_path, utils_file_class_t cls);

/**
 * @brief Copy the contents of one file to another.
 * 
 * @param src source file path
 * @param dst destination file path
 * @return true on success
 * @return false on failure
 */
bool utils_copy_file_contents(const char *src, const char *dst);

/**
 * @brief Move a file from one path to another, even across different filesystems.
 * 
 * @param old_path original file path
 * @param new_path new file path
 * @return true on success
 * @return false on failure
 */
bool utils_move_file_cross_fs(const char *old_path, const char *new_path);