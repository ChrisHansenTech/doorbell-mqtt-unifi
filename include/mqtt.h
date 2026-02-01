#pragma once

#include "config_types.h"

#include <stdbool.h>
#include <stddef.h>

typedef void (*mqtt_on_connect_fn)(bool reconnect, void *user);
typedef void (*mqtt_on_disconnect_fn)(void *user);

void mqtt_set_on_connect(mqtt_on_connect_fn fn, void *user);
void mqtt_set_on_disconnect(mqtt_on_disconnect_fn fn, void *user);
bool mqtt_set_last_will(const char *topic, const char *payload, int qos, int retained);

bool mqtt_init(const config_mqtt_t *cfg);
void mqtt_subscribe(const char *topic);
void mqtt_publish(const char *topic, const char *payload, int qos, int retained);
void mqtt_loop(int timeout_ms);
void mqtt_disconnect(void);
