#pragma once

#include <stddef.h>

typedef void (*mqtt_handler_fn)(const char *payload, size_t len);

int mqtt_inbound_start(void);
void mqtt_inbound_stop(void);

int mqtt_inbound_enqueue(const char *topic, int topicLen, const char *payload, size_t len);

int mqtt_routes_add(const char *topic, mqtt_handler_fn fn);