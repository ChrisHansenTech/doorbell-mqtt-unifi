#pragma once

#include <stdbool.h>
#include <time.h>

typedef struct {
    bool enabled;
    char file[256];
    int count;
    int duration_ms;
    bool loop;
    char gui_id[32];          // optional: "WELCOME"
} unifi_profile_welcome_t;

typedef struct {
    bool enabled;
    char file[256];
    int repeat_times;
    int volume;
    char sound_state_name[64]; // optional: "RING_BUTTON_PRESSED"
} unifi_profile_ring_button_t;

typedef struct {
    int schema_version;
    unifi_profile_welcome_t welcome;
    unifi_profile_ring_button_t ring_button;
} unifi_profile_t;

typedef struct {
    char profile_name[256];
    bool is_preset;
    time_t appied_at;
} unifi_last_applied_profile_t;