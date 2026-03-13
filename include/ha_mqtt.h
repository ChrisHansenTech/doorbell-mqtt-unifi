#pragma once

#include "config_types.h"
#include <stdbool.h>

/**
 * @brief Bind the MQTT client to Home Assistant by setting up the necessary callbacks and last will message.
 * 
 * @param cfg Pointer to the configuration structure containing MQTT settings.
 * @param state_dir Directory where the state file is located, which will be used in the on_connect callback to load the last applied state.
 * @return true if binding was successful, false otherwise.
 */
bool ha_mqtt_bind(const config_t *cfg, const char *state_dir);