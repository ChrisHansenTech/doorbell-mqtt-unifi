#pragma once

#include "mqtt.h"
#include <stdbool.h>
#include <stddef.h>

#define MOCK_MQTT_MAX_PUBLISHES 128
#define MOCK_MQTT_MAX_SUBSCRIBES 32
#define MOCK_MQTT_TOPIC_LEN 256
#define MOCK_MQTT_PAYLOAD_LEN 2048

typedef struct {
    char topic[MOCK_MQTT_TOPIC_LEN];
    char payload[MOCK_MQTT_PAYLOAD_LEN];
    int qos;
    int retained;
} mock_mqtt_publish_t;

typedef struct {
    mock_mqtt_publish_t publishes[MOCK_MQTT_MAX_PUBLISHES];
    size_t publish_count;

    char subscribes[MOCK_MQTT_MAX_SUBSCRIBES][MOCK_MQTT_TOPIC_LEN];
    size_t subscribe_count;

    bool lwt_set;
    char lwt_topic[MOCK_MQTT_TOPIC_LEN];
    char lwt_payload[MOCK_MQTT_PAYLOAD_LEN];
    int lwt_qos;
    int lwt_retained;
    bool lwt_return_value;

    mqtt_on_connect_fn on_connect_fn;
    void *on_connect_user;
    mqtt_on_disconnect_fn on_disconnect_fn;
    void *on_disconnect_user;
} mock_mqtt_state_t;

extern mock_mqtt_state_t g_mock_mqtt;

void mock_mqtt_reset(void);
const mock_mqtt_publish_t *mock_mqtt_find_last_publish(const char *topic_suffix);
size_t mock_mqtt_count_publishes_for(const char *topic_suffix);
bool mock_mqtt_has_subscribe(const char *topic);
