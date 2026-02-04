#pragma once
#include "mqtt_router_types.h"
#include <stddef.h>

/**
 * @brief Handles the MQTT set command.
 * 
 * @param json JSON payload for the command.
 * @param len Length of the JSON payload.
 */
void command_handle_set(const char *json, size_t len);

/**
 * @brief Handles the set command payload.
 * 
 * @param payload 
 * @param len 
 */
void handle_set(const char *payload, size_t len);

/**
 * @brief Sets a preset based on the provided payload.
 * 
 * @param ctx 
 * @param payload 
 * @param payloadLen 
 */
void command_set_preset(const mqtt_router_ctx_t *ctx, const char *payload, size_t payloadLen);

/**
 * @brief Applies custom settings based on the provided payload.
 * 
 * @param ctx 
 * @param payload 
 * @param payloadLen 
 */
void command_apply_custom(const mqtt_router_ctx_t *ctx, const char *payload, size_t payloadLen);


/**
 * @brief Applies the test profile.
 * 
 * @param ctx 
 * @param payload 
 * @param payloadLen 
 */
void command_test_config(const mqtt_router_ctx_t *ctx, const char *payload, size_t payloadLen);

/**
 * @brief Downloads assets based
 * 
 * @param ctx 
 * @param payload 
 * @param payloadLen 
 */
void command_download_assets(const mqtt_router_ctx_t *ctx, const char *payload, size_t payloadLen);
