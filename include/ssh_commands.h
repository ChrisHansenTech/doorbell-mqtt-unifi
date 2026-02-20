#pragma once
#include <stddef.h>
#include <stdbool.h>

#define CMD_MKDIR "mkdir -p '%s'"
#define CMD_MV "mv '%s' '%s'"
#define CMD_RM_RF "rm -rf '%s'"
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


typedef struct {
    bool has_error;     
    char step[64];
    int rc;                 
} ssh_step_error_t;

bool ssh_cmd_mkdir(char *out, size_t out_sz, const char *path);

bool ssh_cmd_mv(char *out, size_t out_sz, const char *src, const char *dst);

bool ssh_cmd_rm_rf(char *out, size_t out_sz, const char *path);

bool ssh_cmd_restart_lcm(char *out, size_t out_sz);

bool build_apply_profile_command(char *out, size_t out_sz, const char *tmp_dir, const char *anim_file, const char *sound_file);

bool ssh_parse_step_error(const char *stderr_text, ssh_step_error_t *out);