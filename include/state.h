#pragma once

#include "state_types.h"

/** @brief Loads the last applied state from disk.
 *  @param state Output parameter to receive the loaded state. The caller is responsible for freeing any dynamically 
 *               allocated memory within this structure using state_applied_free().
 *  @param state_dir Directory where the state file is located.
 *  @return 0 on success, non-zero on failure.
 */
int state_load(applied_state_t *state, const char *state_dir);

/** @brief Compares the given profile state with the currently active state on the device to determine if they match.
 *  @param active_state The profile state to compare against the active state on the device.
 *  @return 0 if the states match, non-zero if they do not match or if an error occurs during comparison.
 */
int state_save(profile_state_t *profile_state, const char *profile_name, bool is_preset, unifi_apply_method_t apply_method, const char *state_dir);

/** @brief Compares the given active state with the last applied state to determine if they match.
 *  @param active_state The profile state currently active on the device.
 *  @param applied_state The last applied state loaded from disk.
 *  @param active_hash Output parameter to receive the computed hash of the active state.
 *  @return 0 if the states match, non-zero if they do not match or if an error occurs during comparison.
 */
int state_compare(profile_state_t *active_state, applied_state_t *applied_state, char active_hash[65], const char *state_dir);

/** @brief Free the memory allocated for an applied state.
 *  @param state The applied state to free.
 */
void state_applied_free(applied_state_t *state);

/** @brief Free the memory allocated for a profile state.
 *  @param state The profile state to free.
 */
void state_free_profile_state(profile_state_t *state);