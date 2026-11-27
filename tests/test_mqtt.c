#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "mocks/mock_paho.h"
#include "config_types.h"
#include "mqtt.h"

#include <string.h>

static int g_on_connect_calls = 0;
static bool g_last_reconnect_flag = false;
static void *g_last_connect_user = NULL;
static int g_on_disconnect_calls = 0;
static void *g_last_disconnect_user = NULL;

static void on_connect_cb(bool reconnect, void *user) {
    g_on_connect_calls++;
    g_last_reconnect_flag = reconnect;
    g_last_connect_user = user;
}

static void on_disconnect_cb(void *user) {
    g_on_disconnect_calls++;
    g_last_disconnect_user = user;
}

static config_mqtt_t make_mqtt_cfg(void) {
    config_mqtt_t cfg = {0};
    snprintf(cfg.address, sizeof(cfg.address), "tcp://127.0.0.1:1883");
    snprintf(cfg.client_id, sizeof(cfg.client_id), "test-client");
    snprintf(cfg.username, sizeof(cfg.username), "user1");
    snprintf(cfg.password, sizeof(cfg.password), "pass1");
    cfg.qos = 1;
    cfg.keepalive = 30;
    cfg.clean_session = 1;
    cfg.retained_online = 1;
    return cfg;
}

static time_t g_clock = 1000;

void setUp(void) {
    g_clock += 100; /* ensure RECONNECT_DELAY (5s) always elapsed since prior test */
    mock_paho_reset();
    g_mock_paho.fake_time = g_clock;

    g_on_connect_calls = 0;
    g_last_reconnect_flag = false;
    g_last_connect_user = NULL;
    g_on_disconnect_calls = 0;
    g_last_disconnect_user = NULL;

    mqtt_set_on_connect(NULL, NULL);
    mqtt_set_on_disconnect(NULL, NULL);
}

void tearDown(void) {
    /* Reset static `connected` inside src/mqtt.c via its registered conn_lost callback */
    if (g_mock_paho.cb_conn_lost) {
        g_mock_paho.cb_conn_lost(NULL, "test teardown");
    }
    mqtt_set_on_connect(NULL, NULL);
    mqtt_set_on_disconnect(NULL, NULL);
}

void test_mqtt_init_success_and_create_failure(void) {
    config_mqtt_t cfg = make_mqtt_cfg();

    g_mock_paho.create_rc = MQTTCLIENT_FAILURE;
    TEST_ASSERT_FALSE(mqtt_init(&cfg));
    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.create_calls);
    TEST_ASSERT_EQUAL_UINT(0, g_mock_paho.set_callbacks_calls);

    g_mock_paho.create_rc = MQTTCLIENT_SUCCESS;
    TEST_ASSERT_TRUE(mqtt_init(&cfg));
    TEST_ASSERT_EQUAL_UINT(2, g_mock_paho.create_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.set_callbacks_calls);
    TEST_ASSERT_EQUAL_STRING("tcp://127.0.0.1:1883", g_mock_paho.last_uri);
    TEST_ASSERT_EQUAL_STRING("test-client", g_mock_paho.last_client_id);
}

void test_mqtt_set_last_will_validation(void) {
    TEST_ASSERT_FALSE(mqtt_set_last_will(NULL, "offline", 1, 1));
    TEST_ASSERT_FALSE(mqtt_set_last_will("test/avail", NULL, 1, 1));
    TEST_ASSERT_TRUE(mqtt_set_last_will("test/avail", "offline", 1, 1));
}

void test_mqtt_connect_with_credentials_and_lwt_and_callbacks(void) {
    int user_ctx = 99;
    config_mqtt_t cfg = make_mqtt_cfg();

    TEST_ASSERT_TRUE(mqtt_set_last_will("pfx/doorbell-mqtt/default/availability", "offline", 1, 1));
    mqtt_set_on_connect(on_connect_cb, &user_ctx);
    mqtt_set_on_disconnect(on_disconnect_cb, &user_ctx);
    TEST_ASSERT_TRUE(mqtt_init(&cfg));

    mqtt_loop(100);

    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.connect_calls);
    TEST_ASSERT_EQUAL_INT(30, g_mock_paho.last_keepalive);
    TEST_ASSERT_EQUAL_INT(1, g_mock_paho.last_cleansession);
    TEST_ASSERT_EQUAL_STRING("user1", g_mock_paho.last_username);
    TEST_ASSERT_EQUAL_STRING("pass1", g_mock_paho.last_password);
    TEST_ASSERT_TRUE(g_mock_paho.last_has_will);
    TEST_ASSERT_EQUAL_STRING("pfx/doorbell-mqtt/default/availability", g_mock_paho.last_will_topic);
    TEST_ASSERT_EQUAL_STRING("offline", g_mock_paho.last_will_msg);
    TEST_ASSERT_EQUAL_INT(1, g_mock_paho.last_will_qos);
    TEST_ASSERT_EQUAL_INT(1, g_mock_paho.last_will_retained);
    TEST_ASSERT_FALSE(g_mock_paho.last_has_ssl);

    TEST_ASSERT_EQUAL_INT(1, g_on_connect_calls);
    TEST_ASSERT_EQUAL_PTR(&user_ctx, g_last_connect_user);

    /* Calling mqtt_loop again while connected must NOT reconnect */
    g_mock_paho.fake_time += 10;
    mqtt_loop(100);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.connect_calls);

    mqtt_disconnect();
    TEST_ASSERT_EQUAL_INT(1, g_on_disconnect_calls);
    TEST_ASSERT_EQUAL_PTR(&user_ctx, g_last_disconnect_user);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.disconnect_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.destroy_calls);
}

void test_mqtt_reconnect_backoff_and_unexpected_disconnect(void) {
    config_mqtt_t cfg = make_mqtt_cfg();
    mqtt_set_on_connect(on_connect_cb, NULL);
    TEST_ASSERT_TRUE(mqtt_init(&cfg));

    /* First attempt fails */
    g_mock_paho.connect_rc = MQTTCLIENT_FAILURE;
    mqtt_loop(100);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.connect_calls);
    TEST_ASSERT_EQUAL_INT(0, g_on_connect_calls);

    /* 4 seconds later (< 5s RECONNECT_DELAY): should NOT retry */
    g_mock_paho.fake_time += 4;
    mqtt_loop(100);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.connect_calls);

    /* 1 more second (total 5s): retries and succeeds */
    g_mock_paho.fake_time += 1;
    g_mock_paho.connect_rc = MQTTCLIENT_SUCCESS;
    mqtt_loop(100);
    TEST_ASSERT_EQUAL_UINT(2, g_mock_paho.connect_calls);
    TEST_ASSERT_EQUAL_INT(1, g_on_connect_calls);

    /* Simulate unexpected disconnect (including NULL cause) */
    g_mock_paho.cb_conn_lost(NULL, NULL);
    g_mock_paho.fake_time += 5;
    mqtt_loop(100);
    TEST_ASSERT_EQUAL_UINT(3, g_mock_paho.connect_calls);
    TEST_ASSERT_EQUAL_INT(2, g_on_connect_calls);
}

void test_mqtt_tls_requires_cafile_and_sets_ssl_options(void) {
    config_mqtt_t cfg = make_mqtt_cfg();
    cfg.tls_enabled = 1;
    cfg.cafile[0] = '\0';
    TEST_ASSERT_TRUE(mqtt_init(&cfg));

    /* Missing cafile must fail before calling MQTTClient_connect */
    mqtt_loop(50);
    TEST_ASSERT_EQUAL_UINT(0, g_mock_paho.connect_calls);

    /* Configure full mTLS options */
    snprintf(cfg.cafile, sizeof(cfg.cafile), "/certs/ca.pem");
    snprintf(cfg.certfile, sizeof(cfg.certfile), "/certs/client.pem");
    snprintf(cfg.keyfile, sizeof(cfg.keyfile), "/certs/client.key");
    snprintf(cfg.keypass, sizeof(cfg.keypass), "secret");
    TEST_ASSERT_TRUE(mqtt_init(&cfg));

    g_mock_paho.fake_time += 10;
    mqtt_loop(50);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.connect_calls);
    TEST_ASSERT_TRUE(g_mock_paho.last_has_ssl);
    TEST_ASSERT_EQUAL_STRING("/certs/ca.pem", g_mock_paho.last_ssl_trust);
    TEST_ASSERT_EQUAL_STRING("/certs/client.pem", g_mock_paho.last_ssl_keystore);
    TEST_ASSERT_EQUAL_STRING("/certs/client.key", g_mock_paho.last_ssl_privkey);
    TEST_ASSERT_EQUAL_STRING("secret", g_mock_paho.last_ssl_keypass);
}

void test_mqtt_subscribe_publish_and_incoming_messages(void) {
    config_mqtt_t cfg = make_mqtt_cfg();
    cfg.qos = 2;
    TEST_ASSERT_TRUE(mqtt_init(&cfg));

    /* Subscribe success and failure */
    mqtt_subscribe("cmd/preset_set");
    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.subscribe_calls);
    TEST_ASSERT_EQUAL_STRING("cmd/preset_set", g_mock_paho.last_sub_topic);
    TEST_ASSERT_EQUAL_INT(2, g_mock_paho.last_sub_qos);

    g_mock_paho.subscribe_rc = MQTTCLIENT_FAILURE;
    mqtt_subscribe("cmd/fail");
    TEST_ASSERT_EQUAL_UINT(2, g_mock_paho.subscribe_calls);

    /* Publish short payload, long payload (>56 bytes for preview truncation), and failure */
    mqtt_publish("status", "idle", 1, 1);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.publish_calls);
    TEST_ASSERT_EQUAL_STRING("status", g_mock_paho.last_pub_topic);
    TEST_ASSERT_EQUAL_STRING("idle", g_mock_paho.last_pub_payload);
    TEST_ASSERT_EQUAL_INT(1, g_mock_paho.last_pub_qos);
    TEST_ASSERT_EQUAL_INT(1, g_mock_paho.last_pub_retained);

    const char *long_msg = "012345678901234567890123456789012345678901234567890123456789_extra_tail";
    mqtt_publish("long/topic", long_msg, 0, 0);
    TEST_ASSERT_EQUAL_UINT(2, g_mock_paho.publish_calls);
    TEST_ASSERT_EQUAL_STRING(long_msg, g_mock_paho.last_pub_payload);

    g_mock_paho.publish_rc = MQTTCLIENT_FAILURE;
    mqtt_publish("fail/topic", "x", 1, 0);
    TEST_ASSERT_EQUAL_UINT(3, g_mock_paho.publish_calls);

    /* Incoming message callback (topicLen == 0 and explicit topicLen) */
    char topic1[] = "pfx/doorbell-mqtt/default/cmd/preset_set";
    char payload1[] = "Christmas";
    MQTTClient_message msg1 = MQTTClient_message_initializer;
    msg1.payload = payload1;
    msg1.payloadlen = (int)strlen(payload1);

    int rc = g_mock_paho.cb_msg_arrived(NULL, topic1, 0, &msg1);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.enqueue_calls);
    TEST_ASSERT_EQUAL_STRING(topic1, g_mock_paho.last_enqueued_topic);
    TEST_ASSERT_EQUAL_STRING("Christmas", g_mock_paho.last_enqueued_payload);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.free_message_calls);
    TEST_ASSERT_EQUAL_UINT(1, g_mock_paho.free_calls);

    char topic2[] = "short_topic_ignored_suffix";
    char payload2[] = "ok";
    MQTTClient_message msg2 = MQTTClient_message_initializer;
    msg2.payload = payload2;
    msg2.payloadlen = 2;
    rc = g_mock_paho.cb_msg_arrived(NULL, topic2, 11, &msg2);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_EQUAL_STRING("short_topic", g_mock_paho.last_enqueued_topic);

    /* Delivery callback is safe */
    g_mock_paho.cb_delivered(NULL, 123);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_mqtt_init_success_and_create_failure);
    RUN_TEST(test_mqtt_set_last_will_validation);
    RUN_TEST(test_mqtt_connect_with_credentials_and_lwt_and_callbacks);
    RUN_TEST(test_mqtt_reconnect_backoff_and_unexpected_disconnect);
    RUN_TEST(test_mqtt_tls_requires_cafile_and_sets_ssl_options);
    RUN_TEST(test_mqtt_subscribe_publish_and_incoming_messages);

    return UNITY_END();
}
