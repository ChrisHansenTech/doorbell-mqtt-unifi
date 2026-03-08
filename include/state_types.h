#pragma once

#include "config_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

typedef struct {
    int count;
    int duration_ms;
    bool enable;
    char *file;
    char *gui_id;
    bool loop;
} profile_animation_t;

typedef struct {
    bool enable;
    char *file;
    int repeat_times;
    char *sound_state_name;
    int volume;
} profile_sound_t;

typedef struct {
    profile_animation_t *custom_animations;
    size_t custom_animation_count;
    profile_sound_t *custom_sounds;
    size_t custom_sound_count;
} profile_state_t;

typedef struct {
    char *profile_name;
    bool is_preset;
    unifi_apply_method_t apply_method;
    time_t applied_at;
    profile_state_t state;
    char *hash;
} applied_state_t;