#pragma once

#include <stdbool.h>
#include <stddef.h>

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
 * @brief Build a timestamp string in the format "YYYYMMDD-HHMMSS".
 * 
 * @param out output buffer
 * @param out_size size of the output buffer
 */
void utils_build_timestamp(char *out, size_t out_size);


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

