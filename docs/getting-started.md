# Getting Started

This guide walks through deploying Doorbell MQTT UniFi using Docker.

## Requirements

- Home Assistant with MQTT integration enabled
- MQTT broker (Mosquitto or equivalent)
- UniFi Protect G4 Doorbell Pro
- SSH enabled for the doorbell

---

## Step 1: Run the Container

```bash
docker run -d \
  --name doorbell-mqtt-unifi \
  --restart unless-stopped \
  -e UNIFI_PROTECT_RECOVERY_CODE=your_recovery_code \
  -v /path/to/doorbell-mqtt-unifi/config:/config \
  -v /path/to/doorbell-mqtt-unifi/profiles:/profiles \
  ghcr.io/chrishansentech/doorbell-mqtt-unifi:latest
```

---

## Step 2: Edit Configuration

Edit:

```
/path/to/doorbell-mqtt-unifi/config/config.json
```

Set:

- `mqtt.host`
- `ssh.host`

Then restart:

```bash
docker restart doorbell-mqtt-unifi
```

---

## Step 3: Verify in Home Assistant

After restart, the device and entities should appear automatically via MQTT discovery.
