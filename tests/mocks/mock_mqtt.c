#include "mock_mqtt.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

mock_mqtt_state_t g_mock_mqtt;
static pthread_mutex_t g_mock_mqtt_mtx = PTHREAD_MUTEX_INITIALIZER;

void mock_mqtt_reset(void) {
    pthread_mutex_lock(&g_mock_mqtt_mtx);
    memset(&g_mock_mqtt, 0, sizeof(g_mock_mqtt));
    g_mock_mqtt.lwt_return_value = true;
    pthread_mutex_unlock(&g_mock_mqtt_mtx);
}

static bool str_ends_with(const char *str, const char *suffix) {
    size_t slen = strlen(str);
    size_t suflen = strlen(suffix);
    if (suflen > slen) {
        return false;
    }
    return strcmp(str + (slen - suflen), suffix) == 0;
}

const mock_mqtt_publish_t *mock_mqtt_find_last_publish(const char *topic_suffix) {
    pthread_mutex_lock(&g_mock_mqtt_mtx);
    const mock_mqtt_publish_t *found = NULL;
    for (size_t i = 0; i < g_mock_mqtt.publish_count; i++) {
        if (str_ends_with(g_mock_mqtt.publishes[i].topic, topic_suffix)) {
            found = &g_mock_mqtt.publishes[i];
        }
    }
    pthread_mutex_unlock(&g_mock_mqtt_mtx);
    return found;
}

size_t mock_mqtt_count_publishes_for(const char *topic_suffix) {
    pthread_mutex_lock(&g_mock_mqtt_mtx);
    size_t count = 0;
    for (size_t i = 0; i < g_mock_mqtt.publish_count; i++) {
        if (str_ends_with(g_mock_mqtt.publishes[i].topic, topic_suffix)) {
            count++;
        }
    }
    pthread_mutex_unlock(&g_mock_mqtt_mtx);
    return count;
}

bool mock_mqtt_has_subscribe(const char *topic) {
    pthread_mutex_lock(&g_mock_mqtt_mtx);
    bool found = false;
    for (size_t i = 0; i < g_mock_mqtt.subscribe_count; i++) {
        if (strcmp(g_mock_mqtt.subscribes[i], topic) == 0) {
            found = true;
            break;
        }
    }
    pthread_mutex_unlock(&g_mock_mqtt_mtx);
    return found;
}

void __wrap_mqtt_publish(const char *topic, const char *payload, int qos, int retained) {
    pthread_mutex_lock(&g_mock_mqtt_mtx);
    if (g_mock_mqtt.publish_count < MOCK_MQTT_MAX_PUBLISHES) {
        mock_mqtt_publish_t *entry = &g_mock_mqtt.publishes[g_mock_mqtt.publish_count++];
        snprintf(entry->topic, sizeof(entry->topic), "%s", topic ? topic : "");
        snprintf(entry->payload, sizeof(entry->payload), "%s", payload ? payload : "");
        entry->qos = qos;
        entry->retained = retained;
    }
    pthread_mutex_unlock(&g_mock_mqtt_mtx);
}

void __wrap_mqtt_subscribe(const char *topic) {
    pthread_mutex_lock(&g_mock_mqtt_mtx);
    if (g_mock_mqtt.subscribe_count < MOCK_MQTT_MAX_SUBSCRIBES) {
        snprintf(g_mock_mqtt.subscribes[g_mock_mqtt.subscribe_count++], MOCK_MQTT_TOPIC_LEN, "%s", topic ? topic : "");
    }
    pthread_mutex_unlock(&g_mock_mqtt_mtx);
}

bool __wrap_mqtt_set_last_will(const char *topic, const char *payload, int qos, int retained) {
    pthread_mutex_lock(&g_mock_mqtt_mtx);
    bool rc = g_mock_mqtt.lwt_return_value;
    if (topic && payload && rc) {
        g_mock_mqtt.lwt_set = true;
        snprintf(g_mock_mqtt.lwt_topic, sizeof(g_mock_mqtt.lwt_topic), "%s", topic);
        snprintf(g_mock_mqtt.lwt_payload, sizeof(g_mock_mqtt.lwt_payload), "%s", payload);
        g_mock_mqtt.lwt_qos = qos;
        g_mock_mqtt.lwt_retained = retained;
    }
    pthread_mutex_unlock(&g_mock_mqtt_mtx);
    return rc;
}

void __wrap_mqtt_set_on_connect(mqtt_on_connect_fn fn, void *user) {
    pthread_mutex_lock(&g_mock_mqtt_mtx);
    g_mock_mqtt.on_connect_fn = fn;
    g_mock_mqtt.on_connect_user = user;
    pthread_mutex_unlock(&g_mock_mqtt_mtx);
}

void __wrap_mqtt_set_on_disconnect(mqtt_on_disconnect_fn fn, void *user) {
    pthread_mutex_lock(&g_mock_mqtt_mtx);
    g_mock_mqtt.on_disconnect_fn = fn;
    g_mock_mqtt.on_disconnect_user = user;
    pthread_mutex_unlock(&g_mock_mqtt_mtx);
}
