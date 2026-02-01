#pragma once

#include "unifi_profile.h"
#include <stdbool.h>

/**
 * @brief Read profile data from LCM GUI configuration.
 * 
 * @param path 
 * @param out 
 * @return true 
 * @return false 
 */
bool unifi_profile_read_from_lcm_gui_conf(const char *path, unifi_profile_t *out);

/**
 * @brief Read profile data from Sounds & LEDs configuration.
 * 
 * @param path 
 * @param out 
 * @return true 
 * @return false 
 */
bool unifi_profile_read_from_sounds_leds_conf(const char *path, unifi_profile_t *out);

/**
 * @brief Patch LCM GUI configuration with desired profile data.
 * 
 * @param in_path 
 * @param out_path 
 * @param desired 
 * @return true 
 * @return false 
 */
bool unifi_profile_patch_lcm_gui_conf(const char *in_path, const char *out_path, const unifi_profile_t *desired);

/**
 * @brief Patch Sounds & LEDs configuration with desired profile data.
 * 
 * @param in_path 
 * @param out_path 
 * @param desired 
 * @return true 
 * @return false 
 */
bool unifi_profile_patch_sounds_leds_conf(const char *in_path, const char *out_path, const unifi_profile_t *desired);