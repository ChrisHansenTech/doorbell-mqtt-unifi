#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "mqtt.h"

static volatile int running = 1;
static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    printf("[INFO] Starting UniFi Doorbell MQTT…\n");

    MqttConfig cfg;
    if (mqtt_load_from_env(&cfg) != 0) {
        fprintf(stderr, "[ERROR] Failed to load MQTT env\n");
        return 1;
    }

    if (mqtt_init(&cfg) != 0) {
        fprintf(stderr, "[ERROR] MQTT init failed\n");
        return 1;
    }

    mqtt_subscribe(cfg.topic_set);

    while (running) {
        mqtt_loop(100); // 100ms sleep; callbacks handle messages
    }

    mqtt_disconnect();
    printf("[INFO] Stopped cleanly.\n");
    return 0;
}
