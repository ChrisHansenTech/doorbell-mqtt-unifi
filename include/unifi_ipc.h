#pragma once

#include "unifi_profile.h"


/**
 * @brief Build an IPC message for updating the LCM GUI configuration based on the given profile.
 * 
 * @param out_path 
 * @param profile 
 * @return bool 
 */
bool unifi_ipc_build_lcm_gui_message(const char *out_path, const unifi_profile_t *profile);

/**
 * @brief 
 * 
 * @param out_path 
 * @param profile 
 * @return bool 
 */
bool unifi_ipc_build_sounds_leds_message(const char *out_path,const unifi_profile_t *profile);