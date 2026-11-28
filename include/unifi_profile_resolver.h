#pragma once

#include "config_types.h"
#include "errors.h"
#include "unifi_profile.h"

#include <linux/limits.h>
#include <stdbool.h>

typedef struct {
    unifi_profile_t profile;
    char welcome_dir[PATH_MAX];
    char ring_button_dir[PATH_MAX];
    bool welcome_is_default;
    bool ring_button_is_default;
} unifi_effective_profile_t;

bool unifi_profile_resolve_effective(
    const unifi_profile_t *source_profile,
    const char *profile_dir,
    const config_default_assets_t *default_assets,
    unifi_effective_profile_t *out
);

bool unifi_profile_build_default_effective(
    const config_default_assets_t *default_assets,
    unifi_effective_profile_t *out
);

error_return_t unifi_effective_profile_validate_assets(const unifi_effective_profile_t *effective);
