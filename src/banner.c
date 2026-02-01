#include "banner.h"
#include "logger.h"
#include "version.h"

void print_banner(void) {
    LOG_INFO("UniFi Doorbell MQTT Service v%s", APP_VERSION);
    LOG_INFO("ChrisHansenTech — https://chrishansen.tech");
    LOG_INFO("Source: https://github.com/ChrisHansenTech/doorbell-mqtt-unifi");
    LOG_INFO("License: MIT");
}