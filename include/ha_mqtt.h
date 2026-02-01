#pragma once

#include "config_types.h"
#include <stdbool.h>

/**
 * @brief Bind Home Assistant MQTT functionality to the MQTT client.
 * 
 * @param cfg Pointer to the configuration structure.
 * @return
*/
bool ha_mqtt_bind(const config_t *cfg);