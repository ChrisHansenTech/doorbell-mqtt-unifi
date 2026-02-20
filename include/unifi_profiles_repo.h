#pragma once

#include "config_types.h"
#include "unifi_profile.h"
#include <linux/limits.h>
#include <stdbool.h>


/**
 * @brief Initialize the profiles module with the given base directory and configuration.
 * 
 * @param base_dir 
 * @param cfg 
 * @return true 
 * @return false 
 */
bool profiles_repo_init(const char *base_dir, const config_preset_t *cfg);

/**
 * @brief Resolve the given preset name to an profile directory.
 * 
 * @param preset_name 
 * @param out_dir 
 * @param out_len 
 * @return true 
 * @return false 
 */
bool profiles_repo_resolve_preset(const char *preset_name, char *out_dir, size_t out_len);

/**
 * @brief Resolve a custom profile directory.
 * 
 * @param custom_directory 
 * @param out_dir 
 * @param out_len 
 * @return true 
 * @return false 
 */
bool profiles_repo_resolve_custom(const char *custom_directory, char *out_dir, size_t out_len);

/**
 * @brief Create a temporary directory for profile downloads and return the path.
 * 
 * @param out_dir 
 * @param out_len 
 * @return true 
 * @return false 
 */
bool profiles_repo_create_temp_profile_dir(char *out_dir, size_t out_len);

/**
 * @brief Rename the temporary profile directory to a final location. If partial is true, it will be moved to a "partial" 
 *        subdirectory to indicate an incomplete download. Otherwise, it will be moved to a "downloads" subdirectory. 
 *        The final path will be returned in out.
 * 
 * @param temp_dir
 * @param time 
 * @param partial 
 * @param out_dir
 * @param out_dir_len
 * @param out_path
 * @param out_path_len
 * @return true 
 * @return false 
 */
bool profiles_repo_rename_temp_profile_dir(const char *temp_dir, time_t *time, bool partial, char *out_dir, size_t out_dir_len, char *out_path, size_t out_path_len);

/**
 * @brief Write the last applied profile information to storage. This can be used to track which profile was last applied,
 *        when, and whether it was a preset or custom profile.
 * 
 * @param profile 
 * @param is_preset 
 * @return true 
 * @return false 
 */
bool profiles_write_last_applied(const char *name, bool is_preset);

/**
 * @brief Load the last applied profile information from storage.
 * 
 * @param out 
 * @return true 
 * @return false 
 */
bool profile_load_last_applied(unifi_last_applied_profile_t *out);

/**
 * @brief Shutdown the profiles module and release global resources.
 * 
 * Clears internal global state and frees the stored base directory. * 
 */
void profiles_repo_shutdown(void);
