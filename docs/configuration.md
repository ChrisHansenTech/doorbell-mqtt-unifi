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
  "unifi": {
    "apply_method": "ipc"
  }
  "presets": [
    { "name": "Christmas", "directory": "christmas" },
    { "name": "New Years", "directory": "new_years" },
    { "name": "St. Patrick's Day", "directory": "st_pats" },
    { "name": "Birthday party", "directory": "birthday" }
  ]
}
```

## MQTT Section

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

## MQTT Namespacing

These keys are supported by the loader (even though they aren’t shown in the shipped `config.json`).

They control how the service identifies itself in MQTT and Home Assistant. Proper configuration is important when running multiple doorbells.

### mqtt.prefix

Env: `MQTT_PREFIX`
Default: `chrishansentech`

Base prefix used for all MQTT topics published by the service.

Example topic structure:

```
<prefix>/doorbell-mqtt-unifi/<instance>/...
```

Most users do not need to change this.


### mqtt.instance

Env: `MQTT_INSTANCE`
Default: `default`

Logical instance name used for:

- MQTT topic namespacing
- MQTT client ID generation
- Home Assistant discovery identifiers
- Home Assistant device name
- Entity unique IDs

This allows multiple doorbells to run as separate containers without conflicts.

For example:

```
MQTT_INSTANCE=front_door
MQTT_INSTANCE=back_door
MQTT_INSTANCE=garage
```

Each instance will appear as a separate device in Home Assistant.

A human-readable version of the instance name is automatically generated for display in Home Assistant:

- Dashes (`-`) and underscores (`_`) are replaced with spaces
- The first letter of each word is capitalized

Example:

```
MQTT_INSTANCE=front_door
```

Will display as:

```
UniFi Doorbell MQTT Service (Front Door)
```

The raw instance value is always used for machine identifiers and unique IDs. Only the display name is formatted.

Important:
Changing `mqtt.instance` after initial setup will create a new device in Home Assistant

## SSH Section

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

## UniFi Configuration

These settings control how changes are applied to the doorbell.

### unifi.apply_method

Env: `UNIF_APPLY_METHOD`  
Default: `legacy` (when upgrading from versions prior to v0.2.0)  
Default: `IPC` (new installs of v0.2.0 and later)

Controls the method used to apply animations and sounds to the doorbell.

Behavior by version:

- If upgrading from a version prior to **v0.2.0** and the `unifi` section does not exist in `config.json`, 
  the service defaults to `legacy` to avoid changing existing behavior.
- For new installations of **v0.2.0+**, the default is `IPC`.
- The method can always be overridden using the `UNIF_APPLY_METHOD` environment variable.    

#### legacy

Legacy file-based apply method.

The service:

1. Downloads the current `.conf` files from the doorbell
2. Patches them with your configured changes
3. Uploads the updated files
4. Restarts the related doorbell processes to reload configuration

This method is compatible with existing setups, but is slower. Updates typically take **~30 seconds**.

#### IPC

Fast in-memory apply method using inter-process communication.

The service:

1. Uploads IPC message payloads (for `customAnimations` and `customSounds`)
2. Sends IPC commands to update the doorbell’s in-memory configuration (no process restarts)

This method is significantly faster. Updates typically take **< 2 seconds**.

Notes:

- `IPC` is recommended for improved responsiveness and access to newer features.
- Some advanced features may require `IPC` and will be unavailable when using `legacy`.

## Presets Section

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

## SFX section

The `sfx` section enables ad-hoc sound playback using the doorbell’s existing `playSound.sh`.

Unlike profile-based ring sounds, SFX playback:

- Does not modify Protect configuration
- Does not require reboot or process restart
- Is executed immediately via SSH

### Example

{
  "sfx": {
    "presets": [
      {
        "name": "Hello",
        "file": "hello.ogg",
        "volume": 100
      },
      {
        "name": "Chime",
        "file": "chime.wav"
      }
    ],
    "defaultVolume": 75
  }
}

### sfx.presets[]

Defines named sound effects selectable from Home Assistant.

#### name

Required  
Display name shown in Home Assistant.

Must be unique (case-insensitive).

#### file

Required  
Filename under the `/sounds` bind mount.

Only files inside `/sounds` are allowed.

Supported formats:

- `.ogg`
- `.wav`

#### volume

Optional  
If defined and > 0, this value overrides `defaultVolume` for the preset.

If missing or `0`, `defaultVolume` is used.

## sfx.defaultVolume

Range: `0–100`  
Default: `100`

Global fallback volume used when a preset does not specify its own volume.

## Runtime Behavior

When a preset is selected:

1. The service validates the preset.
2. The file path is resolved under `/sounds`.
3. The file is uploaded to:

   `/tmp/doorbell-mqtt-unifi/sfx/`

4. The following command is executed:

   `playSound.sh <file> -v <volume>`

5. The temporary file is removed.


## Quick “what must I set?” checklist

Most users only need:

- `mqtt.host`    
- `ssh.host`
- `UNIFI_PROTECT_RECOVERY_CODE` (environment variable)
- `presets` (if they want custom names/directories)