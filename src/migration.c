#include "migration.h"
#include "logger.h"
#include "utils.h"
#include <errno.h>
#include <linux/limits.h>
#include <unistd.h>

static void migrate_last_applied_state_if_needed(const char *profiles_dir,  const char *state_dir) {
    char old_path[PATH_MAX];
    char new_path[PATH_MAX] = "/config/state/last_applied.json";

    if (!utils_build_path(old_path, sizeof(old_path), profiles_dir, ".state/last_applied.json")) {
        LOG_ERROR("Failed to create path '%s/%s'", profiles_dir, ".state/last_applied.json");
        return;
    }

    if (!utils_build_path(new_path, sizeof(new_path), state_dir, "last_applied.json")) {
        LOG_ERROR("Failed to create path '%s/%s'", profiles_dir, ".state/last_applied.json");
        return;
    }

    if (access(new_path, F_OK) == 0) return;
    if (access(old_path, F_OK) != 0) return;

    if (!utils_create_directory(state_dir)) {
        LOG_ERROR("Failed to create directory '%s'", state_dir);
        return;
    }

    if (utils_move_file_cross_fs(old_path, new_path)) {
        LOG_INFO("Migrated last_applied state: %s -> %s", old_path, new_path);
    } else {
        LOG_WARN("Failed migrating last_applied state: %s -> %s (errno=%d)", old_path, new_path, errno);
    }
}

void migration_run(const char *profiles_dir, const char *sound_dir, const char *state_dir) {
    (void)sound_dir;

    migrate_last_applied_state_if_needed(profiles_dir, state_dir);
}