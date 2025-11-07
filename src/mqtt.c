#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include "MQTTClient.h"
#include "mqtt_inbound.h"
#include "mqtt.h"
#include "command.h"

#define TIMEOUT 10000L

static MQTTClient client;
static MqttConfig g_cfg; 
static _Thread_local bool in_callback = false;

// ---------- Helpers ----------
static const char* env_or(const char *name, const char *fallback) {
    const char *v = getenv(name);
    return (v && *v) ? v : fallback;
}

static void set_str(char *dst, size_t cap, const char *src, const char *fallback) {
    const char *s = (src && *src) ? src : fallback;
    if (!s) { dst[0] = '\0'; return; }
    snprintf(dst, cap, "%s", s);
}

// ---------- Public: load from env ----------
int mqtt_load_from_env(MqttConfig *cfg) {
    if (!cfg) return -1;

    const char *host   = env_or("MQTT_HOST", "localhost");
    const char *port   = env_or("MQTT_PORT", "1883");
    const char *ssl    = env_or("MQTT_SSL",  "0");

    int tls_enabled = (strcmp(ssl, "1") == 0 || strcasecmp(ssl, "true") == 0);
    char proto[8];
    snprintf(proto, sizeof(proto), "%s", tls_enabled ? "ssl" : "tcp");

    snprintf(cfg->address, sizeof(cfg->address), "%s://%s:%s", proto, host, port);

    char default_client[128];
    snprintf(default_client, sizeof(default_client), "doorbell-client-%d", getpid());
    set_str(cfg->client_id, sizeof(cfg->client_id), getenv("MQTT_CLIENT_ID"), default_client);

    set_str(cfg->username, sizeof(cfg->username), getenv("MQTT_USERNAME"), "");
    set_str(cfg->password, sizeof(cfg->password), getenv("MQTT_PASSWORD"), "");

    cfg->qos          = atoi(env_or("MQTT_QOS", "1"));
    cfg->keepalive    = atoi(env_or("MQTT_KEEPALIVE", "30"));
    cfg->clean_session= atoi(env_or("MQTT_CLEAN_SESSION", "1"));
    cfg->retained_online = atoi(env_or("MQTT_RETAINED_ONLINE", "1"));

    cfg->tls_enabled = tls_enabled;
    set_str(cfg->cafile,   sizeof(cfg->cafile),   getenv("MQTT_CAFILE"),   "");
    set_str(cfg->certfile, sizeof(cfg->certfile), getenv("MQTT_CERTFILE"), "");
    set_str(cfg->keyfile,  sizeof(cfg->keyfile),  getenv("MQTT_KEYFILE"),  "");
    set_str(cfg->keypass,  sizeof(cfg->keypass),  getenv("MQTT_KEYPASS"),  "");

    set_str(cfg->topic_set,    sizeof(cfg->topic_set),    getenv("MQTT_TOPIC_SET"),    "doorbell/set");
    set_str(cfg->topic_status, sizeof(cfg->topic_status), getenv("MQTT_TOPIC_STATUS"), "doorbell/status");

    return 0;
}

// ---------- MQTT Callbacks ----------
static void conn_lost(void *context, char *cause) {
    (void)context;
    
    fprintf(stderr, "[MQTT] Connection lost: %s\n", cause ? cause : "(unknown)");
}

static int msg_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    (void)context;

    int got_len = (topicLen > 0) ? topicLen : (int)strlen(topicName);
    printf("[MQTT] %.*s: %.*s\n",
           got_len, topicName,
           message->payloadlen, (char*)message->payload);

    mqtt_inbound_enqueue(topicName, topicLen, message->payload, (size_t)message->payloadlen);

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

static void delivered(void *context, MQTTClient_deliveryToken dt) {
    (void)context; (void)dt;
}

// ---------- Public: init/connect ----------
int mqtt_init(const MqttConfig *cfg_in) {
    if (!cfg_in) return -1;
    g_cfg = *cfg_in; // copy for later use

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_willOptions    will_opts = MQTTClient_willOptions_initializer;
    MQTTClient_SSLOptions     ssl_opts  = MQTTClient_SSLOptions_initializer;

    int rc = MQTTClient_create(&client, g_cfg.address, g_cfg.client_id,
                               MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "[ERROR] MQTTClient_create rc=%d\n", rc);
        return -1;
    }

    MQTTClient_setCallbacks(client, NULL, conn_lost, msg_arrived, delivered);

    conn_opts.keepAliveInterval = g_cfg.keepalive;
    conn_opts.cleansession      = g_cfg.clean_session;

    if (g_cfg.username[0]) conn_opts.username = g_cfg.username;
    if (g_cfg.password[0]) conn_opts.password = g_cfg.password;

    // Birth/Last Will
    const char *lw_topic = g_cfg.topic_status;
    const char *lw_msg   = "{\"status\":\"offline\"}";
    will_opts.message    = (char*)lw_msg;
    will_opts.topicName  = (char*)lw_topic;
    will_opts.qos        = g_cfg.qos;
    will_opts.retained   = g_cfg.retained_online ? 1 : 0;
    conn_opts.will = &will_opts;

    // TLS (optional)
    if (g_cfg.tls_enabled) {
        if (!g_cfg.cafile[0]) {
            fprintf(stderr, "[ERROR] MQTT_SSL=1 but MQTT_CAFILE not set\n");
            return -1;
        }
        ssl_opts.trustStore   = g_cfg.cafile;
        if (g_cfg.certfile[0]) ssl_opts.keyStore   = g_cfg.certfile;
        if (g_cfg.keyfile[0])  ssl_opts.privateKey = g_cfg.keyfile;
        if (g_cfg.keypass[0])  ssl_opts.privateKeyPassword = g_cfg.keypass;
        conn_opts.ssl = &ssl_opts;
    }

    rc = MQTTClient_connect(client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "[ERROR] MQTTClient_connect rc=%d to %s\n", rc, g_cfg.address);
        MQTTClient_destroy(&client);
        return -1;
    }

    printf("[INFO] Connected: %s (client_id=%s)\n", g_cfg.address, g_cfg.client_id);

    // Publish "online" retained
    mqtt_publish(g_cfg.topic_status, "{\"status\":\"online\"}", g_cfg.qos, g_cfg.retained_online);

    mqtt_inbound_start();

    return 0;
}

void mqtt_subscribe(const char *topic) {
    int rc = MQTTClient_subscribe(client, topic, g_cfg.qos);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "[ERROR] Subscribe rc=%d topic=%s\n", rc, topic);
    } else {
        printf("[INFO] Subscribed: %s (qos=%d)\n", topic, g_cfg.qos);
    }

    mqtt_routes_add(topic, handle_set);

}

void mqtt_publish(const char *topic, const char *payload, int qos, int retained) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    pubmsg.payload    = (char*)payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos        = qos;
    pubmsg.retained   = retained;

    printf("[MQTT] Publishing %s (in_callback=%d)\n", payload, in_callback);


    int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "[ERROR] Publish rc=%d topic=%s\n", rc, topic);
        return;
    }

    if (!in_callback && rc == MQTTCLIENT_SUCCESS) {
        MQTTClient_waitForCompletion(client, token, TIMEOUT);
    }
    
    printf("[INFO] Published -> %s\n", topic);
}

void mqtt_loop(int timeout_ms) {
    // If you lean on callbacks, there’s no busy work needed here.
    // Sleep to keep CPU low; Paho runs its own network thread.
    struct timespec ts;

    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (timeout_ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void mqtt_disconnect(void) {
    // send offline before disconnect (only if not relying solely on LWT)
    mqtt_publish(g_cfg.topic_status, "{\"status\":\"offline\"}", g_cfg.qos, g_cfg.retained_online);
    MQTTClient_disconnect(client, (int)TIMEOUT);
    MQTTClient_destroy(&client);
}

int mqtt_publish_status(const char *json) {
    mqtt_publish(g_cfg.topic_status, json, g_cfg.qos, 0);
    return 0;
}
