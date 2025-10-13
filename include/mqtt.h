#ifndef MQTT_H
#define MQTT_H

#include <stddef.h>

typedef struct {
    char address[256];      // e.g., tcp://host:1883 or ssl://host:8883
    char client_id[128];    // default: doorbell-client-<pid>
    char username[128];     // optional
    char password[128];     // optional
    int  qos;               // default: 1
    int  keepalive;         // default: 30
    int  clean_session;     // default: 1
    int  retained_online;   // default: 1 (retain birth/last-will)
    // TLS (optional)
    int  tls_enabled;       // 0/1, default: 0
    char cafile[256];       // path to CA bundle (required if tls_enabled)
    char certfile[256];     // client cert (optional)
    char keyfile[256];      // client key (optional)
    char keypass[128];      // key password (optional)
    // Topics
    char topic_set[128];    // default: doorbell/set
    char topic_status[128]; // default: doorbell/status
} MqttConfig;

int mqtt_load_from_env(MqttConfig *cfg);
int mqtt_init(const MqttConfig *cfg);
void mqtt_subscribe(const char *topic);
void mqtt_publish(const char *topic, const char *payload, int qos, int retained);
void mqtt_loop(int timeout_ms);
void mqtt_disconnect(void);

#endif
