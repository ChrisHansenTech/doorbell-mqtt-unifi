#pragma once
#include <stddef.h>

/**
 * @brief Handles the MQTT set command.
 * 
 * @param json JSON payload for the command.
 * @param len Length of the JSON payload.
 */
void command_handle_set(const char *json, size_t len);
void handle_set(const char *payload, size_t len);