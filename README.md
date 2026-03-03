# Doorbell MQTT UniFi

![CI](https://github.com/ChrisHansenTech/doorbell-mqtt-unifi/actions/workflows/ci.yml/badge.svg?branch=dev)
![Release](https://github.com/ChrisHansenTech/doorbell-mqtt-unifi/actions/workflows/release.yml/badge.svg)
![Version](https://img.shields.io/github/v/release/ChrisHansenTech/doorbell-mqtt-unifi)
![License](https://img.shields.io/github/license/ChrisHansenTech/doorbell-mqtt-unifi)

**Doorbell MQTT UniFi** is a lightweight, C-based service that bridges
**UniFi Protect G4 Doorbell Pro** devices with **MQTT** and **Home Assistant**.

It enables fast, local-only control of custom animations, sounds, and profile assets — without cloud dependencies or manual SSH scripting.

Designed for reliability, speed, and clean Home Assistant integration.

> **Release Channels**
> - `:latest` → Stable release  
> - `:dev` → Development/nightly build (may contain bugs)

## Why Use It?

- Apply animation & sound changes in under 2 seconds (IPC mode)
- Native Home Assistant MQTT Discovery integration
- Safe legacy compatibility for existing users
- Clean multi-doorbell support (one container per device)
- Docker-first deployment model

No polling. No background agent on the doorbell. No cloud round trips.

## Apply Methods

The service supports two methods for applying animations and sounds.

### IPC (Recommended)

The IPC (Inter-process Communication) method:

* Uploads IPC payloads for `customAnimations` and `customSounds`
* Updates the doorbell’s in-memory configuration
* Does **not** restart doorbell processes

Typical update time: **< 2 seconds**

* New installations of **v0.2.0+ default to IPC**.
* Some advanced features require IPC.

### Legacy

The legacy method:

1. Downloads existing `.conf` files
2. Patches them
3. Uploads updates
4. Restarts relevant doorbell processes

Typical update time: **~30 seconds**

If upgrading from a version prior to v0.2.0 and the `unifi` section does not exist in `config.json`, the service defaults to `legacy` to preserve existing behavior.

You can override the method at any time:

```
UNIF_APPLY_METHOD=IPC
```

## Multi‑Doorbell Support

Each container represents **one doorbell**.

To run multiple doorbells, deploy multiple containers with different instances:

```
MQTT_INSTANCE=front_door
MQTT_INSTANCE=back_door
```

The instance value controls:

- MQTT topic namespacing
- MQTT client ID
- Home Assistant discovery identifiers
- Entity unique IDs

### Human‑Readable Device Names

The service automatically formats the instance name for display in Home Assistant:

* Dashes (`-`) and underscores (`_`) become spaces
* The first letter of each word is capitalized

Example:

```
MQTT_INSTANCE=front_door
```

Displays as:

```
(Front Door)
```

⚠️ Changing `MQTT_INSTANCE` after initial setup will create a new device in Home Assistant because unique IDs are derived from it.

## Home Assistant Integration

The service publishes MQTT discovery payloads so Home Assistant automatically creates entities for control and status.

Entities include:

- Preset Profile selector
- Custom Profile Directory input
- Download Assets button
- Test Config button
- Last Applied Profile sensor
- Last Error sensor
- Status sensor
- Last Asset Download timestamp sensor

Automation is fully local via MQTT.

## Ad-Hoc Sound Effects (SFX)

In addition to profile-based ring sounds, the service supports ad-hoc sound effects (SFX).

This allows Home Assistant users to trigger custom sounds on the doorbell at any time, 
independently of UniFi Protect’s ring/visitor tone configuration.

### Features

- Uses the doorbell’s built-in `playSound.sh`
- Supports `.ogg` and `.wav`
- Global volume control (0–100)
- Preset-based dropdown (no raw JSON required)
- No Protect config changes
- No service restart required
- No doorbell process restart

Playback typically begins in under 1 second.

### How It Works

1. Select an SFX preset in Home Assistant.
2. The file is uploaded to:
   `/tmp/doorbell-mqtt-unifi/sfx/<unique_filename>`
3. The service executes:
   `playSound.sh /tmp/doorbell-mqtt-unifi/sfx/<unique_filename> -v <volume>`
4. The temporary file is removed after playback.
5. Status is published via MQTT.

SFX playback is mutex-protected to prevent concurrent executions.


## Quick Start (Docker Recommended)

On first startup, if `/config/config.json` does not exist, the container generates a sample configuration file.

Required environment variable:

- `UNIFI_PROTECT_RECOVERY_CODE`

Minimal example:

```bash
docker run -d \
  --name doorbell-mqtt-unifi \
  --restart unless-stopped \
  -e UNIFI_PROTECT_RECOVERY_CODE=your_recovery_code \
  -v /path/to/config:/config \
  -v /path/to/profiles:/profiles \
  ghcr.io/chrishansentech/doorbell-mqtt-unifi:latest
```

Restart the container after modifying `config.json`.

## Profile Configuration Note

In `profile.json`, `welcome.durationMs` represents the **total animation duration**, not per-frame duration.

Example:

If `count = 57` and `durationMs = 1900`, the animation runs at approximately 30 FPS.

Lower values increase animation speed.

## License

MIT License — see `LICENSE`.
