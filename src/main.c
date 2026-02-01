
#include "banner.h"
#include "config.h"
#include "config_types.h"
#include "ha_mqtt.h"
#include "logger.h"
#include "mqtt.h"
#include "mqtt_router.h"
#include "ha_topics.h"
#include "mqtt_router_types.h"
#include "unifi_profiles_repo.h"
#include "utils.h"

#include <stdbool.h>
#include <signal.h>

static volatile int running = 1;
static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    log_init(NULL);

    print_banner();

    if (!utils_delete_directory("./tmp")) {
        LOG_ERROR("Error cleaning '.tmp'");
    }

    config_t cfg = {0};
    bool mqtt_initialized = false;
    bool mqtt_router_started = false;
    int rc = 0;

    if (!config_load("config.json", &cfg)) {
        LOG_FATAL("Configuration load failed. Exiting.");
        rc = 1;
        goto cleanup;
    }

    if(!profiles_repo_init("./profiles", &cfg.holiday_cfg)) {
        LOG_FATAL("Profiles initialization failed. Exiting.");
        rc = 1;
        goto cleanup;
    }

    ha_topics_init(&cfg);

    if (!ha_mqtt_bind(&cfg)) {
        LOG_FATAL("Home Assistant MQTT bind failed. Exiting.");
        rc = 1;
        goto cleanup;
    }

    if (!mqtt_init(&cfg.mqtt_cfg)) {
        LOG_FATAL("MQTT initialization failed. Exiting.");
        rc = 1;
        goto cleanup;
    }

    mqtt_initialized = true;

    mqtt_router_ctx_t inbound_ctx;
    inbound_ctx.ssh_cfg = &cfg.ssh_cfg;
    inbound_ctx.holiday_cfg = &cfg.holiday_cfg;

    if (!mqtt_router_start(&inbound_ctx)) {
        LOG_FATAL("MQTT inbound worker failed to start. Exiting.");
        rc = 1;
        goto cleanup;
    }

    mqtt_router_started = true;

    ha_topic_subscribe_commands();

    while (running) {
        mqtt_loop(100);
    }

    LOG_INFO("Shutdown requested, stopping service...");

cleanup:
    if (mqtt_router_started) {
        mqtt_router_stop();
    }

    if (mqtt_initialized) {
        mqtt_disconnect();
    }

    profiles_repo_shutdown();
    config_free(&cfg);
    
    LOG_INFO("Service stopped cleanly.");
    
    return rc;
}
