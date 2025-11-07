#include "mqtt_inbound.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define IN_Q_CAP 64
#define MAX_ROUTES 16

struct InMsg {
    char *topic;
    int topic_len;
    char *payload;
    size_t payload_len;
};

static struct {
    struct InMsg buf[IN_Q_CAP];
    size_t head, tail;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
    pthread_t th;
    int running;
} inq = {
    .head = 0, .tail = 0,
    .mtx = PTHREAD_MUTEX_INITIALIZER,
    .cv = PTHREAD_COND_INITIALIZER,
    .running = 0
};

static bool inq_push_locked(const char *topic, int topicLen, const void *payload, size_t payloadLen) {
    size_t next = (inq.tail + 1) % IN_Q_CAP;
    if (next == inq.head) return false; // queue is full

    int tlen = topicLen > 0 ? topicLen : (int)strlen(topic);
    char *tcopy = (char*)malloc((size_t)tlen + 1);
    if (!tcopy) return false;
    memcpy(tcopy, topic, tlen);
    tcopy[tlen] = '\0';

    char *pcopy = (char*)malloc(payloadLen + 1);
    if(!pcopy) return false;
    memcpy(pcopy, payload, payloadLen);
    pcopy[payloadLen] = '\0';

    inq.buf[inq.tail] = (struct InMsg){ tcopy, tlen, pcopy, payloadLen };
    inq.tail = next;
    pthread_cond_signal(&inq.cv);

    return true;
}

int mqtt_inbound_enqueue(const char *topic, int topicLen, const char *payload, size_t payloadLen) {
    int ok = 0;

    pthread_mutex_lock(&inq.mtx);

    ok = inq_push_locked(topic, topicLen, payload, payloadLen) ? 0 : -1;

    pthread_mutex_unlock(&inq.mtx);

    if (ok != 0) {
        fprintf(stderr,"[WARN] Inbound queue is full, dropping message on '%.*s'\n",
                (topicLen > 0) ? topicLen : (int)strlen(topic), topic);
    }

    return ok;
}

static bool inq_pop(struct InMsg *out) {
    pthread_mutex_lock(&inq.mtx);

    while (inq.head == inq.tail && inq.running) {
        pthread_cond_wait(&inq.cv, &inq.mtx);
    }

    if (!inq.running && inq.head == inq.tail) {
        pthread_mutex_unlock(&inq.mtx);
        return false;
    }

    *out = inq.buf[inq.head];
    inq.head = (inq.head + 1) % IN_Q_CAP;
    pthread_mutex_unlock(&inq.mtx);
    return true;
}

static void inq_free(struct InMsg *m) {
    free(m->topic);
    free(m->payload);
}

struct Route {
    const char *topic;
    mqtt_handler_fn fn;
};

static struct Route routes[MAX_ROUTES];
static size_t route_count = 0;

int mqtt_routes_add(const char *topic, mqtt_handler_fn fn) {
    if (route_count >= MAX_ROUTES) return -1;

    routes[route_count++] = (struct Route){ topic, fn };
    return 0;
}

static void mqtt_routes_dispatch(const char *topic, const char *payload, size_t len) {
    for (size_t i = 0; i < route_count; i++) {
        if (routes[i].topic && strcmp(topic, routes[i].topic) == 0) {
            routes->fn(payload, len);
            return;
        }
    }

    fprintf(stderr, "[WARN] Unknown topic: '%.s'\n", topic);
}

static void *in_worker(void *arg) {
    (void)arg;

    struct InMsg m;

    while (inq_pop(&m)) {
        printf("[MQTT:Q] %s: %.*s\n", m.topic, (int)m.payload_len, m.payload);

        mqtt_routes_dispatch(m.topic, m.payload, m.payload_len);
        inq_free(&m);
    }

    return NULL;
}

int mqtt_inbound_start(void) {
    if (inq.running) return 0;

    inq.running = 1;

    int rc = pthread_create(&inq.th, NULL, in_worker, NULL);

    if (rc != 0) {
        inq.running = 0;
        return -1;
    }

    return 0;
}

void mqtt_inbound_stop(void) {
    if (!inq.running) return;
    
    pthread_mutex_lock(&inq.mtx);

    inq.running = 0;
    
    pthread_cond_broadcast(&inq.cv);
    pthread_mutex_unlock(&inq.mtx);
    pthread_join(inq.th, NULL);

    while (inq.head != inq.tail) {
        struct InMsg m = inq.buf[inq.head];
        inq.head = (inq.head + 1) % IN_Q_CAP;
        inq_free(&m);
    }
}
