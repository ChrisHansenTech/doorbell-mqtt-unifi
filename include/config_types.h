#pragma once

#include <linux/limits.h>
#include <stdbool.h>
#include <stddef.h>

#define DEFAULT_PROFILE_NAME "Default"

typedef enum {
    UNIFI_APPLY_LEGACY = 0,
    UNIFI_APPLY_IPC
} unifi_apply_method_t;

typedef struct {
    char address[256];
    char client_id[256];
    char username[128];
    char password[128];
    int  qos;
    int  keepalive;
    int  clean_session;
    int  retained_online;

    int  tls_enabled;
    char cafile[256];
    char certfile[256];
    char keyfile[256];
    char keypass[128];

    char prefix[30];
    char instance[64];
    char instance_human[64];
} config_mqtt_t;

typedef struct {
    char host[256];
    int port;
    char user[30];
    char password_env[50];
} config_ssh_t;

typedef struct {
    char *display_name;
    char *key_name;
    char *directory;
} config_preset_item_t;

typedef struct {
    config_preset_item_t *items;
    size_t count;
} config_preset_t;

typedef struct {
    char *name;
    char *key_name;
    char *file;
    char *dir;
    int volume;
} config_sfx_preset_item_t;

typedef struct {
    config_sfx_preset_item_t *items;
    size_t count;
    int default_volume;
} config_sfx_preset_t;

typedef struct {
    unifi_apply_method_t apply_method;
} config_unifi_t;

typedef struct {
    bool enabled;
    char file[256];
    int count;
    int duration_ms;
    bool loop;
} config_default_welcome_t;

typedef struct {
    bool enabled;
    char file[256];
    int repeat_times;
    int volume;
} config_default_ring_button_t;

typedef struct {
    bool enabled;
    char directory[PATH_MAX];
    config_default_welcome_t welcome;
    config_default_ring_button_t ring_button;
} config_default_assets_t;

static inline bool config_default_assets_available(const config_default_assets_t *cfg) {
    return cfg && cfg->enabled && (cfg->welcome.enabled || cfg->ring_button.enabled);
}

typedef struct {
    config_mqtt_t mqtt_cfg;
    config_ssh_t ssh_cfg;
    config_preset_t preset_cfg;
    config_sfx_preset_t sfx_preset_cfg;
    config_unifi_t unifi_cfg;
    config_default_assets_t default_assets_cfg;
} config_t;
