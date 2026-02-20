#pragma  once

#include "ssh.h"
#include "unifi_profile.h"
#include <stdbool.h>

/**
 * @brief Downloads the current configuration from the device, including the ubnt_lcm_gui.conf 
 *        and ubnt_sounds_leds.conf files, and loads them into a unifi_profile_t structure. 
 * 
 * @param session 
 * @param tmp_dir 
 * @param out 
 * @return true 
 * @return false 
 */
bool unifi_conf_download_and_load(ssh_session_t *session, const char *tmp_dir, unifi_profile_t *out);

/**
 * @brief Downloads the current configuration from the device including the assets.
 * 
 * @param session 
 * @param tmp_dir 
 * @param out 
 * @return true 
 * @return false 
 */
bool unifi_profile_download_and_load(ssh_session_t *session, const char *tmp_dir, unifi_profile_t *out);

/**
 * @brief Uploads the given profile to the device and applies it. This includes uploading any custom animation or sound files,
 * 
 * @param session 
 * @param profile_dir 
 * @param profile 
 * @return int 
 */
int unifi_profile_upload_and_apply(ssh_session_t *session, const char *profile_dir, const unifi_profile_t *profile);
