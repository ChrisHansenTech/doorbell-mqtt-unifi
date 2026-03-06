#pragma once
#include <stddef.h>
#include <stdbool.h>

#define CMD_MKDIR "mkdir -p '%s'"
#define CMD_MV "mv '%s' '%s'"
#define CMD_RM_RF "rm -rf '%s'"
#define CMD_RESET_DIR "rm -rf '%s' && mkdir -p '%s'"
#define CMD_RESTART_LCM "systemctl restart unifi-lcm-gui unifi-lcm-sound"

#define SCRIPT_PREAMBLE \
    "set -eu\n" \
    "STEP=\"\"\n" \
    "fail() { rc=$1; echo \"ERROR step=$STEP rc=$rc\" 1>&2; exit $rc; }\n" \
    "run() { STEP=$1; shift; \"$@\" || fail $?; }\n" \
    "\n" \
    "PERSIST_DIR='/etc/persistent'\n" \
    "ANIM_DIR='/etc/persistent/lcm/animation'\n" \
    "SND_DIR='/etc/persistent/sounds'\n" \
    "run ensure_dirs mkdir -p \"$ANIM_DIR\" \"$SND_DIR\"\n"

#define SCRIPT_RESTART \
    "run restart_services sh -c '\n" \
    "  has_proc() { pidof \"$1\" >/dev/null 2>&1; }\n" \
    "  kill_ok()  { killall \"$1\" >/dev/null 2>&1; rc=$?; [ $rc -eq 0 -o $rc -eq 1 ] || exit $rc; }\n" \
    "\n" \
    "  wait_both_gone() {\n" \
    "    t=$1; while [ $t -gt 0 ]; do\n" \
    "      has_proc ubnt_lcm_gui || a=0; has_proc ubnt_lcm_gui && a=1;\n" \
    "      has_proc ubnt_sounds_leds || b=0; has_proc ubnt_sounds_leds && b=1;\n" \
    "      [ $a -eq 0 -a $b -eq 0 ] && return 0;\n" \
    "      sleep 0.2; t=$((t-1));\n" \
    "    done; return 1;\n" \
    "  }\n" \
    "\n" \
    "  wait_both_back() {\n" \
    "    t=$1; while [ $t -gt 0 ]; do\n" \
    "      has_proc ubnt_lcm_gui && a=1; has_proc ubnt_lcm_gui || a=0;\n" \
    "      has_proc ubnt_sounds_leds && b=1; has_proc ubnt_sounds_leds || b=0;\n" \
    "      [ $a -eq 1 -a $b -eq 1 ] && return 0;\n" \
    "      sleep 0.2; t=$((t-1));\n" \
    "    done; return 1;\n" \
    "  }\n" \
    "\n" \
    "  kill_ok ubnt_lcm_gui\n" \
    "  kill_ok ubnt_sounds_leds\n" \
    "\n" \
    "  # 0.2s * 25 = 5s max\n" \
    "  wait_both_gone 25 || exit 210\n" \
    "  # 0.2s * 50 = 10s max\n" \
    "  wait_both_back 50 || exit 211\n" \
    "  echo \"OK\"\n" \
    "'\n"

#define SCRIPT_STORAGE_GUARDRAIL \
    "PERSIST_ROOT=/var/etc/persistent; " \
    "ANIM_DIR=\"$PERSIST_ROOT/lcm/animation\"; " \
    "SOUND_DIR=\"$PERSIST_ROOT/sounds\"; " \
    "STAGED_ROOT=/tmp/doorbell-mqtt-unifi/profile; " \
    "STAGED_ANIM_DIR=\"$STAGED_ROOT/anim\"; " \
    "STAGED_SOUND_DIR=\"$STAGED_ROOT/sound\"; " \
    "\n" \
    "if [ ! -d \"$STAGED_ROOT\" ]; then " \
    "    echo \"ERROR: staged root $STAGED_ROOT does not exist\" >&2; " \
    "    exit 2; " \
    "fi; " \
    "\n" \
    "if [ ! -d \"$STAGED_ANIM_DIR\" ] && [ ! -d \"$STAGED_SOUND_DIR\" ]; then " \
    "    echo \"ERROR: no staged assets found under $STAGED_ROOT\" >&2; " \
    "    exit 2; " \
    "fi; " \
    "\n" \
    "free_now_kb=$(df -kP \"$PERSIST_ROOT\" | awk 'NR==2 {print $4}'); " \
    "\n" \
    "old_assets_kb=$(du -sk \"$ANIM_DIR\" \"$SOUND_DIR\" 2>/dev/null | awk '{sum+=$1} END {print sum+0}'); " \
    "\n" \
    "new_assets_kb=$(du -sk \"$STAGED_ANIM_DIR\" \"$STAGED_SOUND_DIR\" 2>/dev/null | awk '{sum+=$1} END {print sum+0}'); " \
    "\n" \
    "projected_free_kb=$((free_now_kb + old_assets_kb - new_assets_kb)); " \
    "\n" \
    "echo \"free_now_kb=$free_now_kb\"; " \
    "echo \"old_assets_kb=$old_assets_kb\"; " \
    "echo \"new_assets_kb=$new_assets_kb\"; " \
    "echo \"projected_free_kb=$projected_free_kb\"; " \
    "\n" \
    "if [ \"$projected_free_kb\" -lt 1024 ]; then " \
    "    echo \"ERROR: projected free space would fall below 1024 KB\" >&2; " \
    "    exit 1; " \
    "fi; " \
    "\n" \
    "if [ \"$projected_free_kb\" -lt 2048 ]; then " \
    "    echo \"WARN: projected free space below 2048 KB\"; " \
    "    exit 10; " \
    "fi; " \
    "\n" \
    "echo \"OK: storage guardrail passed\"; " \
    "exit 0"

#define CMD_IPC_CLI "ubnt_ipc_cli -T=%s -f=%s -M=%s -r=1 -F=- -z"

#define CMD_PLAY_SFX \
    "/usr/local/bin/playSound.sh '%s' -v %d; " \
    "rc=$?; " \
    "rm -f -- '%s'; " \
    "exit $rc"


typedef struct {
    bool has_error;     
    char step[64];
    int rc;                 
} ssh_step_error_t;

bool ssh_cmd_mkdir(char *out, size_t out_sz, const char *path);

bool ssh_cmd_mv(char *out, size_t out_sz, const char *src, const char *dst);

bool ssh_cmd_rm_rf(char *out, size_t out_sz, const char *path);

bool ssh_cmd_reset_dir(char *out, size_t out_sz, const char *path);

bool ssh_cmd_restart_lcm(char *out, size_t out_sz);

bool ssh_cmd_storage_guardrail(char *out, size_t out_sz);

bool build_apply_profile_command(char *out, size_t out_sz, const char *tmp_dir);

bool ssh_parse_step_error(const char *stderr_text, ssh_step_error_t *out);

bool ssh_cmd_ipc_cli(char *out, size_t out_sz, const char *target, const char *payload_path);

bool build_move_assets_ipc(char *out, size_t out_sz, const char *tmp_dir);

bool ssh_cmd_play_sfx(char *out, size_t out_sz, const char *sfx_file_path, int volume);