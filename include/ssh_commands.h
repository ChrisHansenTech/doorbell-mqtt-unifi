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
    "STEP=\"ensure_dirs\"\n" \
    "PERSIST_DIR='/etc/persistent'\n" \
    "ANIM_DIR='/etc/persistent/lcm/animation'\n" \
    "SND_DIR='/etc/persistent/sounds'\n" \
    "mkdir -p \"$ANIM_DIR\" \"$SND_DIR\"\n"

#define SCRIPT_RESTART \
    "run restart_services sh -c 'killall ubnt_lcm_gui >/dev/null 2>&1 || true; " \
    "killall ubnt_sounds_leds >/dev/null 2>&1 || true; sleep 1'\n" \
    "echo \"OK\"\n"



bool ssh_cmd_mkdir(char *out, size_t out_sz, const char *path);

bool ssh_cmd_mv(char *out, size_t out_sz, const char *src, const char *dst);

bool ssh_cmd_rm_rf(char *out, size_t out_sz, const char *path);

bool ssh_cmd_restart_lcm(char *out, size_t out_sz);

bool build_apply_profile_command(char *out, size_t out_sz, const char *tmp_dir, const char *anim_file, const char *sound_file);