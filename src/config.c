#include "config.h"
#include "config_types.h"
#include "errors.h"
#include "logger.h"
#include "utils.h"

#include "cJSON.h"
#include "utils_json.h"

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


static bool cfg_set_str_from_env_json_default(
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

    const char *name = label ? label : (json_key ? json_key : (env_name ? env_name : "(unnamed)"));

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
        int written = snprintf(dst, dst_size, "%s", value);

        if (written < 0 || (size_t)written >= dst_size) {
            LOG_ERROR("Configuration value for '%s' exceeds maximum length (%zu).",name, dst_size - 1);
            return false;
        }
    } else {
        if (dst_size > 0) {
            dst[0] = '\0';
        }
    }

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

    if (source == SOURCE_DEFAULT && dst && dst[0] != '\0') {

        if (strcmp(name, "mqtt.host") == 0 && strcmp(dst, "localhost") == 0) {
            LOG_WARN("%s not set; defaulting to '%s'. "
                     "When running in Docker, '%s' refers to the container itself. "
                     "Set MQTT_HOST or update /config/config.json and restart.", name, dst, dst);
        }

        if (strcmp(name, "ssh.host") == 0 && strcmp(dst, "localhost") == 0) {
            LOG_WARN("ssh.host not set; defaulting to '%s'. "
                     "Set SSH_HOST or update /config/config.json and restart.", dst);
        }
    }

    return true;
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

static bool preset_key_exists(const config_preset_item_t *items, size_t count, const char *key) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(items[i].key_name, key) == 0) return true;
    }
    return false;
}

static bool config_load_mqtt(config_mqtt_t *mqtt_cfg, const cJSON *root) {
    char host[40];
    
    if (!cfg_set_str_from_env_json_default(host, sizeof(host), root, "host", "MQTT_HOST", "localhost", "mqtt.host", false)) {
        return false;
    }
    
    mqtt_cfg->tls_enabled = cfg_get_int_from_env_json_default(root, "tls_enabled", "MQTT_TLS_ENABLED", 0);

    int port = cfg_get_int_from_env_json_default(root, "port", "MQTT_PORT", 1883);

    char proto[8];
    snprintf(proto, sizeof(proto), "%s", mqtt_cfg->tls_enabled ? "ssl" : "tcp");

    snprintf(mqtt_cfg->address, sizeof(mqtt_cfg->address), "%s://%s:%d", proto, host, port);
    
    if (!cfg_set_str_from_env_json_default(mqtt_cfg->prefix, sizeof(mqtt_cfg->prefix), root, "prefix", "MQTT_PREFIX", "chrishansentech", "mqtt.prefix", false) ||
        !cfg_set_str_from_env_json_default(mqtt_cfg->instance, sizeof(mqtt_cfg->instance), root, "instance", "MQTT_INSTANCE", "default", "mqtt.instance", false)) {
        return false;
    }

    to_human_readable(mqtt_cfg->instance, mqtt_cfg->instance_human, sizeof(mqtt_cfg->instance_human));
    
    char default_client_id[256];
    snprintf(default_client_id, sizeof(default_client_id), "%s-doorbell-mqtt-unifi-%s", mqtt_cfg->prefix, mqtt_cfg->instance);

    if (!cfg_set_str_from_env_json_default(mqtt_cfg->client_id, sizeof(mqtt_cfg->client_id), NULL, NULL, "MQTT_CLIENT_ID", default_client_id, "mqtt.client_id", false) ||
        !cfg_set_str_from_env_json_default(mqtt_cfg->username, sizeof(mqtt_cfg->username), root, "username", "MQTT_USERNAME", "", "mqtt.username", false) ||
        !cfg_set_str_from_env_json_default(mqtt_cfg->password, sizeof(mqtt_cfg->password), root, "password", "MQTT_PASSWORD", "", "mqtt.password", true)) {
        return false;
    }

    mqtt_cfg->qos = cfg_get_int_from_env_json_default(root, "qos", "MQTT_QOS", 1);
    mqtt_cfg->keepalive = cfg_get_int_from_env_json_default(root, "keepalive", "MQTT_KEEPALIVE", 30);
    mqtt_cfg->clean_session = cfg_get_int_from_env_json_default(root, "clean_session", "MQTT_CLEAN_SESSION", 1);
    mqtt_cfg->retained_online = cfg_get_int_from_env_json_default(root, "retained_online", "MQTT_RETAINED_ONLINE", 1);

    if (!cfg_set_str_from_env_json_default(mqtt_cfg->cafile, sizeof(mqtt_cfg->cafile), root, "cafile", "MQTT_CAFILE", "", "mqtt.cafile", false) ||
        !cfg_set_str_from_env_json_default(mqtt_cfg->certfile, sizeof(mqtt_cfg->certfile), root, "certfile", "MQTT_CERT_FILE", "", "mqtt.certfile", false) ||
        !cfg_set_str_from_env_json_default(mqtt_cfg->keyfile, sizeof(mqtt_cfg->keyfile), root, "keyfile", "MQTT_KEY_FILE", "", "mqtt.keyfile", false) ||
        !cfg_set_str_from_env_json_default(mqtt_cfg->keypass, sizeof(mqtt_cfg->keypass), root, "keypass", "MQTT_KEYPASS", "", "mqtt.keypass", true)) {
        return false;
    }

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

    
    return true;
}

static bool config_load_ssh(config_ssh_t *ssh_cfg, const cJSON *root) {
    if (!cfg_set_str_from_env_json_default(ssh_cfg->host, sizeof(ssh_cfg->host), root, "host", "SSH_HOST", "localhost", "ssh.host", false) ||
        !cfg_set_str_from_env_json_default(ssh_cfg->user, sizeof(ssh_cfg->user), root, "username", "SSH_USERNAME", "ubnt", "ssh.username", false) ||
        !cfg_set_str_from_env_json_default(ssh_cfg->password_env, sizeof(ssh_cfg->password_env), root, "password_env", NULL, "UNIFI_PROTECT_RECOVERY_CODE", "ssh.password_env", false)) {
        return false;
    }

    ssh_cfg->port = cfg_get_int_from_env_json_default(root, "port", "SSH_PORT", 22);

    return true;
}

static bool config_load_unifi(config_unifi_t *unifi_cfg, const cJSON *unifi) {
    char type[30];

    if (!cfg_set_str_from_env_json_default(type, sizeof(type), unifi, "apply_method", "UNIFI_APPLY_METHOD", "IPC", "unifi.apply_method", false)) {
        return false;
    }

    if (strcasecmp(type, "legacy") == 0) {
        unifi_cfg->apply_method = UNIFI_APPLY_LEGACY;
        return true;
    }

    if (strcasecmp(type, "ipc") == 0) {
        unifi_cfg->apply_method = UNIFI_APPLY_IPC;
        return true;
    }

    return false;
}

static void config_load_presets(config_preset_t *preset_cfg, const cJSON *presets) {
    if(!preset_cfg || !presets) {
        return;
    }

    preset_cfg->items = NULL;
    preset_cfg->count = 0;

    size_t count = cJSON_GetArraySize(presets);

    if (count <= 0) {
        LOG_WARN("No presets configured in the 'presets' section");
        return;
    }

    config_preset_item_t *items = calloc((size_t)count, sizeof(config_preset_item_t));
    if (!items) {
        LOG_ERROR("Out of memory allocating preset mappings (count=%zu)", count);
        return;
    }

    size_t i = 0, out = 0;
    for (cJSON *item = presets->child; item && i < count; item = item->next, i++) {
        const char *name = json_get_string(item, "name");
        const char *directory = json_get_string(item, "directory");
        
        if (!name || !directory) {
            LOG_WARN("Invalid preset entry at index %ld (missing name or directory).", i);
            continue;
        }

        if (!utils_is_valid_directory_name(directory)) {
            LOG_WARN("Invalid preset directory name '%s' for preset '%s'", directory, name);
            continue;
        }
        
        char *key_name = json_strdup_normalized(item, "name");

        if (preset_key_exists(items, out, key_name)) {
            LOG_WARN("Duplicate preset name (case/space-insensitive) '%s' at index %zu", name, i);
            free(key_name);
            continue;
        }

        char *display_name = strdup(name);
        char *dir = strdup(directory);
        if (!display_name || !dir) {
            free(key_name);
            LOG_ERROR("Out of memory duplicating preset mapping at index %zu", i);
            goto fail;
        }

        items[out].display_name = display_name;
        items[out].key_name = key_name;
        items[out].directory = dir;
        out++;
    }

    preset_cfg->items = items;
    preset_cfg->count = out;

    LOG_INFO("Loaded %ld preset mappings from configuration.", preset_cfg->count);
    return;

fail:
    for (size_t j = 0; j < out; j++) {
        free(items[j].display_name);
        free(items[j].key_name);
        free(items[j].directory);
    }
    free(items);

    preset_cfg->items = NULL;
    preset_cfg->count = 0;
    return;
}

static bool sfx_preset_key_exists(const config_sfx_preset_item_t *items, size_t count, const char *key) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(items[i].key_name, key) == 0) return true;
    }
    return false;
}

static void config_load_sfx_presets(config_sfx_preset_t *sfx_cfg, const cJSON *presets) {
    if(!sfx_cfg || !presets) {
        return;
    }

    sfx_cfg->items = NULL;
    sfx_cfg->count = 0;

    size_t count = cJSON_GetArraySize(presets);

    if (count <= 0) {
        LOG_WARN("No presets configured in the 'sfx.presets' section");
        return;
    }

    config_sfx_preset_item_t *items = calloc((size_t)count, sizeof(config_sfx_preset_item_t));
    if (!items) {
        LOG_ERROR("Out of memory allocating sfx preset mappings (count=%zu)", count);
        return;
    }

    size_t i = 0, out = 0;
    for (cJSON *item = presets->child; item && i < count; item = item->next, i++) {
        const char *name = json_get_string(item, "name");
        const char *file = json_get_string(item, "file");
        
        int volume = 0;
        if (!json_get_int(item, "volume", &volume) || volume <= 0 || volume > 100) {
            volume = sfx_cfg->default_volume;
        }
        
        if (!name || !file) {
            LOG_WARN("Invalid sfx preset entry at index %ld (missing name or file).", i);
            continue;
        }

        error_return_t err = utils_is_valid_filename(file, UTILS_FILE_CLASS_SOUND);
        if (err.error_code != ERROR_NONE) {
            LOG_WARN("Invalid file '%s' in SFX preset '%s'", file, name);
            continue;
        }
        
        char *key_name = json_strdup_normalized(item, "name");

        if (sfx_preset_key_exists(items, out, key_name)) {
            LOG_WARN("Duplicate preset name (case/space-insensitive) '%s' at index %zu", name, i);
            free(key_name);
            continue;
        }

        char *display_name = strdup(name);
        char *filename = strdup(file);
        if (!display_name || !filename) {
            free(key_name);
            LOG_ERROR("Out of memory duplicating sfx preset mapping at index %zu", i);
            goto fail;
        }

        items[out].name = display_name;
        items[out].key_name = key_name;
        items[out].file = filename;
        items[out].volume = volume;
        out++;
    }

    sfx_cfg->items = items;
    sfx_cfg->count = out;

    LOG_INFO("Loaded %ld sfx preset mappings from configuration.", sfx_cfg->count);
    return;

fail:
    for (size_t j = 0; j < out; j++) {
        free(items[j].name);
        free(items[j].key_name);
        free(items[j].file);
    }
    free(items);

    sfx_cfg->items = NULL;
    sfx_cfg->count = 0;
    return;


}

static bool config_load_sfx(config_sfx_preset_t *sfx_cfg, const cJSON *sfx) {
    if (!sfx_cfg || !sfx) {
        return false;
    }

    sfx_cfg->default_volume = cfg_get_int_from_env_json_default(sfx, "defaultVolume", "SFX_DEFAULT_VOLUME", 100);

    cJSON *presets = cJSON_GetObjectItem(sfx, "presets");

    if (presets && cJSON_IsArray(presets)) {
        config_load_sfx_presets(sfx_cfg, presets);
    }

    return true;
    
}

static bool json_get_optional_bool_strict(const cJSON *obj, const char *key, bool default_val, bool *out, const char *label) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) {
        *out = default_val;
        return true;
    }
    if (!cJSON_IsBool(item)) {
        LOG_ERROR("Configuration property '%s' must be a boolean.", label);
        return false;
    }
    *out = cJSON_IsTrue(item);
    return true;
}

static bool json_get_optional_int_strict(const cJSON *obj, const char *key, int default_val, int min_val, int max_val, int *out, const char *label) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) {
        *out = default_val;
        return true;
    }
    if (!cJSON_IsNumber(item)) {
        LOG_ERROR("Configuration property '%s' must be a number.", label);
        return false;
    }
    int v = item->valueint;
    if ((double)v != item->valuedouble || v < min_val || (max_val >= min_val && v > max_val)) {
        if (max_val >= min_val) {
            LOG_ERROR("Configuration property '%s' must be an integer between %d and %d (got %g).", label, min_val, max_val, item->valuedouble);
        } else {
            LOG_ERROR("Configuration property '%s' must be an integer >= %d (got %g).", label, min_val, item->valuedouble);
        }
        return false;
    }
    *out = v;
    return true;
}

static bool config_load_default_assets(config_default_assets_t *def_cfg, const config_preset_t *preset_cfg, const cJSON *root) {
    if (!def_cfg) {
        return false;
    }

    memset(def_cfg, 0, sizeof(*def_cfg));

    const cJSON *da = cJSON_GetObjectItemCaseSensitive(root, "defaultAssets");
    if (!da) {
        return true;
    }

    if (!cJSON_IsObject(da)) {
        LOG_ERROR("'defaultAssets' must be an object.");
        return false;
    }

    bool enabled = false;
    if (!json_get_optional_bool_strict(da, "enabled", false, &enabled, "defaultAssets.enabled")) {
        return false;
    }

    def_cfg->enabled = enabled;
    if (!enabled) {
        LOG_DEBUG("defaultAssets feature is disabled; ignoring inactive default asset settings.");
        return true;
    }

    /* Check for case-insensitive preset name conflict with reserved 'Default' name */
    if (preset_cfg) {
        for (size_t i = 0; i < preset_cfg->count; i++) {
            const char *disp = preset_cfg->items[i].display_name;
            const char *key = preset_cfg->items[i].key_name;
            if ((disp && strcasecmp(disp, DEFAULT_PROFILE_NAME) == 0) ||
                (key && strcasecmp(key, DEFAULT_PROFILE_NAME) == 0)) {
                LOG_ERROR("Cannot enable default assets: a user-defined preset named 'Default' already exists. "
                          "Rename the existing preset or disable the default assets feature.");
                return false;
            }
        }
    }

    /* Validate optional directory string type (required below if at least one asset is enabled) */
    const cJSON *dir_item = cJSON_GetObjectItemCaseSensitive(da, "directory");
    if (dir_item) {
        if (!cJSON_IsString(dir_item) || !dir_item->valuestring) {
            LOG_ERROR("Configuration property 'defaultAssets.directory' must be a string.");
            return false;
        }
        int written = snprintf(def_cfg->directory, sizeof(def_cfg->directory), "%s", dir_item->valuestring);
        if (written < 0 || (size_t)written >= sizeof(def_cfg->directory)) {
            LOG_ERROR("Configuration value for 'defaultAssets.directory' exceeds maximum length (%zu).",
                      sizeof(def_cfg->directory) - 1);
            return false;
        }
    }

    /* Parse optional defaultAssets.welcome */
    const cJSON *welcome = cJSON_GetObjectItemCaseSensitive(da, "welcome");
    if (welcome) {
        if (!cJSON_IsObject(welcome)) {
            LOG_ERROR("'defaultAssets.welcome' must be an object.");
            return false;
        }

        if (!json_get_optional_bool_strict(welcome, "enabled", false, &def_cfg->welcome.enabled, "defaultAssets.welcome.enabled")) {
            return false;
        }

        if (def_cfg->welcome.enabled) {
            const cJSON *file_item = cJSON_GetObjectItemCaseSensitive(welcome, "file");
            if (!cJSON_IsString(file_item) || !file_item->valuestring || file_item->valuestring[0] == '\0') {
                LOG_ERROR("'defaultAssets.welcome.file' is required when default welcome animation is enabled.");
                return false;
            }

            error_return_t ferr = utils_is_valid_filename(file_item->valuestring, UTILS_FILE_CLASS_ANIMATION);
            if (ferr.error_code != ERROR_NONE) {
                LOG_ERROR("Invalid 'defaultAssets.welcome.file' '%s': %s", file_item->valuestring, ferr.message);
                return false;
            }

            int written = snprintf(def_cfg->welcome.file, sizeof(def_cfg->welcome.file), "%s", file_item->valuestring);
            if (written < 0 || (size_t)written >= sizeof(def_cfg->welcome.file)) {
                LOG_ERROR("Configuration value for 'defaultAssets.welcome.file' exceeds maximum length.");
                return false;
            }

            if (!json_get_optional_int_strict(welcome, "count", 1, 1, -1, &def_cfg->welcome.count, "defaultAssets.welcome.count") ||
                !json_get_optional_int_strict(welcome, "durationMs", 3000, 1, -1, &def_cfg->welcome.duration_ms, "defaultAssets.welcome.durationMs") ||
                !json_get_optional_bool_strict(welcome, "loop", false, &def_cfg->welcome.loop, "defaultAssets.welcome.loop")) {
                return false;
            }
        }
    }

    /* Parse optional defaultAssets.ringButton */
    const cJSON *ring = cJSON_GetObjectItemCaseSensitive(da, "ringButton");
    if (ring) {
        if (!cJSON_IsObject(ring)) {
            LOG_ERROR("'defaultAssets.ringButton' must be an object.");
            return false;
        }

        if (!json_get_optional_bool_strict(ring, "enabled", false, &def_cfg->ring_button.enabled, "defaultAssets.ringButton.enabled")) {
            return false;
        }

        if (def_cfg->ring_button.enabled) {
            const cJSON *file_item = cJSON_GetObjectItemCaseSensitive(ring, "file");
            if (!cJSON_IsString(file_item) || !file_item->valuestring || file_item->valuestring[0] == '\0') {
                LOG_ERROR("'defaultAssets.ringButton.file' is required when default ring button sound is enabled.");
                return false;
            }

            error_return_t ferr = utils_is_valid_filename(file_item->valuestring, UTILS_FILE_CLASS_SOUND);
            if (ferr.error_code != ERROR_NONE) {
                LOG_ERROR("Invalid 'defaultAssets.ringButton.file' '%s': %s", file_item->valuestring, ferr.message);
                return false;
            }

            int written = snprintf(def_cfg->ring_button.file, sizeof(def_cfg->ring_button.file), "%s", file_item->valuestring);
            if (written < 0 || (size_t)written >= sizeof(def_cfg->ring_button.file)) {
                LOG_ERROR("Configuration value for 'defaultAssets.ringButton.file' exceeds maximum length.");
                return false;
            }

            if (!json_get_optional_int_strict(ring, "repeatTimes", 1, 1, -1, &def_cfg->ring_button.repeat_times, "defaultAssets.ringButton.repeatTimes") ||
                !json_get_optional_int_strict(ring, "volume", 100, 1, 100, &def_cfg->ring_button.volume, "defaultAssets.ringButton.volume")) {
                return false;
            }
        }
    }

    if ((def_cfg->welcome.enabled || def_cfg->ring_button.enabled) && def_cfg->directory[0] == '\0') {
        LOG_ERROR("'defaultAssets.directory' is required when at least one default asset is enabled.");
        return false;
    }

    LOG_INFO("Loaded defaultAssets configuration (enabled=true, dir='%s', welcome=%s, ringButton=%s).",
             def_cfg->directory,
             def_cfg->welcome.enabled ? "enabled" : "disabled",
             def_cfg->ring_button.enabled ? "enabled" : "disabled");
    return true;
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

    free(json_buffer);
    json_buffer = NULL;

    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (!mqtt || !cJSON_IsObject(mqtt)) {
        LOG_ERROR("Missing or invalid 'mqtt' section in '%s'", filename);
        cJSON_Delete(root);
        return false;
    }

    if (!config_load_mqtt(&cfg->mqtt_cfg, mqtt)) {
        return false;
    }

    cJSON *unifi = cJSON_GetObjectItem(root, "unifi");

    if (unifi) {
        if (!cJSON_IsObject(unifi)) {
            LOG_ERROR("'unifi' must be an object in '%s'", filename);
            cJSON_Delete(root);
            return false;
        }
    } else {
        LOG_WARN("Missing 'unifi' section in '%s'. Defaulting to 'legacy' apply method", filename);
        
        unifi = cJSON_AddObjectToObject(root, "unifi");
        if (!unifi) {
            LOG_ERROR("Failed to create default 'unifi' object");
            cJSON_Delete(root);
            return false;
        }

        cJSON_AddStringToObject(unifi, "apply_method", "legacy");
    }

    if (!config_load_unifi(&cfg->unifi_cfg, unifi)) {
        return false;
    }

    cJSON *ssh = cJSON_GetObjectItem(root, "ssh");
    if (!ssh || !cJSON_IsObject(ssh)) {
        LOG_ERROR("Missing or invalid 'ssh' section in '%s'", filename);
        cJSON_Delete(root);
        return false;
    }
    
    if (!config_load_ssh(&cfg->ssh_cfg, ssh)) {
        return false;
    }

    cJSON *presets = cJSON_GetObjectItem(root, "presets");
    if (!presets || !cJSON_IsArray(presets)) {
        LOG_WARN("Missing or invalid 'presets' section in '%s'", filename);
        presets = cJSON_AddArrayToObject(root, "presets");
    }

    config_load_presets(&cfg->preset_cfg, presets);

    cJSON *sfx = cJSON_GetObjectItem(root, "sfx");
    if (!sfx || !cJSON_IsObject(sfx)) {
        LOG_WARN("Missing or invalid 'sfx' section in '%s'.", filename);
        sfx = cJSON_AddObjectToObject(root, "sfx");
        cJSON_AddNumberToObject(sfx, "defaultVolume", 100);
        cJSON_AddArrayToObject(sfx, "presets");
    }

    config_load_sfx(&cfg->sfx_preset_cfg, sfx);

    if (!config_load_default_assets(&cfg->default_assets_cfg, &cfg->preset_cfg, root)) {
        cJSON_Delete(root);
        config_free(cfg);
        return false;
    }

    cJSON_Delete(root);

    LOG_INFO("Configuration loaded successfully from %s", filename);
    return true;
}

void config_free(config_t *cfg) {
    if (!cfg) {
        return;
    }

    for (size_t i = 0; i < cfg->preset_cfg.count; i++) {
        free(cfg->preset_cfg.items[i].display_name);
        free(cfg->preset_cfg.items[i].key_name);
        free(cfg->preset_cfg.items[i].directory);
    }

    free(cfg->preset_cfg.items);

    cfg->preset_cfg.items = NULL;
    cfg->preset_cfg.count = 0;

    for (size_t i = 0; i < cfg->sfx_preset_cfg.count; i++) {
        free(cfg->sfx_preset_cfg.items[i].name);
        free(cfg->sfx_preset_cfg.items[i].key_name);
        free(cfg->sfx_preset_cfg.items[i].file);
    }

    free(cfg->sfx_preset_cfg.items);

    cfg->sfx_preset_cfg.items = NULL;
    cfg->sfx_preset_cfg.count = 0;
}