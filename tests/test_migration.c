#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "support/test_helpers.h"
#include "migration.h"
#include "utils.h"

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>

static char g_tmp_dir[128];

void setUp(void) {
    TEST_ASSERT_TRUE(test_make_temp_dir(g_tmp_dir, sizeof(g_tmp_dir)));
}

void tearDown(void) {
    test_remove_dir_recursive(g_tmp_dir);
}

void test_migration_moves_legacy_last_applied_state(void) {
    char profiles_dir[256];
    char legacy_state_dir[512];
    char old_file[PATH_MAX];
    char state_dir[256];
    char new_file[PATH_MAX];

    snprintf(profiles_dir, sizeof(profiles_dir), "%s/profiles", g_tmp_dir);
    snprintf(legacy_state_dir, sizeof(legacy_state_dir), "%s/.state", profiles_dir);
    snprintf(old_file, sizeof(old_file), "%s/last_applied.json", legacy_state_dir);
    snprintf(state_dir, sizeof(state_dir), "%s/state", g_tmp_dir);
    snprintf(new_file, sizeof(new_file), "%s/last_applied.json", state_dir);

    TEST_ASSERT_TRUE(utils_create_directory(profiles_dir));
    TEST_ASSERT_TRUE(utils_create_directory(legacy_state_dir));
    TEST_ASSERT_TRUE(test_write_text(old_file, "{\"profileName\":\"Christmas\"}"));

    migration_run(profiles_dir, "/unused", state_dir);

    TEST_ASSERT_FALSE(utils_file_exists(old_file));
    TEST_ASSERT_TRUE(utils_file_exists(new_file));

    char *content = NULL;
    TEST_ASSERT_TRUE(utils_read_file(new_file, &content, NULL));
    TEST_ASSERT_EQUAL_STRING("{\"profileName\":\"Christmas\"}", content);
    free(content);
}

void test_migration_does_not_overwrite_existing_new_state(void) {
    char profiles_dir[256];
    char legacy_state_dir[512];
    char old_file[PATH_MAX];
    char state_dir[256];
    char new_file[PATH_MAX];

    snprintf(profiles_dir, sizeof(profiles_dir), "%s/profiles", g_tmp_dir);
    snprintf(legacy_state_dir, sizeof(legacy_state_dir), "%s/.state", profiles_dir);
    snprintf(old_file, sizeof(old_file), "%s/last_applied.json", legacy_state_dir);
    snprintf(state_dir, sizeof(state_dir), "%s/state", g_tmp_dir);
    snprintf(new_file, sizeof(new_file), "%s/last_applied.json", state_dir);

    TEST_ASSERT_TRUE(utils_create_directory(profiles_dir));
    TEST_ASSERT_TRUE(utils_create_directory(legacy_state_dir));
    TEST_ASSERT_TRUE(utils_create_directory(state_dir));
    TEST_ASSERT_TRUE(test_write_text(old_file, "{\"profileName\":\"Old\"}"));
    TEST_ASSERT_TRUE(test_write_text(new_file, "{\"profileName\":\"Current\"}"));

    migration_run(profiles_dir, NULL, state_dir);

    TEST_ASSERT_TRUE(utils_file_exists(old_file));
    char *content = NULL;
    TEST_ASSERT_TRUE(utils_read_file(new_file, &content, NULL));
    TEST_ASSERT_EQUAL_STRING("{\"profileName\":\"Current\"}", content);
    free(content);
}

void test_migration_noop_when_old_state_absent_or_state_dir_uncreatable(void) {
    char profiles_dir[256];
    char legacy_state_dir[512];
    char old_file[PATH_MAX];
    char state_dir[256];

    snprintf(profiles_dir, sizeof(profiles_dir), "%s/profiles", g_tmp_dir);
    snprintf(legacy_state_dir, sizeof(legacy_state_dir), "%s/.state", profiles_dir);
    snprintf(old_file, sizeof(old_file), "%s/last_applied.json", legacy_state_dir);
    snprintf(state_dir, sizeof(state_dir), "%s/state", g_tmp_dir);

    TEST_ASSERT_TRUE(utils_create_directory(profiles_dir));
    migration_run(profiles_dir, NULL, state_dir);
    TEST_ASSERT_FALSE(utils_directory_exists(state_dir));

    /* Old file exists, but state_dir cannot be created */
    TEST_ASSERT_TRUE(utils_create_directory(legacy_state_dir));
    TEST_ASSERT_TRUE(test_write_text(old_file, "{}"));
    migration_run(profiles_dir, NULL, "/no/such/parent/dir/state");
    TEST_ASSERT_TRUE(utils_file_exists(old_file));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_migration_moves_legacy_last_applied_state);
    RUN_TEST(test_migration_does_not_overwrite_existing_new_state);
    RUN_TEST(test_migration_noop_when_old_state_absent_or_state_dir_uncreatable);

    return UNITY_END();
}
