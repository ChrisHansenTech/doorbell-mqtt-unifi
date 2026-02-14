#pragma once

#include "config_types.h"

#include <stdbool.h>

/**
 * @brief Initializes Home Assistant MQTT topics based on the provided configuration.
 * 
 * @param cfg 
 */
void ha_topics_init(const config_t *cfg);

/**
 * @brief Returns the base MQTT topic for this instance, constructed from the configured prefix and instance name.
 * 
 * @return const char* 
 */
const char *ha_get_base_topic(void);

/**
 * @brief Builds a full MQTT topic by appending the provided subtopic to the base topic. 
 *        The result is written to the provided output buffer.
 * 
 * @param out 
 * @param out_size 
 * @param subtopic 
 */
void ha_build_topic(char *out, size_t out_size, const char *subtopic);

/**
 * @brief Returns the MQTT topic used for reporting availability status to Home Assistant.
 * 
 * @return const char* 
 */
const char *ha_availability_topic(void);

/**
 * @brief 
 * 
 * @return const char* 
 */
const char *ha_status_topic(void);

/**
 * @brief Subscribes to all command topics for entities defined in ha_topics.h. 
 *        This should be called after connecting to MQTT to ensure command handling 
 *        is active.
 * 
 */
void ha_topic_subscribe_commands(void);

/**
 * @brief Registers command handlers for all entities defined in ha_topics.h. 
 *        This should be called during initialization to ensure that incoming
 *        commands are properly routed to their handlers.
 * 
 */
void  ha_routes_register_commands(void);
