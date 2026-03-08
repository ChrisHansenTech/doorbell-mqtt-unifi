#pragma once

#include "state_types.h"
#include "unifi_profile.h"

typedef struct {
    char *lcm_gui_json;
    char *sounds_leds_json;
} unifi_ipc_raw_t;

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

/**
 * @brief Parses the raw IPC response from the device and extracts the applied state into a structured format.
 * 
 * @param ipc_state 
 * @param out_state 
 * @return true if parsing was successful and out_state is valid, false otherwise
 */
bool unifi_ipc_parse_apply_response(unifi_ipc_raw_t *ipc_state, profile_state_t *out_state);

/**
 * @brief Parses the raw IPC response from the device and extracts the validation state into a structured format.
 * 
 * @param ipc_state 
 * @param out_state 
 * @return true if parsing was successful and out_state is valid, false otherwise
 */
bool unifi_ipc_parse_validate_response(unifi_ipc_raw_t *ipc_state, profile_state_t *out_state);

/**
 * @brief Frees any dynamically allocated memory within the given unifi_ipc_raw_t structure.
 * 
 * @param ipc_state 
 */
void unifi_ipc_raw_free(unifi_ipc_raw_t *ipc_state);