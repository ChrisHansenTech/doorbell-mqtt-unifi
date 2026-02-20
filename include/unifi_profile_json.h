#pragma once

#include "unifi_profile.h"

#include <stdbool.h>

/**
 * @brief Load a unifi_profile_t from a JSON file.
 * 
 * @param path 
 * @param p 
 * @return true 
 * @return false 
 */
bool unifi_profile_load_from_file(const char *path, unifi_profile_t *p);

/**
 * @brief Write a unifi_profile_t to a JSON file.
 * 
 * @param path 
 * @param p 
 * @return true 
 * @return false 
 */
bool unifi_profile_write_to_file(const char *path, const unifi_profile_t *p);

