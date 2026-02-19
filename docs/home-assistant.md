# Home Assistant Integration

The service automatically creates a device and entities in Home Assistant using MQTT Discovery.

Once the container is running and connected to MQTT, the device will appear under:

**Settings → Devices & Services → MQTT**

If the device or entities are deleted, simply restart the service and they will be recreated.

# Device Overview

Device name:

- `UniFi Doorbell MQTT Service`
- If using a custom instance: `UniFi Doorbell MQTT Service (<instance>)`

All entities listed below belong to this device.

# Main Controls

## Presets (Select)

**Entity type:** Select  
**Purpose:** Choose which profile to apply

This dropdown lists all presets defined in your `config.json`.

When you select a preset:

- The profile is uploaded to the doorbell
- The animation and/or sound is applied
- The status sensor updates during the process

This is the primary way most users will interact with the service.

## Custom Directory (Text)

**Entity type:** Text  
**Purpose:** Apply a profile directory that is not in the preset list

Enter the name of a directory under `/profiles/` and the service will attempt to apply it.

This is useful for:

- Testing new profiles
- Applying temporary or one-off directories
- Working with downloaded profiles before adding them to presets

## Asset Download (Button)

**Entity type:** Button  
**Purpose:** Capture the current doorbell configuration as a profile

Pressing this button will:

- Download the currently active animation and sound from the doorbell
- Convert them into a reusable profile directory
- Save the result under:

```
/profiles/downloads/YYYYMMDD_HHMMSS
```

The timestamp is in UTC.

If a download fails, partial files are saved under:

```
/profiles/partial/YYYYMMDD_HHMMSS
```

Downloaded profiles can be:

1. Renamed or moved
2. Added to the `presets` list in `config.json`
3. Applied like any other preset

## Test Config (Button)

**Entity type:** Button  
**Purpose:** Verify SSH connectivity and upload capability

Pressing this button will:

- Connect to the doorbell using your configured SSH settings
- Upload a small test animation and sound file
- Apply them to the doorbell

This is useful during initial setup to confirm:

- SSH host and credentials are correct
- File upload is working
- The service can successfully modify doorbell assets

If the test succeeds, you know the service is properly configured and ready to use with real profiles.

If it fails, check:

- `ssh.host` in `config.json`
- The `UNIFI_PROTECT_RECOVERY_CODE` environment variable
- Network connectivity between the container and the doorbell

The **Status** and **Last Error** sensors will provide feedback if something goes wrong.

# Status & Diagnostic Sensors

These sensors help you monitor what the service is doing.

## Status

Shows the current state of the service.

Typical states include:

- Online
- Idle
- Uploading
- Downloading
- Error

If something appears stuck, check this sensor first.

## Last Asset Download

**Entity type:** Sensor  
**Device class:** Timestamp  
**Purpose:** Shows when the most recent successful asset download occurred

Home Assistant displays this as a relative time (for example: “5 minutes ago”).

### Attributes

This sensor also includes additional details:

- **directory**  
    The name of the directory created for the download  
    Example:  
    `20260218_214530`
- **full_path**  
    The full filesystem path where the downloaded assets were saved  
    Example:  
    `/profiles/downloads/20260218_214530`
    
These attributes are useful for:

- Locating the downloaded profile on disk
- Renaming or moving the directory
- Adding it to the `presets` section in `config.json`

## Last Error

**Entity type:** Sensor  
**Purpose:** Displays the most recent error message from the service

If something fails (upload, download, SSH connection, invalid profile, etc.), this sensor shows the error message.

### Attributes

Additional diagnostic details are available in the attributes:

- **code**  
    Numeric error code
- **name**  
    Internal error name
- **message**  
    Detailed error message describing what went wrong

These attributes are useful for troubleshooting and when opening GitHub issues.

If an operation fails, check:

1. The **Status** sensor
2. The **Last Error** message
3. The attribute details

# Availability

If the service goes offline (for example, the container stops), the device will automatically show as unavailable in Home Assistant.

When the service restarts, it will automatically come back online.


# Typical User Workflow

Most Home Assistant users will:

1. Create profiles under `/profiles/`  
2. Define presets in `config.json`
3. Restart the container
4. Use the **Presets** dropdown to switch doorbell themes

For users who prefer designing assets inside the UniFi Protect app:

1. Upload animation/sound in Protect  
2. Apply it
3. Press **Asset Download**
4. Rename the generated folder
5. Add it to presets
6. Restart the service
