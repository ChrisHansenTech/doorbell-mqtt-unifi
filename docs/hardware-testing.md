# Manual Hardware Testing Guide

This guide defines the reusable manual validation procedures for verifying `doorbell-mqtt-unifi` against a live **UniFi Protect G4 Doorbell Pro**, an MQTT broker, and Home Assistant.

Automated unit, integration, and startup smoke tests run completely offline using link-time mocks (see [TESTING.md](../TESTING.md)). Use this guide before releases, after firmware updates, or after modifying SSH, SCP, IPC, Home Assistant discovery, or Docker runtime code.

---

## Scope

Manual hardware validation covers behaviors that cannot be verified offline:

- Live TCP/SSH handshake and password authentication against the doorbell's SSH daemon using `UNIFI_PROTECT_RECOVERY_CODE`.
- Actual execution of `ubnt_ipc_cli` (`ChangeLcmGuiSettings`, `ChangeSoundLedSettings`) and firmware JSON response parsing.
- Actual execution of `/usr/local/bin/playSound.sh` and audible playback on the doorbell speaker.
- Legacy `.conf` patching and `killall`/`pidof` service restart behavior (`ubnt_lcm_gui`, `ubnt_sounds_leds`).
- Storage guardrail execution (`df -kP` / `du -sk`) on `/var/etc/persistent` under BusyBox.
- Home Assistant MQTT Discovery ingestion, entity controls, and sensor updates.
- Docker container entrypoint behavior (`docker-entrypoint.sh`, `PUID`/`PGID` privilege drop, and mounted volumes).

---

## Prerequisites and Safety Rules

### Requirements

- **Hardware**: UniFi Protect G4 Doorbell Pro with SSH enabled, `ubnt_ipc_cli`, and `/usr/local/bin/playSound.sh`.
- **Services**: Reachable MQTT broker and Home Assistant instance with the MQTT integration enabled.
- **Build**: Fresh release binary (`make release`) or container image (`docker build .`).
- **Isolated test data**: Dedicated test `config.json`, at least two valid profile directories (with `.png` animation and `.ogg`/`.wav` ring sound), at least one valid SFX file under `/sounds`, and a separate `MQTT_INSTANCE` (e.g., `test_doorbell`) if testing alongside a production instance.

### Safety Rules (Non-Destructive Testing Only)

- **Never** perform destructive operations on a physical doorbell (do not intentionally fill `/var/etc/persistent`, alter permissions under `/etc/persistent`, or kill unrelated Protect processes).
- **Back up** the doorbell's current profile first using the **Download assets** button (or keep a known-good preset ready to restore afterward).
- For destructive fault conditions (disk full, permission denied, service restart timeout), use the automated unit tests described in [Automated Alternatives for Destructive Faults](#automated-alternatives-for-destructive-faults).

---

## Permanent Expected Behavior Test Procedures

The scenarios below describe the **permanent expected behavior** of the service.

### Core Profile, Audio, and Asset Workflows

| # | Scenario | Mode / Prereq | Steps | Expected Behavior |
|---|---|---|---|---|
| 1 | **IPC profile upload, apply, and validation** | `unifi.apply_method = "ipc"` | Select a valid preset (with `.png` + `.ogg`/`.wav`) in Home Assistant **Presets**, then press **Validate profile**. | **Status** transitions `uploading` → `idle`; **Last error** clears to code `0`; **Last applied profile** updates to the preset name; doorbell displays the new animation and uses the new ring sound in <2 seconds; **Validate profile** reports `match`. |
| 2 | **Sound configuration change** | IPC or Legacy | Update a test profile's `ringButton.file` or `volume`, apply the profile, and press the physical doorbell button. | Doorbell plays the updated ring audio at the configured volume; **Last error** reports code `0`. |
| 3 | **Legacy profile operation** | `unifi.apply_method = "legacy"` | Set `UNIFI_APPLY_METHOD=legacy` (or `"apply_method": "legacy"`), restart the service, and select a valid preset. | Service downloads `.conf` files, patches them, uploads updates, restarts `ubnt_lcm_gui` and `ubnt_sounds_leds` (~30 seconds), returns **Status** to `idle`, clears **Last error** to `0`, and updates the doorbell display/sound. Pressing **Validate profile** reports `unsupported`. |
| 4 | **Test config button** | IPC and Legacy | Press **Test config** in Home Assistant. | Applies `./test-profile`, clears **Last error** to `0`, sets **Last applied profile** to `test-config`, resets **Presets** to `none`, and returns **Status** to `idle`. |
| 5 | **Asset download** | IPC or Legacy | Press **Download assets** in Home Assistant. | **Status** transitions `downloading` → `idle`; assets and generated `profile.json` are saved under `/profiles/downloads/<YYYYMMDD_HHMMSS>`; **Last asset download** timestamp and attributes update; **Last error** clears to `0`. |
| 6 | **Ad-hoc SFX preset playback** | Configured `sfx.presets` + `/sounds` file | Select an SFX preset in Home Assistant **SFX presets**. | Doorbell plays the audio file immediately (<1 second) without changing the active profile or restarting doorbell processes; remote `/tmp/doorbell-mqtt-unifi/sfx` file is cleaned up; **SFX presets** resets to `none`; **Status** returns to `idle`; **Last error** clears to `0`. |

### Error Handling, State Integrity, and Recovery

| # | Scenario | Mode / Prereq | Steps | Expected Behavior |
|---|---|---|---|---|
| 7 | **Failed SSH authentication (safe)** | Any | Configure an intentionally wrong `UNIFI_PROTECT_RECOVERY_CODE`, restart the service, and trigger **Presets** or **Test config**. | Operation fails cleanly without crashing; **Last error** reports `ERROR_SSH_CONNECTION_FAILED` (`3001`); **Last applied profile** and `/config/state/last_applied.json` are **not** overwritten; **Status** returns to `idle`. |
| 8 | **Unreachable doorbell / dropped packets (safe)** | Any | Point `SSH_HOST` to a firewalled/blackholed IP (e.g., RFC 5737 `192.0.2.1` or a blocked VLAN IP) or a closed port, restart the service, and trigger **Presets** or **Test config**. | Silently dropped packets time out in ~10 seconds and report `ERROR_SSH_CONNECTION_TIMEOUT` (`3003`); immediate refusal or ICMP unreachable fails immediately with `ERROR_SSH_CONNECTION_FAILED` (`3001`); **Last applied profile** and persisted state remain unchanged; **Status** returns to `idle`. |
| 9 | **Failed remote upload / staging (safe)** | IPC and Legacy | Temporarily remove or rename a referenced asset file in a test profile directory (or replace it with a non-PNG spoofed `.png` file), then select that preset. | Fails during staging before apply runs; **Last error** reports `ERROR_FILE_NOT_FOUND` (`1205`) or `ERROR_FILE_TYPE_INVALID` (`1204`); doorbell configuration is untouched; remote `/tmp/doorbell-mqtt-unifi/profile` and local temp upload directory are cleaned up; **Last applied profile** is unchanged; **Status** returns to `idle`. |
| 10 | **Recovery following a failed operation** | IPC and Legacy | Immediately after Scenario 7 (after restoring credentials) or Scenario 9 (without restarting the container), select a valid preset. | Subsequent apply succeeds cleanly, clears **Last error** (`code=0`, `name=ERROR_NONE`, `message=""`), updates **Last applied profile**, and **Validate profile** reports `match` (in IPC mode). |
| 11 | **Home Assistant status & error reporting** | Live Home Assistant | Observe **Status**, **Last error** (state + `code`/`name`/`message` attributes), and **Last applied profile** across Scenarios 1–10. | Every failure publishes a specific non-zero error code, name, and message; failed operations never publish success state; a subsequent successful operation clears **Last error** back to `0`. |
| 12 | **Stability under repeated failures** | Any | Trigger 5 consecutive failing profile applies (e.g., missing asset or invalid `profile.json`) followed by 1 valid apply while monitoring logs and container resource usage (`docker stats`). | Service remains responsive with no crash or stuck `uploading` state, recovers on the valid apply, and leaves no leaked `/tmp/doorbell-mqtt-unifi/upload-*` directories on the host. |

### Container Runtime Smoke Check

| # | Scenario | Mode / Prereq | Steps | Expected Behavior |
|---|---|---|---|---|
| 13 | **Docker entrypoint and volumes** | Docker (`linux/amd64` or `linux/arm64`) | Start a fresh container with empty `/config`, `/profiles`, `/sounds` mounts and custom `PUID`/`PGID`. | Entrypoint initializes `/config/config.json` on first run, drops privileges to `PUID`/`PGID` via `gosu`, starts the service cleanly, and stops cleanly on `docker stop` (`SIGTERM`). |

### Default Assets and Virtual `Default` Profile Validation

Validate these scenarios in **IPC** mode (and repeat Scenarios 17, 18, 20, and 21 in **Legacy** mode to verify `.conf` patching and process restart behavior).

| # | Scenario | Mode / Prereq | Steps | Expected Behavior |
|---|---|---|---|---|
| 14 | **Feature disabled baseline** | `defaultAssets` omitted or `enabled: false` | Start service, inspect **Presets** dropdown, and apply an existing preset. | `Default` does **not** appear in **Presets** (unless user-defined); existing profile apply behavior is unchanged. |
| 15 | **Enable both default assets & discovery** | `defaultAssets.enabled = true`, both `welcome` and `ringButton` enabled with valid files in `/profiles/default-assets` | Restart service and inspect **Presets** dropdown in Home Assistant. | `Default` appears at the top of the **Presets** dropdown (immediately after `none` if present) without modifying `presets[]`; service does **not** auto-apply assets on startup. |
| 16 | **Manual virtual `Default` apply** | Both default assets enabled (IPC & Legacy) | Select `Default` manually in Home Assistant **Presets**. | Doorbell displays default welcome animation and plays default ring sound on press; **Last applied profile** and **Presets** report `Default`; **Custom directory** clears; **Status** returns to `idle`; **Last error** clears to `0`. |
| 17 | **Welcome-only profile uses default sound** | Both default assets enabled (IPC & Legacy) | Select a profile with `welcome.enabled: true` and `ringButton.enabled: false` (or omitted). | Doorbell displays the profile's animation and plays the **default ring sound** with default `volume`/`repeatTimes`; **Last applied profile** reports the selected profile name (not `Default`). |
| 18 | **Sound-only profile uses default welcome** | Both default assets enabled (IPC & Legacy) | Select a profile with `welcome.enabled: false` (or omitted) and `ringButton.enabled: true`. | Doorbell displays the **default welcome animation** with default `count`/`durationMs`/`loop` and plays the profile's ring sound; **Last applied profile** reports the selected profile name. |
| 19 | **Profile-specific precedence** | Both default assets enabled | Select a profile with both `welcome.enabled: true` and `ringButton.enabled: true`. | Both profile-specific assets are applied; neither default asset is uploaded or substituted. |
| 20 | **Switch seasonal → `Default` & HA automation** | IPC & Legacy | Apply a seasonal preset, then trigger a Home Assistant automation/script calling `select.select_option` with `option: "Default"`. | Default animation and ring sound replace the seasonal assets on the doorbell; **Last applied profile** and **Presets** update to `Default`. |
| 21 | **IPC validation, reapply, and restart restoration** | IPC mode | With `Default` (or a profile using defaults) active: (a) press **Validate profile**, (b) restart container, (c) update a default asset file in `config.json`, restart, and press **Reapply last profile**, then **Validate profile**. | (a) Reports `match`; (b) Restart restores **Last applied profile** and **Presets** without re-uploading; (c) **Reapply last profile** applies the updated default asset and subsequent **Validate profile** reports `match`. |
| 22 | **Missing selected default asset & recovery** | `defaultAssets` enabled | Temporarily rename/move the configured default welcome file, select `Default`, then restore the file and retry without restarting the container. | Failed apply reports `ERROR_FILE_NOT_FOUND` (`1205`) naming the default welcome animation, returns **Status** to `idle`, and preserves previous **Last applied profile** and `last_applied.json`; retry after restoring the file succeeds cleanly. |
| 23 | **Disable feature & verify unaffected workflows** | Set `defaultAssets.enabled = false` | Restart service, inspect **Presets** dropdown, trigger an **SFX preset**, and press **Asset download**. | `Default` is removed from retained MQTT discovery; **SFX presets** plays normally on doorbell speaker; **Asset download** saves the active doorbell config normally. |

---

## Automated Alternatives for Destructive Faults

Do **not** simulate the following failure modes on physical hardware; verify them with the offline unit test suites instead:

- **Remote disk full (`rc=28` → `ERROR_REMOTE_DISK_FULL`)**
- **Remote permission denied (`rc=13` → `ERROR_PROFILE_APPLY_PERMISSION_DENIED`)**
- **Remote file move / verification failure (`move_*`, `verify_*`)**
- **Legacy service restart timeout (`rc=210` → `ERROR_PROFILE_APPLY_RESTART_FAILED`)**
- **Cleanup failure not overwriting the primary upload/apply error**

Run the automated suites that verify all of these paths:

```bash
make test SUITE=test_unifi_remote
make test SUITE=test_command
```

---

## Temporary Limitations and Known Defects

The behaviors below are **temporary limitations or known defects** in the current implementation rather than intended permanent behavior. Review and remove items from this section as their corresponding GitHub issues are resolved:

1. **Post-connect SSH session/command/SCP timeouts**:
   TCP connection establishment enforces a fixed 10-second timeout, but established `libssh2` session operations (handshake, auth, remote command execution, and SCP transfers) do not yet call `libssh2_session_set_timeout()`.
2. **Remaining non-SSH early-return stuck Home Assistant status**:
   SSH connection failures across all commands now return **Status** to `idle`, but certain non-SSH early-return paths (such as missing SFX preset in **SFX presets** or missing `last_applied.json` in **Reapply last profile**) may still leave **Status** in `uploading` or `validating` until another command completes.
3. **Custom profile directory command in Docker**:
   `command_apply_custom()` currently builds profile paths relative to `./profiles` instead of `DOORBELL_PROFILES_DIR` (`/profiles`). Testing custom directory applies inside Docker may fail with `ERROR_PROFILE_NOT_FOUND` unless `/app/profiles` exists.
4. **Downloaded `profile.json` `loop` field and empty `ring_button.file` (Scenario 5)**:
   `unifi_profile_read_from_lcm_gui_conf()` does not currently read `"loop"` from `ubnt_lcm_gui.conf`, so downloaded profiles always write `"loop": false`. Additionally, asset download currently requires `ring_button.file` to be non-empty.
5. **Button entity discovery `state_topic` metadata**:
   MQTT discovery payloads for Home Assistant `button` entities currently include an unintended `state_topic` field. Home Assistant ignores it on button entities, though it is visible when inspecting raw discovery JSON on the broker.
