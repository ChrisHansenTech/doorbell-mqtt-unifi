#pragma once

#include <MQTTClient.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

typedef struct {
    int create_rc;
    int connect_rc;
    int subscribe_rc;
    int publish_rc;
    int disconnect_rc;

    size_t create_calls;
    size_t set_callbacks_calls;
    size_t connect_calls;
    size_t subscribe_calls;
    size_t publish_calls;
    size_t disconnect_calls;
    size_t destroy_calls;
    size_t free_message_calls;
    size_t free_calls;
    size_t nanosleep_calls;

    char last_uri[256];
    char last_client_id[256];
    char last_sub_topic[256];
    int last_sub_qos;
    char last_pub_topic[256];
    char last_pub_payload[512];
    int last_pub_qos;
    int last_pub_retained;

    int last_keepalive;
    int last_cleansession;
    char last_username[128];
    char last_password[128];
    bool last_has_will;
    char last_will_topic[256];
    char last_will_msg[256];
    int last_will_qos;
    int last_will_retained;
    bool last_has_ssl;
    char last_ssl_trust[256];
    char last_ssl_keystore[256];
    char last_ssl_privkey[256];
    char last_ssl_keypass[128];

    void *cb_context;
    MQTTClient_connectionLost *cb_conn_lost;
    MQTTClient_messageArrived *cb_msg_arrived;
    MQTTClient_deliveryComplete *cb_delivered;

    time_t fake_time;

    size_t enqueue_calls;
    char last_enqueued_topic[256];
    int last_enqueued_topic_len;
    char last_enqueued_payload[512];
    size_t last_enqueued_payload_len;
} mock_paho_state_t;

extern mock_paho_state_t g_mock_paho;

void mock_paho_reset(void);
