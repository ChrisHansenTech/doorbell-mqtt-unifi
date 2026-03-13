#pragma  once

#include "ssh.h"
#include "unifi_ipc.h"
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

/**
 * @brief Uploads the given profile to the device and applies it using the specified method. This includes uploading any custom animation or sound files.
 * 
 * @param session 
 * @param profile_dir 
 * @param profile 
 * @param method 
 * @param applied_state 
 * @return int 
 */
int unifi_profile_upload_and_apply_ex(ssh_session_t *session, const char *profile_dir, const unifi_profile_t *profile, unifi_apply_method_t method, unifi_ipc_raw_t *applied_state);

/**
 * @brief Uploads the given SFX file to the device and plays it at the specified volume.
 * 
 * @param session 
 * @param sfx_file_path 
 * @param sfx_name 
 * @param volume 
 * @return int 
 */
int unifi_sfx_upload_and_play(ssh_session_t *session, const config_sfx_preset_item_t *sfx, const char *sounds_dir);

/**
 * @brief Fetches the current state from the device using IPC and returns it as a raw JSON string.
 * 
 * @param session 
 * @param out_state 
 * @return int 
 */
int unifi_fetch_state(ssh_session_t *session, unifi_ipc_raw_t *out_state);
