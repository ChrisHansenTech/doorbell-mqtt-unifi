#include "unifi_profile_resolver.h"
#include "logger.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

static bool copy_dir_str(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) {
        return false;
    }
    if (!src) {
        dst[0] = '\0';
        return true;
    }
    int n = snprintf(dst, dst_size, "%s", src);
    return n >= 0 && (size_t)n < dst_size;
}

bool unifi_profile_resolve_effective(
    const unifi_profile_t *source_profile,
    const char *profile_dir,
    const config_default_assets_t *default_assets,
    unifi_effective_profile_t *out
) {
    if (!source_profile || !out) {
        LOG_ERROR("Invalid parameters: source_profile=%p out=%p", (void *)source_profile, (void *)out);
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->profile = *source_profile;

    if (!copy_dir_str(out->welcome_dir, sizeof(out->welcome_dir), profile_dir) ||
        !copy_dir_str(out->ring_button_dir, sizeof(out->ring_button_dir), profile_dir)) {
        LOG_ERROR("Profile directory path exceeds maximum length.");
        return false;
    }

    /* Rule 1-3 for Welcome animation (resolved independently of ringButton) */
    if (!source_profile->welcome.enabled &&
        default_assets &&
        default_assets->enabled &&
        default_assets->welcome.enabled) {
        memset(&out->profile.welcome, 0, sizeof(out->profile.welcome));
        out->profile.welcome.enabled = true;
        snprintf(out->profile.welcome.file, sizeof(out->profile.welcome.file), "%s", default_assets->welcome.file);
        out->profile.welcome.count = default_assets->welcome.count;
        out->profile.welcome.duration_ms = default_assets->welcome.duration_ms;
        out->profile.welcome.loop = default_assets->welcome.loop;
        snprintf(out->profile.welcome.gui_id, sizeof(out->profile.welcome.gui_id), "WELCOME");

        if (!copy_dir_str(out->welcome_dir, sizeof(out->welcome_dir), default_assets->directory)) {
            LOG_ERROR("Default assets directory path exceeds maximum length.");
            return false;
        }
        out->welcome_is_default = true;
        LOG_INFO("Substituting default welcome animation '%s' from '%s'",
                 out->profile.welcome.file, out->welcome_dir);
    }

    /* Rule 1-3 for Ring Button sound (resolved independently of welcome) */
    if (!source_profile->ring_button.enabled &&
        default_assets &&
        default_assets->enabled &&
        default_assets->ring_button.enabled) {
        memset(&out->profile.ring_button, 0, sizeof(out->profile.ring_button));
        out->profile.ring_button.enabled = true;
        snprintf(out->profile.ring_button.file, sizeof(out->profile.ring_button.file), "%s", default_assets->ring_button.file);
        out->profile.ring_button.repeat_times = default_assets->ring_button.repeat_times;
        out->profile.ring_button.volume = default_assets->ring_button.volume;
        snprintf(out->profile.ring_button.sound_state_name, sizeof(out->profile.ring_button.sound_state_name), "RING_BUTTON_PRESSED");

        if (!copy_dir_str(out->ring_button_dir, sizeof(out->ring_button_dir), default_assets->directory)) {
            LOG_ERROR("Default assets directory path exceeds maximum length.");
            return false;
        }
        out->ring_button_is_default = true;
        LOG_INFO("Substituting default ring button sound '%s' from '%s'",
                 out->profile.ring_button.file, out->ring_button_dir);
    }

    return true;
}

bool unifi_profile_build_default_effective(
    const config_default_assets_t *default_assets,
    unifi_effective_profile_t *out
) {
    if (!config_default_assets_available(default_assets) || !out) {
        return false;
    }

    unifi_profile_t empty = {0};
    empty.schema_version = 1;
    return unifi_profile_resolve_effective(&empty, NULL, default_assets, out);
}

static error_return_t validate_single_effective_asset(
    const char *asset_dir,
    const char *asset_filename,
    utils_file_class_t cls,
    bool is_default
) {
    const char *label = is_default
        ? (cls == UTILS_FILE_CLASS_ANIMATION ? "Default welcome animation" : "Default ring button sound")
        : (cls == UTILS_FILE_CLASS_ANIMATION ? "Profile welcome animation" : "Profile ring button sound");

    error_return_t err = utils_is_valid_filename(asset_filename, cls);
    if (err.error_code != ERROR_NONE) {
        return error_wrap(err, "%s '%s' filename invalid", label, asset_filename ? asset_filename : "");
    }

    if (!asset_dir || asset_dir[0] == '\0' || !utils_directory_exists(asset_dir)) {
        LOG_ERROR("%s directory '%s' does not exist", label, asset_dir ? asset_dir : "");
        return error_createf(ERROR_DIRECTORY_NOT_FOUND, "%s directory does not exist for '%s'", label, asset_filename);
    }

    char asset_path[PATH_MAX];
    if (!utils_build_path(asset_path, sizeof(asset_path), asset_dir, asset_filename)) {
        LOG_ERROR("Error building path for %s '%s/%s'", label, asset_dir, asset_filename);
        return error_createf(ERROR_PATH_TOO_LONG, "Error building path for %s '%s'", label, asset_filename);
    }

    if (!utils_file_exists(asset_path)) {
        LOG_ERROR("%s file '%s' does not exist", label, asset_path);
        return error_createf(ERROR_FILE_NOT_FOUND, "%s '%s' does not exist", label, asset_filename);
    }

    err = utils_is_valid_file(asset_path, cls);
    if (err.error_code != ERROR_NONE) {
        return error_wrap(err, "%s '%s' validation failed", label, asset_filename);
    }

    return error_none();
}

error_return_t unifi_effective_profile_validate_assets(const unifi_effective_profile_t *effective) {
    if (!effective) {
        return error_create(ERROR_INVALID_PARAMETERS, "Invalid parameters");
    }

    if (effective->profile.welcome.enabled) {
        error_return_t err = validate_single_effective_asset(
            effective->welcome_dir,
            effective->profile.welcome.file,
            UTILS_FILE_CLASS_ANIMATION,
            effective->welcome_is_default
        );
        if (err.error_code != ERROR_NONE) {
            return err;
        }
    }

    if (effective->profile.ring_button.enabled) {
        error_return_t err = validate_single_effective_asset(
            effective->ring_button_dir,
            effective->profile.ring_button.file,
            UTILS_FILE_CLASS_SOUND,
            effective->ring_button_is_default
        );
        if (err.error_code != ERROR_NONE) {
            return err;
        }
    }

    return error_none();
}
