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
bool profiles_repo_init(const char *base_dir, const config_holiday_t *cfg);

/**
 * @brief Resolve the given holiday name to an asset directory.
 * 
 * @param holiday_name 
 * @param out_dir 
 * @param out_len 
 * @return true 
 * @return false 
 */
bool profiles_repo_resolve_holiday(const char *holiday_name, char *out_dir, size_t out_len);

/**
 * @brief Resolve a custom asset directory.
 * 
 * @param custom_directory 
 * @param out_dir 
 * @param out_len 
 * @return true 
 * @return false 
 */
bool profiles_repo_resolve_custom(const char *custom_directory, char *out_dir, size_t out_len);


/**
 * @brief Shutdown the assets module and release global resources.
 * 
 * Clears internal global state and frees the stored base directory. * 
 */
void profiles_repo_shutdown(void);
