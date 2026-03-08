#pragma once

#include "state_types.h"

/** @brief Compares the given profile state with the currently active state on the device to determine if they match.
 *  @param active_state The profile state to compare against the active state on the device.
 *  @return 0 if the states match, non-zero if they do not match or if an error occurs during comparison.
 */
int state_save(profile_state_t *profile_state, const char *profile_name, bool is_preset, unifi_apply_method_t apply_method, const char *state_dir);

/** @brief Free the memory allocated for an applied state.
 *  @param state The applied state to free.
 */
void state_applied_free(applied_state_t *state);

/** @brief Free the memory allocated for a profile state.
 *  @param state The profile state to free.
 */
void state_free_profile_state(profile_state_t *state);