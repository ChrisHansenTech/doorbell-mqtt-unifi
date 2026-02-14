
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
#include <stdlib.h>

#define DEFAULT_CONFIG_PATH   "/config/config.json"
#define DEFAULT_PROFILES_DIR  "/profiles"

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

    const char *config_path = getenv("CONFIG_PATH");
    if (!config_path) {
        config_path = DEFAULT_CONFIG_PATH;
    }

    const char *profiles_dir = getenv("PROFILES_DIR");
    if (!profiles_dir) {
        profiles_dir = DEFAULT_PROFILES_DIR;
    }

    if (!utils_delete_directory("/tmp/doorbell-mqtt-unifi")) {
        LOG_WARN("Did not clean '/tmp/doorbell-mqtt-unifi'");
    }

    config_t cfg = {0};
    bool mqtt_initialized = false;
    bool mqtt_router_started = false;
    int rc = 0;

    if (!config_load(config_path, &cfg)) {
        LOG_FATAL("Configuration load failed. Exiting.");
        rc = 1;
        goto cleanup;
    }

    if(!profiles_repo_init(profiles_dir, &cfg.preset_cfg)) {
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
    inbound_ctx.preset_cfg = &cfg.preset_cfg;

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
