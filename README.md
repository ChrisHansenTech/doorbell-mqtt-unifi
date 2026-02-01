# Doorbell MQTT UniFi

A C-based service that bridges **UniFi Protect doorbell profiles** with **MQTT**
and **Home Assistant**. It publishes Home Assistant discovery entities, applies
holiday or custom profiles over SSH, and can download the active profile assets
from the doorbell.

This project enables deeper, local-only customization of UniFi doorbell
animations and sounds beyond what the official Home Assistant integration
currently supports.

---

## ⚠️ Audience & Safety Notes

This project is intended for **advanced users** who:

- Are comfortable with MQTT and Home Assistant
- Understand SSH access and device-level file modification
- Accept the risks of modifying files on embedded devices

This is **not an official UniFi tool**.

The doorbell generally self-heals by re-syncing configuration from UniFi Protect
after a reboot, but misuse may temporarily remove custom sounds or animations.
Use at your own risk.

---

## Project Status

**Current state:** Alpha (happy-path functional)

### Known working
- Holiday and custom profile application
- Home Assistant MQTT discovery (controls + sensors)
- Test asset upload via Home Assistant
- Profile download from the doorbell
- Graceful shutdown on SIGINT / SIGTERM

### Not yet finalized
- Docker image and container runtime
- Rollback behavior on partial failures
- Long-running retry/backoff logic
- Formal release versioning

---

## Home Assistant UI

The service publishes MQTT discovery payloads so Home Assistant automatically
creates entities for control and status.

![Home Assistant entities](docs/images/ha-entities.png)

---

## Current Capabilities

- Loads configuration from `config.json` with environment-variable overrides
- Connects to MQTT via Eclipse Paho
- Publishes availability, status, and error topics
- Publishes Home Assistant discovery entities
- Applies holiday or custom profiles over SSH
- Downloads the doorbell’s active profile assets
- Writes logs to stdout/stderr (Docker-friendly)

---

## Repository Layout

```
├── src/                 # C sources
├── include/             # Public headers
├── profiles/            # Holiday/custom profiles + downloads
├── test-assets/         # Sample assets used by the test command (not user-facing)
├── tests/               # Unity-based unit tests + fixtures
├── bin/                 # Built binary (created by make)
├── build/               # Object files and dependency data (created by make)
├── config.json          # Runtime configuration (required)
├── config.example.json  # Example configuration template
├── Makefile             # GCC build with pkg-config integration
└── Dockerfile           # Placeholder for a future container image
```

---

## Prerequisites

- GCC or Clang
- pkg-config
- Eclipse Paho MQTT C client (`libpaho-mqtt3c`)
- libssh2
- OpenSSL headers

On Debian/Ubuntu:

```bash
sudo apt install build-essential pkg-config libpaho-mqtt-dev libssh2-1-dev libssl-dev
```

---

## Build and Run

```bash
make          # release-style build
make debug    # debug build (-O0 -g3)
make run      # convenience target; requires a prior build
```

Or run manually:

```bash
./bin/doorbell-mqtt-unifi
```

---

## Configuration

Runtime configuration is loaded from `config.json`. Environment variables take
precedence when set.

Start from `config.example.json`.

### Minimal example

```json
{
  "mqtt": {
    "host": "localhost",
    "port": 1883,
    "prefix": "chrishansentech",
    "instance": "default"
  },
  "ssh": {
    "host": "192.168.1.135",
    "port": 22,
    "user": "ubnt",
    "password_env": "UNIFI_PROTECT_RECOVERY_CODE"
  },
  "holidays": {
    "Christmas": "christmas",
    "New Years": "new_years",
    "Easter": "easter"
  }
}
```

---

## Profiles

Profiles live under:

```
profiles/<name>/
```

Each profile must include:
- `profile.json`
- One image file (`.png`)
- One sound file (`.ogg`)

---

## License

MIT License — see `LICENSE`.
