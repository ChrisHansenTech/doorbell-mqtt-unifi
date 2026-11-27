#pragma once

#include <stdbool.h>
#include <stddef.h>

/**
 * Create a unique temporary directory under /tmp (e.g. /tmp/dmu-test-XXXXXX).
 * Caller should clean it up with test_remove_dir_recursive().
 */
bool test_make_temp_dir(char *out_path, size_t out_size);

/**
 * Recursively remove a directory created by tests.
 */
bool test_remove_dir_recursive(const char *path);

/**
 * Write raw binary bytes to a file path.
 */
bool test_write_bytes(const char *path, const unsigned char *data, size_t len);

/**
 * Write text string to a file path.
 */
bool test_write_text(const char *path, const char *text);

/**
 * Write minimal valid asset files (magic header + small body) for testing.
 */
bool test_write_minimal_png(const char *path);
bool test_write_minimal_ogg(const char *path);
bool test_write_minimal_wav(const char *path);

/**
 * Create a complete valid profile directory (profile.json + anim.png + sound.ogg).
 */
bool test_create_valid_profile_dir(const char *dir_path, const char *anim_file, const char *sound_file, bool welcome_enabled, bool ring_enabled);
