#include "config.h"
#include "config_types.h"
#include "logger.h"
#include "utils.h"

#include "cJSON.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>


typedef enum {
    SOURCE_NONE = 0,
    SOURCE_ENV,
    SOURCE_CONFIG,
    SOURCE_DEFAULT
} config_source_t;


// TODO: Add checks for the length of the string to make sure it is truncating. If there is return and error so we can fail.
static void cfg_set_str_from_env_json_default(
    char *dst, size_t dst_size, 
    const cJSON *root, 
    const char *json_key, 
    const char *env_name, 
    const char *default_value, 
    const char *label, 
    bool is_secret
) {
    const char *value = NULL;
    config_source_t source = SOURCE_NONE;

    if (env_name) {
        const char *env_val = getenv(env_name);
    
        if (env_val && env_val[0] != '\0') {
            value = env_val;
            source = SOURCE_ENV;
        }
    }

    if (!value && root && json_key) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, json_key);
        if (cJSON_IsString(item) && item->valuestring && item->valuestring[0] != '\0') {
            value = item->valuestring;
            source = SOURCE_CONFIG;
        }
    }

    if (!value && default_value) {
        value = default_value;
        source = SOURCE_DEFAULT;
    }

    if (value) {
        snprintf(dst, dst_size, "%s", value);
    } else {
        if (dst_size > 0) {
            dst[0] = '\0';
        }
    }

    const char *name = label ? label : (json_key ? json_key : (env_name ? env_name : "(unnamed)"));

    if (is_secret) {
        if (source == SOURCE_ENV) {
            LOG_DEBUG("%s configured from environment variable '%s'.", name, env_name);
        } else if (source == SOURCE_CONFIG) {
            LOG_DEBUG("%s configured from JSON key '%s'.", name, json_key);
        } else {
            LOG_DEBUG("%s configured from default value.", name);
        }
    } else {
        if (source == SOURCE_ENV) {
            LOG_DEBUG("%s='%s' (from env '%s').", name, value, env_name);
        } else if (source == SOURCE_CONFIG) {
            LOG_DEBUG("%s='%s' (from JSON key '%s').", name, value, json_key);
        } else {
            LOG_DEBUG("%s='%s' (from default).", name, value);
        }
    }
}

static int cfg_get_int_from_env_json_default(const cJSON *root, const char *json_key, const char *env_name, int default_value) {
    const char *env_val = getenv(env_name);
    if (env_val && env_val[0] != '\0') {
        char *end = NULL;
        long v = strtol(env_val, &end, 10);
        if (end && *end == '\0') {
            return (int)v;
        }
    }

    if (root && json_key) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, json_key);
        if (cJSON_IsNumber(item)) {
            return item->valueint;
        }
    }

    return default_value;
}

static void config_load_mqtt(config_mqtt_t *mqtt_cfg, const cJSON *root) {
    char host[40];
    
    cfg_set_str_from_env_json_default(host, sizeof(host), root, "host", "MQTT_HOST", "localhost", "host", false);
    
    mqtt_cfg->tls_enabled = cfg_get_int_from_env_json_default(root, "tls_enabled", "MQTT_TLS_ENABLED", 0);

    int port = cfg_get_int_from_env_json_default(root, "port", "MQTT_PORT", 1883);

    char proto[8];
    snprintf(proto, sizeof(proto), "%s", mqtt_cfg->tls_enabled ? "ssl" : "tcp");

    snprintf(mqtt_cfg->address, sizeof(mqtt_cfg->address), "%s://%s:%d", proto, host, port);
    
    cfg_set_str_from_env_json_default(mqtt_cfg->prefix, sizeof(mqtt_cfg->prefix), root, "prefix", "MQTT_PREFIX", "chrishansentech", "prefix", false);
    cfg_set_str_from_env_json_default(mqtt_cfg->instance, sizeof(mqtt_cfg->instance), root, "instance", "MQTT_INSTANCE", "default", "instance", false);
    to_human_readable(mqtt_cfg->instance, mqtt_cfg->instance_human, sizeof(mqtt_cfg->instance_human));
    

    char default_client_id[256];
    snprintf(default_client_id, sizeof(default_client_id), "%s-doorbell-mqtt-unifi-%s", mqtt_cfg->prefix, mqtt_cfg->instance);

    cfg_set_str_from_env_json_default(mqtt_cfg->client_id, sizeof(mqtt_cfg->client_id), NULL, NULL, "MQTT_CLIENT_ID", default_client_id, "MQTT_CLIENT_ID", false);

    cfg_set_str_from_env_json_default(mqtt_cfg->username, sizeof(mqtt_cfg->username), root, "username", "MQTT_USERNAME", "", "username", false);
    cfg_set_str_from_env_json_default(mqtt_cfg->password, sizeof(mqtt_cfg->password), root, "password", "MQTT_PASSWORD", "", "password", true);

    mqtt_cfg->qos = cfg_get_int_from_env_json_default(root, "qos", "MQTT_QOS", 1);
    mqtt_cfg->keepalive = cfg_get_int_from_env_json_default(root, "keepalive", "MQTT_KEEPALIVE", 30);
    mqtt_cfg->clean_session = cfg_get_int_from_env_json_default(root, "clean_session", "MQTT_CLEAN_SESSION", 1);
    mqtt_cfg->retained_online = cfg_get_int_from_env_json_default(root, "retained_online", "MQTT_RETAINED_ONLINE", 1);

    cfg_set_str_from_env_json_default(mqtt_cfg->cafile, sizeof(mqtt_cfg->cafile), root, "cafile", "MQTT_CAFILE", "", "cafile", false);
    cfg_set_str_from_env_json_default(mqtt_cfg->certfile, sizeof(mqtt_cfg->certfile), root, "certfile", "MQTT_CERT_FILE", "", "certfile", false);
    cfg_set_str_from_env_json_default(mqtt_cfg->keyfile, sizeof(mqtt_cfg->keyfile), root, "keyfile", "MQTT_KEY_FILE", "", "keyfile", false);
    cfg_set_str_from_env_json_default(mqtt_cfg->keypass, sizeof(mqtt_cfg->keypass), root, "keypass", "MQTT_KEYPASS", "", "keypass", true);

    LOG_DEBUG("MQTT address: %s", mqtt_cfg->address);
    LOG_DEBUG("MQTT prefix: '%s', instance: '%s' (human: '%s')",
              mqtt_cfg->prefix,
              mqtt_cfg->instance,
              mqtt_cfg->instance_human);

    LOG_DEBUG("MQTT QoS=%d keepalive=%d clean_session=%d retained_online=%d",
              mqtt_cfg->qos,
              mqtt_cfg->keepalive,
              mqtt_cfg->clean_session,
              mqtt_cfg->retained_online);
}

static void config_load_ssh(config_ssh_t *ssh_cfg, const cJSON *root) {
    cfg_set_str_from_env_json_default(ssh_cfg->host, sizeof(ssh_cfg->host), root, "host", "ssh_host", "localhost", "host", false);
    
    ssh_cfg->port = cfg_get_int_from_env_json_default(root, "port", "ssh_port", 22);

    cfg_set_str_from_env_json_default(ssh_cfg->user, sizeof(ssh_cfg->user), root, "user", "ssh_user", "ubnt", "user", false);

    cfg_set_str_from_env_json_default(ssh_cfg->password_env, sizeof(ssh_cfg->password_env), root, "password_env", NULL, NULL, "UNIFI_PROTECT_RECOVERY_CODE", false);

}

void config_load_holidays(config_holiday_t *holiday_cfg, const cJSON *root) {
    if(!holiday_cfg || !root) {
        return;
    }

    holiday_cfg->items = NULL;
    holiday_cfg->count = 0;

    size_t count = cJSON_GetArraySize(root);

    if (count <= 0) {
        LOG_WARN("No holidays configured in the 'holiday' section");
        return;
    }

    config_holiday_item_t *items = calloc((size_t)count, sizeof(config_holiday_item_t));
    if (!items) {
        LOG_ERROR("Out of memory allocating holiday mappings (count=%zu)", count);
        return;
    }
    
    size_t i = 0, out = 0;
    for (cJSON *item = root->child; item && i < count; item = item->next, i++) {
        const char *name = item->string;
        const char *directory = cJSON_GetStringValue(item);

        if (!name || !directory) {
            LOG_WARN("Skipping invalid holiday entry at index %ld (missing name or directory).", i);
            continue;
        }

        char *h = strdup(name);
        char *d = strdup(directory);
        if (!h || !d) {
            free(h);
            free(d);
            LOG_ERROR("Out of memory duplicating holiday mapping at index %zu", i);
            continue;
        }

        items[out].holiday = h;
        items[out].directory = d;
        out++;
    }

    holiday_cfg->items = items;
    holiday_cfg->count = out;

    LOG_INFO("Loaded %ld holiday mappings from configuration.", holiday_cfg->count);
}

bool config_load(const char *filename, config_t *cfg) {
    if (!filename || !cfg) {
        LOG_ERROR("config_load: invalid parameters: filename=%p cfg=%p", (void*)filename, (void*)cfg);
        return false;
    }

    LOG_INFO("Loading configuration file: %s", filename);

    char *json_buffer = NULL;
    if (!utils_read_file(filename, &json_buffer, NULL)) {
        LOG_ERROR("Failed to read configuration file: %s", filename);
        return false;
    }

    const char *error_ptr = NULL;

    cJSON *root = cJSON_ParseWithOpts(json_buffer, &error_ptr, 0);
    
    if (!root) { 
        LOG_ERROR("JSON parsing error in %s before: %s", filename, error_ptr ? error_ptr : "(unknown position)");
        free(json_buffer);
        return false;
    }

    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (!mqtt || !cJSON_IsObject(mqtt)) {
        LOG_ERROR("Missing or invalid 'mqtt' section in '%s'", filename);
        cJSON_Delete(root);
        free(json_buffer);
        return false;
    }

    config_load_mqtt(&cfg->mqtt_cfg, mqtt);


    cJSON *ssh = cJSON_GetObjectItem(root, "ssh");
    if (!ssh || !cJSON_IsObject(ssh)) {
        LOG_ERROR("Missing or invalid 'ssh' section in '%s'", filename);
        cJSON_Delete(root);
        free(json_buffer);
        return false;
    }
    
    config_load_ssh(&cfg->ssh_cfg, ssh);

    cJSON *holidays = cJSON_GetObjectItem(root, "holidays");
    if (!holidays || !cJSON_IsObject(holidays)) {
        LOG_ERROR("Missing or invalid 'holidays' section in '%s'", filename);
        cJSON_Delete(root);
        free(json_buffer);
        return false;
    }

    config_load_holidays(&cfg->holiday_cfg, holidays);

    cJSON_Delete(root);
    free(json_buffer);

    LOG_INFO("Configuration loaded successfully from %s", filename);
    return true;
}

void config_free(config_t *cfg) {
    if (!cfg) {
        return;
    }

    for (size_t i = 0; i < cfg->holiday_cfg.count; i++) {
        free(cfg->holiday_cfg.items[i].holiday);
        free(cfg->holiday_cfg.items[i].directory);
    }

    free(cfg->holiday_cfg.items);

    cfg->holiday_cfg.items = NULL;
    cfg->holiday_cfg.count = 0;
}