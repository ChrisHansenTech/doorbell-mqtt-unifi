#pragma once

#include <stddef.h>

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
    config_mqtt_t mqtt_cfg;
    config_ssh_t ssh_cfg;
    config_preset_t preset_cfg;
} config_t;
