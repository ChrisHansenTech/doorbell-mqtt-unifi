#include "mqtt.h"
#include "config_types.h"
#include "logger.h"
#include "mqtt_router.h"

#include <MQTTClient.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>

#define TIMEOUT 10000L
#define RECONNECT_DELAY 5

static MQTTClient client;
static config_mqtt_t g_mqtt_cfg; 
static bool connected = false;
static mqtt_on_connect_fn g_on_connect = NULL;
static mqtt_on_disconnect_fn g_on_disconnect = NULL;
static void *g_cb_user = NULL;

static const char *g_lwt_topic = NULL;
static const char *g_lwt_payload = NULL;
static int g_lwt_qos = 0;
static int g_lwt_retained = 0;

static void conn_lost(void *context, char *cause) {
    (void)context;
    
    connected = false;

    LOG_WARN("Connection lost: %s", cause ? cause : "(unknown)");
}

static int msg_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    (void)context;

    int got_len = (topicLen > 0) ? topicLen : (int)strlen(topicName);
    LOG_DEBUG("%.*s: %.*s",
           got_len, topicName,
           message->payloadlen, (char*)message->payload);

    mqtt_router_enqueue(topicName, got_len, message->payload, (size_t)message->payloadlen);

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

static void delivered(void *context, MQTTClient_deliveryToken dt) {
    (void)context; (void)dt;
}

static int mqtt_connect_internal(bool reconnect) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_willOptions    will_opts = MQTTClient_willOptions_initializer;
    MQTTClient_SSLOptions     ssl_opts  = MQTTClient_SSLOptions_initializer;

    conn_opts.keepAliveInterval = g_mqtt_cfg.keepalive;
    conn_opts.cleansession      = g_mqtt_cfg.clean_session;

    if (g_mqtt_cfg.username[0]) conn_opts.username = g_mqtt_cfg.username;
    if (g_mqtt_cfg.password[0]) conn_opts.password = g_mqtt_cfg.password;

    // Birth/Last Will
    if (g_lwt_topic && g_lwt_payload) {
        will_opts.message   = (char*)g_lwt_payload;
        will_opts.topicName = (char*)g_lwt_topic;
        will_opts.qos       = g_lwt_qos;
        will_opts.retained  = g_lwt_retained ? 1 : 0;
        conn_opts.will = &will_opts;
    }

    // TLS (optional)
    if (g_mqtt_cfg.tls_enabled) {
        if (!g_mqtt_cfg.cafile[0]) {
            LOG_ERROR("MQTT_SSL=1 but MQTT_CAFILE not set.");
            return -1;
        }
        ssl_opts.trustStore   = g_mqtt_cfg.cafile;
        if (g_mqtt_cfg.certfile[0]) ssl_opts.keyStore   = g_mqtt_cfg.certfile;
        if (g_mqtt_cfg.keyfile[0])  ssl_opts.privateKey = g_mqtt_cfg.keyfile;
        if (g_mqtt_cfg.keypass[0])  ssl_opts.privateKeyPassword = g_mqtt_cfg.keypass;
        conn_opts.ssl = &ssl_opts;
    }

    int rc = MQTTClient_connect(client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR(reconnect 
                ? "[ERROR] MQTT reconnect failed rc=%d to %s"
                : "[ERROR] MQTT connect failed rc=%d to %s", 
            rc, g_mqtt_cfg.address);
        return rc;
    }

    LOG_INFO(reconnect
            ? "Reconnected: %s (client_id=%s)"
            : "Connected: %s (client_id=%s)", 
            g_mqtt_cfg.address, g_mqtt_cfg.client_id);

    connected = true;

    if (g_on_connect) {
        g_on_connect(reconnect, g_cb_user);
    }

    return MQTTCLIENT_SUCCESS;
}

void mqtt_set_on_connect(mqtt_on_connect_fn fn, void *user) {
    g_on_connect = fn;
    g_cb_user = user;
}

void mqtt_set_on_disconnect(mqtt_on_disconnect_fn fn, void *user) {
    g_on_disconnect = fn;
    g_cb_user = user;
}

bool mqtt_set_last_will(const char *topic, const char *payload, int qos, int retained) {
    if (!topic || !payload) {
        return false;
    }

    g_lwt_topic = topic;
    g_lwt_payload = payload;
    g_lwt_qos = qos;
    g_lwt_retained = retained;
    
    return true;
}

bool mqtt_init(const config_mqtt_t *cfg_mqtt_in) {
    if (!cfg_mqtt_in) return -1;
    g_mqtt_cfg = *cfg_mqtt_in;  

    int rc = MQTTClient_create(&client, g_mqtt_cfg.address, g_mqtt_cfg.client_id,
                               MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("MQTTClient_create rc=%d", rc);
        return false;
    }

    MQTTClient_setCallbacks(client, NULL, conn_lost, msg_arrived, delivered);

    //rc = mqtt_connect_internal(false);

    //if (rc != MQTTCLIENT_SUCCESS) {
    //    MQTTClient_destroy(&client);
    //    return false;
    //}

    return true;
}

void mqtt_subscribe(const char *topic) {
    int rc = MQTTClient_subscribe(client, topic, g_mqtt_cfg.qos);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("Subscribe rc=%d topic=%s", rc, topic);
    } else {
        LOG_INFO("Subscribed: %s (qos=%d)", topic, g_mqtt_cfg.qos);
    }
}

void mqtt_publish(const char *topic, const char *payload, int qos, int retained) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    pubmsg.payload    = (char*)payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos        = qos;
    pubmsg.retained   = retained;

    LOG_DEBUG("Publishing %s", payload);

    int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("Publish rc=%d topic=%s", rc, topic);
        return;
    }
    
    char buffer[60];

    size_t payload_len = strlen(payload);
    size_t max_preview = sizeof(buffer) - 4;

    if (payload_len <= max_preview) {
        snprintf(buffer, sizeof(buffer), "%s", payload);
    } else {
        snprintf(buffer, sizeof(buffer), "%.*s...", (int)max_preview, payload);
    }

    LOG_INFO("Published '%s' -> %s", buffer, topic);
}


void mqtt_loop(int timeout_ms) {
    static time_t last_attempt = 0;
    time_t now = time(NULL);

    if (!connected) {
        if (now - last_attempt >= RECONNECT_DELAY) {
            last_attempt = now;
            mqtt_connect_internal(true);
        }
    }

    struct timespec ts;

    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (timeout_ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void mqtt_disconnect(void) {
    if (g_on_disconnect) {
        g_on_disconnect(g_cb_user);
    }

    MQTTClient_disconnect(client, (int)TIMEOUT);
    MQTTClient_destroy(&client);
}

