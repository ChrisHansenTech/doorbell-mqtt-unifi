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

## Device Support

**Supported:**
- UniFi G4 Doorbell Pro

**Not Supported:**
- UniFi G4 Doorbell Lite (no screen)
- UniFi G6 Entry (no screen)
- UniFi G6 Entry Pro (touchscreen)

## Why Use It?

- Apply animation & sound changes in under 2 seconds (IPC mode)
- Ad-hoc doorbell SFX
- Native Home Assistant MQTT Discovery integration
- Clean multi-doorbell support (one container per device)
- Docker-first deployment model

No polling. No background agent on the doorbell. No cloud round trips.

## Supported Architectures

The container image is published as a multi-architecture image and
automatically pulls the correct platform for your system.

Supported container platforms:

- `linux/amd64`
- `linux/arm64`

Validated environments:

- Docker on `linux/amd64`
- Docker on `linux/arm64` (Raspberry Pi 4)
- Native build on Ubuntu under WSL2
- Native build on Raspberry Pi OS Lite 64-bit

The service is written in portable C and has been validated on both x86_64
and ARM64 systems.

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

## Profile Validation

The service includes a profile validation system to confirm that the
configuration currently applied to the doorbell matches the expected
profile state.

This helps detect situations where:

-   Another instance of the service modified the doorbell
-   Manual configuration changes were applied
-   The doorbell restarted and lost its in-memory configuration
-   A profile failed to apply correctly

### How It Works

When a profile is applied, the service:

1.  Collects the full animation and sound configuration.
2.  Sorts the configuration deterministically.
3.  Computes a SHA-256 hash of the profile state.
4.  Saves the result to:

```sh
/config/last_applied.json
```

Example:

``` json
{
  "schemaVersion": 1,
  "profileName": "Test Config",
  "isPreset": false,
  "applyMethod": "ipc",
  "appliedAt": 1772924270,
  "hash": "c640634b28af478afd9a49abf3e5a8d4869ad0fd0c4e39c7559183d0a9a62b24"
}
```

### Validation Process

When the Validate Profile button is triggered from Home Assistant:

1.  The service reads the doorbell's current configuration via IPC.
2.  The configuration is normalized and hashed.
3.  The computed hash is compared against the stored hash.

Possible results:

  | Result      | Meaning |
  |-------------|---------|
  | match       | Doorbell configuration matches the expected profile |
  | mismatch    | Doorbell configuration differs from the expected profile |
  | unknown     | No previous state hash exists or validation cannot be performed |
  | unsupported | Apply method is configured as legacy, the feature only works with IPC |


### Home Assistant Entities

Button

-   Validate Profile\
    Triggers a validation check against the currently running doorbell
    configuration.

Button

-   Reapply last profile\
    Triggers the service to apply the profile saved in `last_applied.json` back to the doorbell

Sensors

-   Profile Validation\
    Reports match, mismatch, or unknown.

These diagnostic values help troubleshoot configuration drift

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
  -v /path/to/sounds:/sounds \
  ghcr.io/chrishansentech/doorbell-mqtt-unifi:latest
```

Restart the container after modifying `config.json`.

## Native Build

Although Docker is the recommended deployment method, the service can also be built and run directly on Linux.

Build tools:

- `build-essential`
- `pkg-config`

Libraries:

- `libssl-dev`
- `libssh2-1-dev`
- `libpaho-mqtt-dev`

Example for Debian, Ubuntu, or Raspberry Pi OS:

```bash
sudo apt update  
sudo apt install -y \  
  build-essential \  
  pkg-config \  
  libssl-dev \  
  libssh2-1-dev \  
  libpaho-mqtt-dev
```

Build and run:

```bash
git clone https://github.com/ChrisHansenTech/doorbell-mqtt-unifi.git  
cd doorbell-mqtt-unifi  
make  
./bin/doorbell-mqtt-unifi
```
### Runtime Paths

When running natively, the service expects the same default paths used by the Docker container.

Default values:

| Environment Variable          | Default               |
| ----------------------------- | --------------------- |
| `DOORBELL_CONFIG_PATH`        | `/config/config.json` |
| `DOORBELL_STATE_DIR`          | `/config/state`       |
| `DOORBELL_PROFILES_DIR`       | `/profiles`           |
| `DOORBELL_SOUNDS_DIR`         | `/sounds`             |

If these environment variables are not set, the service will use the defaults above.

When running outside Docker, you will typically want to override them.

Example:

```bash
export DOORBELL_CONFIG_PATH=./config/config.json  
export DOORBELL_STATE_DIR=./config/state  
export DOORBELL_PROFILES_DIR=./profiles  
export DOORBELL_SOUNDS_DIR=./sounds
```

You can also load them from a `.env` file:

```bash
set -a  
source .env  
set +a  
./doorbell-mqtt-unifi
```

## License

MIT License — see `LICENSE`.
