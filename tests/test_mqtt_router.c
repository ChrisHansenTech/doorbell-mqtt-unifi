#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "mqtt_router.h"
#include "mqtt_router_types.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cv = PTHREAD_COND_INITIALIZER;
static int g_handled_count = 0;
static char g_last_payload[256];
static size_t g_last_len = 0;
static const mqtt_router_ctx_t *g_last_ctx = NULL;

static void reset_handler_state(void) {
    pthread_mutex_lock(&g_mtx);
    g_handled_count = 0;
    g_last_payload[0] = '\0';
    g_last_len = 0;
    g_last_ctx = NULL;
    pthread_mutex_unlock(&g_mtx);
}

static void test_handler(const mqtt_router_ctx_t *ctx, const char *payload, size_t len) {
    pthread_mutex_lock(&g_mtx);
    g_last_ctx = ctx;
    g_last_len = len;
    size_t copy = len < sizeof(g_last_payload) - 1 ? len : sizeof(g_last_payload) - 1;
    if (payload && copy > 0) {
        memcpy(g_last_payload, payload, copy);
    }
    g_last_payload[copy] = '\0';
    g_handled_count++;
    pthread_cond_broadcast(&g_cv);
    pthread_mutex_unlock(&g_mtx);
}

static void wait_for_handled_count(int expected) {
    pthread_mutex_lock(&g_mtx);
    while (g_handled_count < expected) {
        pthread_cond_wait(&g_cv, &g_mtx);
    }
    pthread_mutex_unlock(&g_mtx);
}

void setUp(void) {
    reset_handler_state();
}

void tearDown(void) {
    mqtt_router_stop();
}

void test_router_stop_when_not_started_is_safe(void) {
    mqtt_router_stop();
    TEST_PASS();
}

void test_router_start_dispatch_duplicate_and_unknown_topics(void) {
    TEST_ASSERT_EQUAL_INT(0, mqtt_routes_add("test/cmd/alpha", test_handler));

    mqtt_router_ctx_t ctx = {0};
    TEST_ASSERT_TRUE(mqtt_router_start(&ctx));
    /* Second start while running must fail */
    TEST_ASSERT_FALSE(mqtt_router_start(&ctx));

    /* Topic with topicLen == 0 uses strlen */
    TEST_ASSERT_EQUAL_INT(0, mqtt_router_enqueue("test/cmd/alpha", 0, "Christmas", 9));
    wait_for_handled_count(1);
    TEST_ASSERT_EQUAL_PTR(&ctx, g_last_ctx);
    TEST_ASSERT_EQUAL_STRING("Christmas", g_last_payload);
    TEST_ASSERT_EQUAL_UINT(9, g_last_len);

    /* Duplicate message with explicit topicLen */
    TEST_ASSERT_EQUAL_INT(0, mqtt_router_enqueue("test/cmd/alpha_extra", 14, "Christmas", 9));
    wait_for_handled_count(2);
    TEST_ASSERT_EQUAL_INT(2, g_handled_count);

    /* Unknown topic should be ignored without crashing, followed by known topic as barrier */
    TEST_ASSERT_EQUAL_INT(0, mqtt_router_enqueue("test/cmd/unknown", 0, "payload", 7));
    TEST_ASSERT_EQUAL_INT(0, mqtt_router_enqueue("test/cmd/alpha", 0, "done", 4));
    wait_for_handled_count(3);
    TEST_ASSERT_EQUAL_STRING("done", g_last_payload);

    mqtt_router_stop();
}

void test_router_queue_full_drops_messages_and_drains_on_stop(void) {
    /* Router is stopped; ring buffer has capacity 64 => 63 usable slots */
    for (int i = 0; i < 63; i++) {
        TEST_ASSERT_EQUAL_INT(0, mqtt_router_enqueue("test/cmd/alpha", 0, "x", 1));
    }
    /* 64th enqueue must fail with -1 */
    TEST_ASSERT_EQUAL_INT(-1, mqtt_router_enqueue("test/cmd/alpha", 0, "overflow", 8));

    /* Start worker and verify all 63 queued items drain */
    mqtt_router_ctx_t ctx = {0};
    TEST_ASSERT_TRUE(mqtt_router_start(&ctx));
    wait_for_handled_count(63);
    TEST_ASSERT_EQUAL_INT(63, g_handled_count);
    mqtt_router_stop();
}

static void *producer_thread(void *arg) {
    (void)arg;
    for (int i = 0; i < 10; i++) {
        while (mqtt_router_enqueue("test/cmd/alpha", 0, "msg", 3) != 0) {
            /* retry if momentarily full */
        }
    }
    return NULL;
}

void test_router_concurrent_enqueues(void) {
    mqtt_router_ctx_t ctx = {0};
    TEST_ASSERT_TRUE(mqtt_router_start(&ctx));

    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT(0, pthread_create(&threads[i], NULL, producer_thread, NULL));
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    wait_for_handled_count(40);
    TEST_ASSERT_EQUAL_INT(40, g_handled_count);
    mqtt_router_stop();
}

void test_router_max_routes_enforced(void) {
    /* 1 route was already registered above; fill remaining 15 slots up to MAX_ROUTES (16) */
    char topic[64];
    for (int i = 1; i < 16; i++) {
        snprintf(topic, sizeof(topic), "test/cmd/slot_%d", i);
        TEST_ASSERT_EQUAL_INT(0, mqtt_routes_add(topic, test_handler));
    }
    /* 17th route must fail */
    TEST_ASSERT_EQUAL_INT(-1, mqtt_routes_add("test/cmd/overflow", test_handler));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_router_stop_when_not_started_is_safe);
    RUN_TEST(test_router_start_dispatch_duplicate_and_unknown_topics);
    RUN_TEST(test_router_queue_full_drops_messages_and_drains_on_stop);
    RUN_TEST(test_router_concurrent_enqueues);
    RUN_TEST(test_router_max_routes_enforced);

    return UNITY_END();
}
