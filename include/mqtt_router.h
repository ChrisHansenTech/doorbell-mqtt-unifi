#pragma once

#include "mqtt_router_types.h"

#include <stdbool.h>
#include <stddef.h>

bool mqtt_router_start(const mqtt_router_ctx_t *ctx);

void mqtt_router_stop(void);

int mqtt_router_enqueue(const char *topic, int topicLen, const char *payload, size_t len);

int mqtt_routes_add(const char *topic, mqtt_handler_fn fn);