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
 * @brief   Rename the given temporary directory to a final location in the profiles/downloads directory. 
 *          If partial is true, the final location is profiles/partial directory.
 * 
 * @param temp_dir 
 * @param partial 
 * @return true 
 * @return false 
 */
bool profiles_repo_rename_temp_profile_dir(const char *temp_dir, bool partial);


/**
 * @brief Shutdown the profiles module and release global resources.
 * 
 * Clears internal global state and frees the stored base directory. * 
 */
void profiles_repo_shutdown(void);
