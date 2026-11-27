#define _POSIX_C_SOURCE 200809L

#include "third_party/unity/unity.h"
#include "support/test_helpers.h"
#include "cJSON.h"
#include "state.h"
#include "unifi_ipc.h"
#include "unifi_profile.h"
#include "unifi_profile_conf.h"
#include "unifi_profile_json.h"
#include "unifi_sfx.h"
#include "utils.h"

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_tmp_dir[128];

void setUp(void) {
    TEST_ASSERT_TRUE(test_make_temp_dir(g_tmp_dir, sizeof(g_tmp_dir)));
}

void tearDown(void) {
    test_remove_dir_recursive(g_tmp_dir);
}

/* ===== unifi_profile_json ===== */

void test_profile_json_load_and_write_roundtrip(void) {
    unifi_profile_t p = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file("test-profile", &p));
    TEST_ASSERT_EQUAL_INT(1, p.schema_version);
    TEST_ASSERT_TRUE(p.welcome.enabled);
    TEST_ASSERT_EQUAL_STRING("test.png", p.welcome.file);
    TEST_ASSERT_EQUAL_INT(60, p.welcome.count);
    TEST_ASSERT_EQUAL_INT(1960, p.welcome.duration_ms);
    TEST_ASSERT_FALSE(p.welcome.loop);
    TEST_ASSERT_EQUAL_STRING("WELCOME", p.welcome.gui_id);

    TEST_ASSERT_TRUE(p.ring_button.enabled);
    TEST_ASSERT_EQUAL_STRING("test.ogg", p.ring_button.file);
    TEST_ASSERT_EQUAL_INT(1, p.ring_button.repeat_times);
    TEST_ASSERT_EQUAL_INT(100, p.ring_button.volume);
    TEST_ASSERT_EQUAL_STRING("RING_BUTTON_PRESSED", p.ring_button.sound_state_name);

    char out_path[PATH_MAX];
    snprintf(out_path, sizeof(out_path), "%s/profile.json", g_tmp_dir);
    TEST_ASSERT_TRUE(unifi_profile_write_to_file(out_path, &p));

    unifi_profile_t reloaded = {0};
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(g_tmp_dir, &reloaded));
    TEST_ASSERT_EQUAL_STRING(p.welcome.file, reloaded.welcome.file);
    TEST_ASSERT_EQUAL_INT(p.welcome.count, reloaded.welcome.count);
    TEST_ASSERT_EQUAL_STRING(p.ring_button.file, reloaded.ring_button.file);
    TEST_ASSERT_EQUAL_INT(p.ring_button.volume, reloaded.ring_button.volume);
}

void test_profile_json_errors_and_defaults(void) {
    unifi_profile_t p = {0};
    TEST_ASSERT_FALSE(unifi_profile_load_from_file(NULL, &p));
    TEST_ASSERT_FALSE(unifi_profile_load_from_file(g_tmp_dir, NULL));
    TEST_ASSERT_FALSE(unifi_profile_load_from_file(g_tmp_dir, &p)); /* no profile.json */

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/profile.json", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(path, "{not valid json"));
    TEST_ASSERT_FALSE(unifi_profile_load_from_file(g_tmp_dir, &p));

    /* Missing schemaVersion defaults to 1; missing sections succeed with zeroed fields */
    TEST_ASSERT_TRUE(test_write_text(path, "{}"));
    TEST_ASSERT_TRUE(unifi_profile_load_from_file(g_tmp_dir, &p));
    TEST_ASSERT_EQUAL_INT(1, p.schema_version);
    TEST_ASSERT_FALSE(p.welcome.enabled);
    TEST_ASSERT_FALSE(p.ring_button.enabled);

    TEST_ASSERT_FALSE(unifi_profile_write_to_file(NULL, &p));
    TEST_ASSERT_FALSE(unifi_profile_write_to_file(path, NULL));
    TEST_ASSERT_FALSE(unifi_profile_write_to_file("/no/such/dir/profile.json", &p));
}

/* ===== unifi_profile_conf ===== */

void test_profile_conf_read_lcm_gui(void) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/ubnt_lcm_gui.conf", g_tmp_dir);

    TEST_ASSERT_TRUE(test_write_text(
        path,
        "{\"other\":1,\"customAnimations\":["
        "\"ignore_non_object\","
        "{\"file\":\"no_gui_id.png\"},"
        "{\"guiId\":\"OTHER\",\"file\":\"other.png\"},"
        "{\"guiId\":\"WELCOME\",\"file\":\"xmas.png\",\"count\":24,\"durationMs\":1500,\"enable\":true,\"loop\":true}"
        "]}"
    ));

    unifi_profile_t out = {0};
    TEST_ASSERT_TRUE(unifi_profile_read_from_lcm_gui_conf(path, &out));
    TEST_ASSERT_EQUAL_STRING("WELCOME", out.welcome.gui_id);
    TEST_ASSERT_EQUAL_STRING("xmas.png", out.welcome.file);
    TEST_ASSERT_EQUAL_INT(24, out.welcome.count);
    TEST_ASSERT_EQUAL_INT(1500, out.welcome.duration_ms);
    TEST_ASSERT_TRUE(out.welcome.enabled);

    /* Missing count defaults to 1 */
    TEST_ASSERT_TRUE(test_write_text(path, "{\"customAnimations\":[{\"guiId\":\"WELCOME\",\"file\":\"a.png\"}]}"));
    TEST_ASSERT_TRUE(unifi_profile_read_from_lcm_gui_conf(path, &out));
    TEST_ASSERT_EQUAL_INT(1, out.welcome.count);

    /* Missing customAnimations resets welcome and returns true */
    TEST_ASSERT_TRUE(test_write_text(path, "{\"other\":1}"));
    TEST_ASSERT_TRUE(unifi_profile_read_from_lcm_gui_conf(path, &out));
    TEST_ASSERT_EQUAL_STRING("", out.welcome.file);

    /* Invalid inputs */
    TEST_ASSERT_FALSE(unifi_profile_read_from_lcm_gui_conf(NULL, &out));
    TEST_ASSERT_FALSE(unifi_profile_read_from_lcm_gui_conf(path, NULL));
    TEST_ASSERT_FALSE(unifi_profile_read_from_lcm_gui_conf("/no/such/file", &out));
    TEST_ASSERT_TRUE(test_write_text(path, "{bad"));
    TEST_ASSERT_FALSE(unifi_profile_read_from_lcm_gui_conf(path, &out));
}

void test_profile_conf_read_sounds_leds(void) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/ubnt_sounds_leds.conf", g_tmp_dir);

    TEST_ASSERT_TRUE(test_write_text(
        path,
        "{\"customSounds\":["
        "123,"
        "{\"file\":\"missing_state.ogg\"},"
        "{\"soundStateName\":\"OTHER\",\"file\":\"other.ogg\"},"
        "{\"soundStateName\":\"RING_BUTTON_PRESSED\",\"enable\":true,\"file\":\"bell.ogg\",\"repeatTimes\":2,\"volume\":90}"
        "]}"
    ));

    unifi_profile_t out = {0};
    TEST_ASSERT_TRUE(unifi_profile_read_from_sounds_leds_conf(path, &out));
    TEST_ASSERT_EQUAL_STRING("RING_BUTTON_PRESSED", out.ring_button.sound_state_name);
    TEST_ASSERT_TRUE(out.ring_button.enabled);
    TEST_ASSERT_EQUAL_STRING("bell.ogg", out.ring_button.file);
    TEST_ASSERT_EQUAL_INT(2, out.ring_button.repeat_times);
    TEST_ASSERT_EQUAL_INT(90, out.ring_button.volume);

    TEST_ASSERT_TRUE(test_write_text(path, "{}"));
    TEST_ASSERT_TRUE(unifi_profile_read_from_sounds_leds_conf(path, &out));
    TEST_ASSERT_EQUAL_STRING("", out.ring_button.file);

    TEST_ASSERT_FALSE(unifi_profile_read_from_sounds_leds_conf(NULL, &out));
    TEST_ASSERT_FALSE(unifi_profile_read_from_sounds_leds_conf(path, NULL));
    TEST_ASSERT_FALSE(unifi_profile_read_from_sounds_leds_conf("/no/such/file", &out));
    TEST_ASSERT_TRUE(test_write_text(path, "{bad"));
    TEST_ASSERT_FALSE(unifi_profile_read_from_sounds_leds_conf(path, &out));
}

void test_profile_conf_patch_lcm_and_sounds(void) {
    char in_lcm[PATH_MAX];
    char out_lcm[PATH_MAX];
    char in_snd[PATH_MAX];
    char out_snd[PATH_MAX];

    snprintf(in_lcm, sizeof(in_lcm), "%s/lcm.conf", g_tmp_dir);
    snprintf(out_lcm, sizeof(out_lcm), "%s/lcm.patched", g_tmp_dir);
    snprintf(in_snd, sizeof(in_snd), "%s/snd.conf", g_tmp_dir);
    snprintf(out_snd, sizeof(out_snd), "%s/snd.patched", g_tmp_dir);

    TEST_ASSERT_TRUE(test_write_text(in_lcm, "{\"keepMe\":42,\"customAnimations\":[]}"));
    TEST_ASSERT_TRUE(test_write_text(in_snd, "{\"keepSound\":true,\"customSounds\":[]}"));

    unifi_profile_t desired = {0};
    desired.welcome.enabled = true;
    snprintf(desired.welcome.file, sizeof(desired.welcome.file), "new.png");
    desired.welcome.count = 12;
    desired.welcome.duration_ms = 600;
    desired.welcome.loop = true;

    desired.ring_button.enabled = true;
    snprintf(desired.ring_button.file, sizeof(desired.ring_button.file), "new.ogg");
    desired.ring_button.repeat_times = 3;
    desired.ring_button.volume = 70;

    TEST_ASSERT_TRUE(unifi_profile_patch_lcm_gui_conf(in_lcm, out_lcm, &desired));
    TEST_ASSERT_TRUE(unifi_profile_patch_sounds_leds_conf(in_snd, out_snd, &desired));

    char *lcm_text = NULL;
    TEST_ASSERT_TRUE(utils_read_file(out_lcm, &lcm_text, NULL));
    cJSON *lcm_json = cJSON_Parse(lcm_text);
    TEST_ASSERT_NOT_NULL(lcm_json);
    TEST_ASSERT_EQUAL_INT(42, cJSON_GetObjectItem(lcm_json, "keepMe")->valueint);
    cJSON *anims = cJSON_GetObjectItem(lcm_json, "customAnimations");
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(anims));
    TEST_ASSERT_EQUAL_STRING("new.png", cJSON_GetObjectItem(cJSON_GetArrayItem(anims, 0), "file")->valuestring);
    cJSON_Delete(lcm_json);
    free(lcm_text);

    char *snd_text = NULL;
    TEST_ASSERT_TRUE(utils_read_file(out_snd, &snd_text, NULL));
    cJSON *snd_json = cJSON_Parse(snd_text);
    TEST_ASSERT_NOT_NULL(snd_json);
    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(snd_json, "keepSound")));
    cJSON *snds = cJSON_GetObjectItem(snd_json, "customSounds");
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(snds));
    TEST_ASSERT_EQUAL_STRING("new.ogg", cJSON_GetObjectItem(cJSON_GetArrayItem(snds, 0), "file")->valuestring);
    cJSON_Delete(snd_json);
    free(snd_text);

    /* When ring_button is disabled, patch_sounds_leds_conf renames input unchanged */
    char unchanged_in[PATH_MAX];
    char unchanged_out[PATH_MAX];
    snprintf(unchanged_in, sizeof(unchanged_in), "%s/unchanged.conf", g_tmp_dir);
    snprintf(unchanged_out, sizeof(unchanged_out), "%s/unchanged.patched", g_tmp_dir);
    TEST_ASSERT_TRUE(test_write_text(unchanged_in, "{\"original\":1}"));
    desired.ring_button.enabled = false;
    TEST_ASSERT_TRUE(unifi_profile_patch_sounds_leds_conf(unchanged_in, unchanged_out, &desired));
    TEST_ASSERT_TRUE(utils_file_exists(unchanged_out));
    TEST_ASSERT_FALSE(utils_file_exists(unchanged_in));

    /* Error cases */
    TEST_ASSERT_FALSE(unifi_profile_patch_lcm_gui_conf(NULL, out_lcm, &desired));
    TEST_ASSERT_FALSE(unifi_profile_patch_lcm_gui_conf("/no/such/file", out_lcm, &desired));
    TEST_ASSERT_FALSE(unifi_profile_patch_sounds_leds_conf(NULL, out_snd, &desired));
    TEST_ASSERT_FALSE(unifi_profile_patch_sounds_leds_conf("/no/such/file", out_snd, &desired));
    TEST_ASSERT_NULL(unifi_profile_build_custom_animations_array(NULL));
    TEST_ASSERT_NULL(unifi_profile_build_custom_sounds_array(NULL));
}

/* ===== unifi_ipc ===== */

void test_ipc_build_messages_and_parse_responses(void) {
    unifi_profile_t p = {0};
    p.welcome.enabled = true;
    snprintf(p.welcome.file, sizeof(p.welcome.file), "anim.png");
    p.welcome.count = 15;
    p.welcome.duration_ms = 750;
    p.welcome.loop = true;
    p.ring_button.enabled = true;
    snprintf(p.ring_button.file, sizeof(p.ring_button.file), "ring.ogg");
    p.ring_button.repeat_times = 1;
    p.ring_button.volume = 95;

    char lcm_msg[PATH_MAX];
    char snd_msg[PATH_MAX];
    snprintf(lcm_msg, sizeof(lcm_msg), "%s/lcm_ipc.json", g_tmp_dir);
    snprintf(snd_msg, sizeof(snd_msg), "%s/snd_ipc.json", g_tmp_dir);

    TEST_ASSERT_TRUE(unifi_ipc_build_lcm_gui_message(lcm_msg, &p));
    TEST_ASSERT_TRUE(unifi_ipc_build_sounds_leds_message(snd_msg, &p));

    char *text = NULL;
    TEST_ASSERT_TRUE(utils_read_file(lcm_msg, &text, NULL));
    cJSON *obj = cJSON_Parse(text);
    TEST_ASSERT_NOT_NULL(obj);
    TEST_ASSERT_EQUAL_STRING("ChangeLcmGuiSettings", cJSON_GetObjectItem(obj, "functionName")->valuestring);
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(cJSON_GetObjectItem(obj, "customAnimations")));
    cJSON_Delete(obj);
    free(text);

    TEST_ASSERT_TRUE(utils_read_file(snd_msg, &text, NULL));
    obj = cJSON_Parse(text);
    TEST_ASSERT_NOT_NULL(obj);
    TEST_ASSERT_EQUAL_STRING("ChangeSoundLedSettings", cJSON_GetObjectItem(obj, "functionName")->valuestring);
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(cJSON_GetObjectItem(obj, "customSounds")));
    cJSON_Delete(obj);
    free(text);

    /* Error cases */
    TEST_ASSERT_FALSE(unifi_ipc_build_lcm_gui_message(NULL, &p));
    TEST_ASSERT_FALSE(unifi_ipc_build_lcm_gui_message(lcm_msg, NULL));
    TEST_ASSERT_FALSE(unifi_ipc_build_lcm_gui_message("/no/such/dir/a.json", &p));
    TEST_ASSERT_FALSE(unifi_ipc_build_sounds_leds_message(NULL, &p));
    TEST_ASSERT_FALSE(unifi_ipc_build_sounds_leds_message(snd_msg, NULL));
    TEST_ASSERT_FALSE(unifi_ipc_build_sounds_leds_message("/no/such/dir/b.json", &p));

    /* Parse apply response */
    unifi_ipc_raw_t raw = {0};
    raw.lcm_gui_json = strdup(
        "{\"response\":{\"payload\":{\"customAnimations\":["
        "{\"guiId\":\"WELCOME\",\"file\":\"anim.png\",\"count\":15,\"durationMs\":750,\"enable\":true,\"loop\":true}"
        "]}}}"
    );
    raw.sounds_leds_json = strdup(
        "{\"response\":{\"payload\":{\"customSounds\":["
        "{\"soundStateName\":\"RING_BUTTON_PRESSED\",\"file\":\"ring.ogg\",\"repeatTimes\":1,\"volume\":95,\"enable\":true}"
        "]}}}"
    );

    profile_state_t st = {0};
    TEST_ASSERT_TRUE(unifi_ipc_parse_apply_response(&raw, &st));
    TEST_ASSERT_EQUAL_UINT(1, st.custom_animation_count);
    TEST_ASSERT_EQUAL_STRING("WELCOME", st.custom_animations[0].gui_id);
    TEST_ASSERT_EQUAL_STRING("anim.png", st.custom_animations[0].file);
    TEST_ASSERT_EQUAL_INT(15, st.custom_animations[0].count);
    TEST_ASSERT_EQUAL_UINT(1, st.custom_sound_count);
    TEST_ASSERT_EQUAL_STRING("ring.ogg", st.custom_sounds[0].file);
    TEST_ASSERT_EQUAL_INT(95, st.custom_sounds[0].volume);
    state_free_profile_state(&st);
    unifi_ipc_raw_free(&raw);
    TEST_ASSERT_NULL(raw.lcm_gui_json);
    TEST_ASSERT_NULL(raw.sounds_leds_json);
    unifi_ipc_raw_free(NULL);

    /* Parse validate response (top-level arrays) */
    raw.lcm_gui_json = strdup("[{\"guiId\":\"WELCOME\",\"file\":\"anim.png\"}]");
    raw.sounds_leds_json = strdup("[{\"soundStateName\":\"RING_BUTTON_PRESSED\",\"file\":\"ring.ogg\"}]");
    TEST_ASSERT_TRUE(unifi_ipc_parse_validate_response(&raw, &st));
    TEST_ASSERT_EQUAL_UINT(1, st.custom_animation_count);
    TEST_ASSERT_EQUAL_INT(0, st.custom_animations[0].count);
    TEST_ASSERT_EQUAL_UINT(1, st.custom_sound_count);
    TEST_ASSERT_EQUAL_INT(0, st.custom_sounds[0].volume);
    state_free_profile_state(&st);
    unifi_ipc_raw_free(&raw);

    /* Invalid JSON / non-array responses */
    unifi_ipc_raw_t bad = { .lcm_gui_json = "{bad", .sounds_leds_json = "[]" };
    TEST_ASSERT_FALSE(unifi_ipc_parse_apply_response(&bad, &st));
    TEST_ASSERT_FALSE(unifi_ipc_parse_validate_response(&bad, &st));

    bad.lcm_gui_json = "{\"not_array\":1}";
    TEST_ASSERT_FALSE(unifi_ipc_parse_validate_response(&bad, &st));

    bad.lcm_gui_json = "[]";
    bad.sounds_leds_json = "{bad";
    TEST_ASSERT_FALSE(unifi_ipc_parse_validate_response(&bad, &st));
    state_free_profile_state(&st);

    bad.lcm_gui_json = "[]";
    bad.sounds_leds_json = "{\"not_array\":1}";
    TEST_ASSERT_FALSE(unifi_ipc_parse_validate_response(&bad, &st));
    state_free_profile_state(&st);
}

/* ===== unifi_sfx ===== */

void test_sfx_resolve_preset(void) {
    config_sfx_preset_item_t items[2] = {
        { .name = "Chime", .key_name = "chime", .file = "chime.ogg", .volume = 80 },
        { .name = "Fart", .key_name = "fart", .file = "fart.wav", .volume = 100 }
    };
    config_sfx_preset_t preset_cfg = { .items = items, .count = 2, .default_volume = 100 };
    sfx_ctx_t ctx = { .sfx_preset_cfg = &preset_cfg, .sounds_dir = "/sounds" };

    TEST_ASSERT_NULL(unifi_sfx_resolve_sfx(NULL, &ctx));
    TEST_ASSERT_NULL(unifi_sfx_resolve_sfx("Chime", NULL));
    TEST_ASSERT_NULL(unifi_sfx_resolve_sfx("Missing", &ctx));

    config_sfx_preset_item_t *found = unifi_sfx_resolve_sfx("chime", &ctx);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("chime.ogg", found->file);
    TEST_ASSERT_EQUAL_INT(80, found->volume);

    found = unifi_sfx_resolve_sfx("FART", &ctx);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("fart.wav", found->file);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_profile_json_load_and_write_roundtrip);
    RUN_TEST(test_profile_json_errors_and_defaults);
    RUN_TEST(test_profile_conf_read_lcm_gui);
    RUN_TEST(test_profile_conf_read_sounds_leds);
    RUN_TEST(test_profile_conf_patch_lcm_and_sounds);
    RUN_TEST(test_ipc_build_messages_and_parse_responses);
    RUN_TEST(test_sfx_resolve_preset);

    return UNITY_END();
}
