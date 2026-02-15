#include "ha_status.h"
#include "cJSON.h"
#include "errors.h"
#include "logger.h"
#include "mqtt.h"
#include "ha_topics.h"
#include <linux/limits.h>
#include <string.h>
#include <strings.h>

static int g_last_error_code = 0;
static char g_last_error_message[256] = { '\0'};

void status_set_state(const char *state) {

    if (!state) {
        return;
    }

    char buffer[256];
    ha_build_topic(buffer, sizeof(buffer), "status");

    mqtt_publish(buffer, state, 1, 1);
}

void status_set_status_message(const char *topic, const char *message) {
    if (!topic || !message) {
        return;
    }

    char buffer[256];
    ha_build_topic(buffer, sizeof(buffer), topic);

    mqtt_publish(buffer, message, 1, 1);
}

void status_set_error(error_code_t code, const char *message_override) {
    const char *name = error_code_name(code);
    const char *msg  = message_override ? message_override : error_code_default_message(code);
    
    if (!msg) {
        msg = "Unknown error";
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LOG_ERROR("Failed to allocate cJSON object for last_error.");
        return;
    }

    cJSON_AddNumberToObject(root, "code", code);
    cJSON_AddStringToObject(root, "name", name);
    cJSON_AddStringToObject(root, "message", msg);

    char *json = cJSON_PrintUnformatted(root);
    
    if (json) {
        char topic[256];
        ha_build_topic(topic, sizeof(topic), "last_error");
        mqtt_publish(topic, json, 1, 1);

        cJSON_free(json);
    } else {
        LOG_ERROR("Failed to serialize last_error JSON.");
    }

    cJSON_free(json);
    cJSON_Delete(root);
}

bool status_error_changed(error_code_t code, const char *message)
{
    if (!message) {
        return true;
    }

    if ((int)code != g_last_error_code) {
        return true;
    }

    if (strcmp(message, g_last_error_message) != 0) {
        return true;
    }

    return false;
}

void status_set_last_applied_profile(const char *profile) {
    char buffer[255];

    ha_build_topic(buffer, sizeof(buffer), "last_applied_profile");

    mqtt_publish(buffer, profile, 1, 1);
}

void status_set_preset_selected(const char *preset) {
    char buffer[255];

    ha_build_topic(buffer, sizeof(buffer), "preset/selected");

    mqtt_publish(buffer, preset, 1, 1);
}

void status_set_custom_directory(const char *directory) {
    char buffer[255];

    ha_build_topic(buffer, sizeof(buffer), "custom/directory");

    mqtt_publish(buffer, directory, 1, 1);
}

void status_set_last_download(const char *directory, const char *path, const char *timestamp) {
    if (!directory || !path || !timestamp) {
        LOG_ERROR("Invalid parameters: directory=%p, path=%p, timestamp=%p", (void*)directory, (void*)path, (void*)timestamp);
        return;
    }

    char state_topic[256];
    ha_build_topic(state_topic, sizeof(state_topic), "last_download/time/state");
    mqtt_publish(state_topic, timestamp, 1, 1);

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LOG_ERROR("Failed to allocate cJSON object for 'last_download/time/attributes'");
        return;
    }

    cJSON_AddStringToObject(root, "directory", directory);
    cJSON_AddStringToObject(root, "full_path", path);

    char *json = cJSON_PrintUnformatted(root);
    
    if (json) {
        char json_topic[256];
        ha_build_topic(json_topic, sizeof(json_topic), "last_download/time/attributes");
        mqtt_publish(json_topic, json, 1, 1);

        cJSON_free(json);
    } else {
        LOG_ERROR("Failed to serialize 'last_download/time/attributes' JSON.");
    }

    cJSON_Delete(root);
}

void status_set_availability(bool available) {

    char buffer[255];
    ha_build_topic(buffer, sizeof(buffer), "availability");

    mqtt_publish(buffer, available ? "online" : "offline", 1, 1);

    return;
}