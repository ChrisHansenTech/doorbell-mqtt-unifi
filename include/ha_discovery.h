#pragma once

#include "config_types.h"
#include <stdbool.h>

/**
 * @brief Publish Home Assistant MQTT Discovery configuration
 * 
 * @param cfg 
 * @return true 
 * @return false 
 */
bool ha_publish_discovery(const config_t *cfg);