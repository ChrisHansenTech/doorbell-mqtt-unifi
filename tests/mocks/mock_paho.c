#define _POSIX_C_SOURCE 200809L

#include "mock_paho.h"

#include <stdio.h>
#include <string.h>

mock_paho_state_t g_mock_paho;
static int g_fake_mqtt_handle = 1;

void mock_paho_reset(void) {
    memset(&g_mock_paho, 0, sizeof(g_mock_paho));
    g_mock_paho.create_rc = MQTTCLIENT_SUCCESS;
    g_mock_paho.connect_rc = MQTTCLIENT_SUCCESS;
    g_mock_paho.subscribe_rc = MQTTCLIENT_SUCCESS;
    g_mock_paho.publish_rc = MQTTCLIENT_SUCCESS;
    g_mock_paho.disconnect_rc = MQTTCLIENT_SUCCESS;
    g_mock_paho.fake_time = 1000;
}

int __wrap_MQTTClient_create(MQTTClient *handle, const char *serverURI, const char *clientId, int persistence_type, void *persistence_context) {
    (void)persistence_type;
    (void)persistence_context;
    g_mock_paho.create_calls++;
    snprintf(g_mock_paho.last_uri, sizeof(g_mock_paho.last_uri), "%s", serverURI ? serverURI : "");
    snprintf(g_mock_paho.last_client_id, sizeof(g_mock_paho.last_client_id), "%s", clientId ? clientId : "");
    if (g_mock_paho.create_rc == MQTTCLIENT_SUCCESS && handle) {
        *handle = &g_fake_mqtt_handle;
    }
    return g_mock_paho.create_rc;
}

int __wrap_MQTTClient_setCallbacks(MQTTClient handle, void *context, MQTTClient_connectionLost *cl, MQTTClient_messageArrived *ma, MQTTClient_deliveryComplete *dc) {
    (void)handle;
    g_mock_paho.set_callbacks_calls++;
    g_mock_paho.cb_context = context;
    g_mock_paho.cb_conn_lost = cl;
    g_mock_paho.cb_msg_arrived = ma;
    g_mock_paho.cb_delivered = dc;
    return MQTTCLIENT_SUCCESS;
}

int __wrap_MQTTClient_connect(MQTTClient handle, MQTTClient_connectOptions *options) {
    (void)handle;
    g_mock_paho.connect_calls++;
    if (options) {
        g_mock_paho.last_keepalive = options->keepAliveInterval;
        g_mock_paho.last_cleansession = options->cleansession;
        snprintf(g_mock_paho.last_username, sizeof(g_mock_paho.last_username), "%s", options->username ? options->username : "");
        snprintf(g_mock_paho.last_password, sizeof(g_mock_paho.last_password), "%s", options->password ? options->password : "");

        g_mock_paho.last_has_will = (options->will != NULL);
        if (options->will) {
            snprintf(g_mock_paho.last_will_topic, sizeof(g_mock_paho.last_will_topic), "%s", options->will->topicName ? options->will->topicName : "");
            snprintf(g_mock_paho.last_will_msg, sizeof(g_mock_paho.last_will_msg), "%s", options->will->message ? options->will->message : "");
            g_mock_paho.last_will_qos = options->will->qos;
            g_mock_paho.last_will_retained = options->will->retained;
        }

        g_mock_paho.last_has_ssl = (options->ssl != NULL);
        if (options->ssl) {
            snprintf(g_mock_paho.last_ssl_trust, sizeof(g_mock_paho.last_ssl_trust), "%s", options->ssl->trustStore ? options->ssl->trustStore : "");
            snprintf(g_mock_paho.last_ssl_keystore, sizeof(g_mock_paho.last_ssl_keystore), "%s", options->ssl->keyStore ? options->ssl->keyStore : "");
            snprintf(g_mock_paho.last_ssl_privkey, sizeof(g_mock_paho.last_ssl_privkey), "%s", options->ssl->privateKey ? options->ssl->privateKey : "");
            snprintf(g_mock_paho.last_ssl_keypass, sizeof(g_mock_paho.last_ssl_keypass), "%s", options->ssl->privateKeyPassword ? options->ssl->privateKeyPassword : "");
        }
    }
    return g_mock_paho.connect_rc;
}

int __wrap_MQTTClient_subscribe(MQTTClient handle, const char *topic, int qos) {
    (void)handle;
    g_mock_paho.subscribe_calls++;
    snprintf(g_mock_paho.last_sub_topic, sizeof(g_mock_paho.last_sub_topic), "%s", topic ? topic : "");
    g_mock_paho.last_sub_qos = qos;
    return g_mock_paho.subscribe_rc;
}

int __wrap_MQTTClient_publishMessage(MQTTClient handle, const char *topicName, MQTTClient_message *msg, MQTTClient_deliveryToken *dt) {
    (void)handle;
    g_mock_paho.publish_calls++;
    snprintf(g_mock_paho.last_pub_topic, sizeof(g_mock_paho.last_pub_topic), "%s", topicName ? topicName : "");
    if (msg) {
        size_t copy_len = (size_t)msg->payloadlen;
        if (copy_len >= sizeof(g_mock_paho.last_pub_payload)) {
            copy_len = sizeof(g_mock_paho.last_pub_payload) - 1;
        }
        if (msg->payload && copy_len > 0) {
            memcpy(g_mock_paho.last_pub_payload, msg->payload, copy_len);
        }
        g_mock_paho.last_pub_payload[copy_len] = '\0';
        g_mock_paho.last_pub_qos = msg->qos;
        g_mock_paho.last_pub_retained = msg->retained;
    }
    if (dt) {
        *dt = 1;
    }
    return g_mock_paho.publish_rc;
}

int __wrap_MQTTClient_disconnect(MQTTClient handle, int timeout) {
    (void)handle;
    (void)timeout;
    g_mock_paho.disconnect_calls++;
    return g_mock_paho.disconnect_rc;
}

void __wrap_MQTTClient_destroy(MQTTClient *handle) {
    g_mock_paho.destroy_calls++;
    if (handle) {
        *handle = NULL;
    }
}

void __wrap_MQTTClient_freeMessage(MQTTClient_message **msg) {
    g_mock_paho.free_message_calls++;
    if (msg) {
        *msg = NULL;
    }
}

void __wrap_MQTTClient_free(void *ptr) {
    (void)ptr;
    g_mock_paho.free_calls++;
}

time_t __wrap_time(time_t *tloc) {
    if (tloc) {
        *tloc = g_mock_paho.fake_time;
    }
    return g_mock_paho.fake_time;
}

int __wrap_nanosleep(const struct timespec *req, struct timespec *rem) {
    (void)req;
    (void)rem;
    g_mock_paho.nanosleep_calls++;
    return 0;
}

int __wrap_mqtt_router_enqueue(const char *topic, int topicLen, const char *payload, size_t payloadLen) {
    g_mock_paho.enqueue_calls++;
    g_mock_paho.last_enqueued_topic_len = topicLen;
    size_t tcopy = (size_t)topicLen;
    if (tcopy >= sizeof(g_mock_paho.last_enqueued_topic)) {
        tcopy = sizeof(g_mock_paho.last_enqueued_topic) - 1;
    }
    if (topic && tcopy > 0) {
        memcpy(g_mock_paho.last_enqueued_topic, topic, tcopy);
    }
    g_mock_paho.last_enqueued_topic[tcopy] = '\0';

    g_mock_paho.last_enqueued_payload_len = payloadLen;
    size_t pcopy = payloadLen;
    if (pcopy >= sizeof(g_mock_paho.last_enqueued_payload)) {
        pcopy = sizeof(g_mock_paho.last_enqueued_payload) - 1;
    }
    if (payload && pcopy > 0) {
        memcpy(g_mock_paho.last_enqueued_payload, payload, pcopy);
    }
    g_mock_paho.last_enqueued_payload[pcopy] = '\0';
    return 0;
}
