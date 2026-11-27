#include "third_party/unity/unity.h"
#include "ssh_commands.h"

#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

void test_simple_ssh_command_builders(void) {
    char buf[512];
    char tiny[8];

    TEST_ASSERT_TRUE(ssh_cmd_mkdir(buf, sizeof(buf), "/tmp/test"));
    TEST_ASSERT_EQUAL_STRING("mkdir -p '/tmp/test'", buf);
    TEST_ASSERT_FALSE(ssh_cmd_mkdir(tiny, sizeof(tiny), "/tmp/test"));

    TEST_ASSERT_TRUE(ssh_cmd_mv(buf, sizeof(buf), "/tmp/a", "/tmp/b"));
    TEST_ASSERT_EQUAL_STRING("mv '/tmp/a' '/tmp/b'", buf);
    TEST_ASSERT_FALSE(ssh_cmd_mv(tiny, sizeof(tiny), "/tmp/a", "/tmp/b"));

    TEST_ASSERT_TRUE(ssh_cmd_reset_dir(buf, sizeof(buf), "/tmp/work"));
    TEST_ASSERT_EQUAL_STRING("rm -rf '/tmp/work' && mkdir -p '/tmp/work'", buf);
    TEST_ASSERT_FALSE(ssh_cmd_reset_dir(tiny, sizeof(tiny), "/tmp/work"));

    TEST_ASSERT_TRUE(ssh_cmd_restart_lcm(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING(CMD_RESTART_LCM, buf);
    TEST_ASSERT_FALSE(ssh_cmd_restart_lcm(tiny, sizeof(tiny)));

    char big[4096];
    TEST_ASSERT_TRUE(ssh_cmd_storage_guardrail(big, sizeof(big)));
    TEST_ASSERT_NOT_NULL(strstr(big, "projected_free_kb"));
    TEST_ASSERT_FALSE(ssh_cmd_storage_guardrail(tiny, sizeof(tiny)));

    TEST_ASSERT_TRUE(ssh_cmd_ipc_cli(buf, sizeof(buf), "ubnt_lcm_gui", "/tmp/msg.json"));
    TEST_ASSERT_EQUAL_STRING("ubnt_ipc_cli -T=ubnt_lcm_gui -f=ubnt_ipc_cli -M=/tmp/msg.json -r=1 -F=- -z", buf);
    TEST_ASSERT_FALSE(ssh_cmd_ipc_cli(tiny, sizeof(tiny), "ubnt_lcm_gui", "/tmp/msg.json"));

    TEST_ASSERT_TRUE(ssh_cmd_ipc_cli_cfg(buf, sizeof(buf), "ubnt_lcm_gui", "ChangeLcmGuiSettings", "customAnimations"));
    TEST_ASSERT_EQUAL_STRING("ubnt_ipc_cli -T=ubnt_lcm_gui -f=ubnt_ipc_cli -n=ChangeLcmGuiSettings -p=customAnimations -r=1 -F=- -z", buf);
    TEST_ASSERT_FALSE(ssh_cmd_ipc_cli_cfg(tiny, sizeof(tiny), "ubnt_lcm_gui", "ChangeLcmGuiSettings", "customAnimations"));

    TEST_ASSERT_TRUE(ssh_cmd_play_sfx(buf, sizeof(buf), "/tmp/sfx/bell.ogg", 80));
    TEST_ASSERT_NOT_NULL(strstr(buf, "/usr/local/bin/playSound.sh '/tmp/sfx/bell.ogg' -v 80"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "rm -f -- '/tmp/sfx/bell.ogg'"));
    TEST_ASSERT_FALSE(ssh_cmd_play_sfx(tiny, sizeof(tiny), "/tmp/sfx/bell.ogg", 80));
}

void test_build_apply_profile_command_valid_and_injection_rejected(void) {
    char buf[8192];
    /* Non-zero sentinel byte needed because production code currently checks !*out */
    buf[0] = 'X';

    TEST_ASSERT_TRUE(build_apply_profile_command(buf, sizeof(buf), "/tmp/doorbell-mqtt-unifi/profile"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "set -eu"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "/tmp/doorbell-mqtt-unifi/profile/anim/"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "ubnt_lcm_gui.conf.patched"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "restart_services"));

    buf[0] = 'X';
    TEST_ASSERT_FALSE(build_apply_profile_command(buf, sizeof(buf), NULL));
    buf[0] = 'X';
    TEST_ASSERT_FALSE(build_apply_profile_command(buf, sizeof(buf), ""));
    buf[0] = 'X';
    TEST_ASSERT_FALSE(build_apply_profile_command(buf, sizeof(buf), "/tmp/bad'quote"));
    buf[0] = 'X';
    TEST_ASSERT_FALSE(build_apply_profile_command(buf, sizeof(buf), "/tmp/bad\nline"));
    buf[0] = 'X';
    TEST_ASSERT_FALSE(build_apply_profile_command(buf, sizeof(buf), "/tmp/bad\rline"));
}

void test_build_move_assets_ipc_valid_and_injection_rejected(void) {
    char buf[8192];
    buf[0] = 'X';

    TEST_ASSERT_TRUE(build_move_assets_ipc(buf, sizeof(buf), "/tmp/doorbell-mqtt-unifi/profile"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "set -eu"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "move_anim"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "move_snd"));
    TEST_ASSERT_NULL(strstr(buf, "restart_services"));

    buf[0] = 'X';
    TEST_ASSERT_FALSE(build_move_assets_ipc(buf, sizeof(buf), NULL));
    buf[0] = 'X';
    TEST_ASSERT_FALSE(build_move_assets_ipc(buf, sizeof(buf), ""));
    buf[0] = 'X';
    TEST_ASSERT_FALSE(build_move_assets_ipc(buf, sizeof(buf), "/tmp/inject'ed"));
}

void test_ssh_parse_step_error_variants(void) {
    ssh_step_error_t err;

    TEST_ASSERT_FALSE(ssh_parse_step_error(NULL, &err));
    TEST_ASSERT_FALSE(ssh_parse_step_error("ERROR step=move_anim rc=13", NULL));
    TEST_ASSERT_FALSE(ssh_parse_step_error("all good\n", &err));
    TEST_ASSERT_FALSE(ssh_parse_step_error("ERROR step=missing_rc\n", &err));

    TEST_ASSERT_TRUE(ssh_parse_step_error("noise\nERROR step=move_anim rc=13\n", &err));
    TEST_ASSERT_TRUE(err.has_error);
    TEST_ASSERT_EQUAL_STRING("move_anim", err.step);
    TEST_ASSERT_EQUAL_INT(13, err.rc);

    /* Multiple errors: must return the LAST occurrence */
    TEST_ASSERT_TRUE(ssh_parse_step_error(
        "ERROR step=cleanup_anim rc=1\nERROR step=move_snd rc=28\n",
        &err
    ));
    TEST_ASSERT_TRUE(err.has_error);
    TEST_ASSERT_EQUAL_STRING("move_snd", err.step);
    TEST_ASSERT_EQUAL_INT(28, err.rc);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_simple_ssh_command_builders);
    RUN_TEST(test_build_apply_profile_command_valid_and_injection_rejected);
    RUN_TEST(test_build_move_assets_ipc_valid_and_injection_rejected);
    RUN_TEST(test_ssh_parse_step_error_variants);

    return UNITY_END();
}
