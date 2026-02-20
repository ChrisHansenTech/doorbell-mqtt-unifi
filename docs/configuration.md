# Configuration Reference

Configuration is loaded from `/config/config.json`.

Environment variables override JSON values for supported settings. Configuration is read on startup — restart the container after changes.

`presets` cannot be set via environment variables.

## Full Example `config.json`

```json
{
  "mqtt": {
    "host": "",
    "port": 1883,
    "username": "",
    "password": "",
    "qos": 1,
    "keepalive": 30,
    "clean_session": 1,
    "retained_online": 1,
    "tls_enabled": 0,
    "cafile": "",
    "certfile": "",
    "keyfile": "",
    "keypass": ""
  },
  "ssh": {
    "host": "",
    "port": 22,
    "user": "ubnt",
    "password_env": "UNIFI_PROTECT_RECOVERY_CODE"
  },
  "presets": [
    { "name": "Christmas", "directory": "christmas" },
    { "name": "New Years", "directory": "new_years" },
    { "name": "St. Patrick's Day", "directory": "st_pats" },
    { "name": "Birthday party", "directory": "birthday" }
  ]
}
```

# MQTT Section

### mqtt.host

Env: `MQTT_HOST`  
Default: `localhost`  

Hostname or IP address of the MQTT broker.

- In Docker, `localhost` refers to the container itself.
- Use a LAN IP (e.g. `192.168.1.10`) or a Docker service name (e.g. `mqtt`) if the broker is on the same Docker network.

### mqtt.port

Env: `MQTT_PORT`  
Default: `1883`

MQTT broker port.

Common values:

- `1883` (non-TLS)    
- `8883` (TLS; depends on broker)

### mqtt.username

Env: `MQTT_USERNAME`  
Default: empty

Username for broker authentication (if your broker requires it).

### mqtt.password

Env: `MQTT_PASSWORD`  
Default: empty

Password for broker authentication.

Recommendation: set via environment variable rather than storing secrets in `config.json`.

### mqtt.qos

Env: `MQTT_QOS`  
Default: `1`

QoS for published messages (0–2).

- `0` = at most once    
- `1` = at least once (recommended)
- `2` = exactly once (highest overhead)

### mqtt.keepalive

Env: `MQTT_KEEPALIVE`  
Default: `30` (seconds)

Keepalive interval in seconds.

### mqtt.clean_session

Env: `MQTT_CLEAN_SESSION`  
Default: `1`

- `1` = clean session (no stored session state)    
- `0` = persistent session (broker stores session state)
    
### mqtt.retained_online

Env: `MQTT_RETAINED_ONLINE`  
Default: `1`

If enabled, availability/online messages are published as **retained** so Home Assistant sees the correct state immediately after restarts.

### mqtt.tls_enabled

Env: `MQTT_TLS_ENABLED`  
Default: `0`

Enable TLS for the MQTT connection.

- `0` = disabled (uses `tcp://`)    
- `1` = enabled (uses `ssl://`)
    
When enabled, set the TLS file options below as needed.

### mqtt.cafile

Env: `MQTT_CAFILE`  
Default: empty

Path to a CA certificate file used to validate the broker certificate.

Example:

- `/config/certs/ca.crt`

### mqtt.certfile

Env: `MQTT_CERTFILE`  
Default: empty

Path to the client certificate file (mutual TLS / mTLS), if your broker requires client certs.

Example:

- `/config/certs/client.crt`

### mqtt.keyfile

Env: `MQTT_KEYFILE`  
Default: empty

Path to the client private key file for mutual TLS / mTLS.

Example:

- `/config/certs/client.key`
    
### mqtt.keypass

Env: `MQTT_KEYPASS`  
Default: empty

Passphrase used to decrypt the private key if the key file is encrypted.

Recommendation: set via environment variable if used.

# MQTT Namespacing

These keys are supported by the loader (even though they aren’t shown in the shipped `config.json`).

### mqtt.prefix

Env: `MQTT_PREFIX`  
Default: `chrishansentech`

Base prefix used for MQTT topics.

### mqtt.instance

Env: `MQTT_INSTANCE`  
Default: `default`

Instance name used for namespacing topics (helpful if running multiple doorbells/services).

### mqtt.client_id

Env: `MQTT_CLIENT_ID`  
Default: computed

If not set, the service generates:

`<prefix>-doorbell-mqtt-unifi-<instance>`

# SSH Section

### ssh.host

Env: `SSH_HOST`  
Default: `localhost`  

Hostname or IP address of the UniFi doorbell.

### ssh.port

Env: `SSH_PORT`  
Default: `22`

SSH port of the doorbell.

### ssh.username

Env: `SSH_USER`  
Default: `ubnt`

SSH username.

### ssh.password_env

Default: `UNIFI_PROTECT_RECOVERY_CODE`

Name of the environment variable containing the SSH password.
This value is the **variable name**, not the password itself.

Example:

```bash
-e UNIFI_PROTECT_RECOVERY_CODE="your_password_here"
```

# Presets Section

Presets define the named profiles users can select, and the directory containing assets for each preset.

Presets are loaded from `config.json` only.

### presets[].name

Required

Display name for the preset.

Internally, the service normalizes the name (case/space-insensitive) and requires it to be unique.

Examples that would collide:

- `New Years` vs `new years`
- `St Pats` vs `St Pats`
    
### presets[].directory

Required

Directory name for the preset’s asset bundle.

Keep it filesystem-friendly (lowercase + underscores recommended).

## Quick “what must I set?” checklist

Most users only need:

- `mqtt.host`    
- `ssh.host`
- `UNIFI_PROTECT_RECOVERY_CODE` (environment variable)
- `presets` (if they want custom names/directories)