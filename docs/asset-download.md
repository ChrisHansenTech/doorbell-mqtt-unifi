# Asset Download

The Asset Download feature captures the currently active animation and sound from the doorbell and converts them into a reusable profile directory.

This allows you to:

1. Configure animation and sound in the UniFi Protect app    
2. Apply the configuration
3. Trigger an asset download
4. Reuse the generated profile with this service

# How UniFi Protect Processes Uploaded Media

When media is uploaded in the UniFi Protect app:

- JPEG, PNG, and GIF animations are converted internally to a **PNG sprite sheet**    
- GIF animations are flattened into a PNG with frame metadata
- MP3 or recorded audio is converted to an **OGG file**

The doorbell stores only the firmware-converted versions.  
The Asset Download feature retrieves those converted assets.

# Download Directory Structure

Completed downloads are saved to:

```
/profiles/downloads/YYYYMMDD_HHMMSS
```

Example:

```
/profiles/downloads/20260218_214530
```

The timestamp:

- Is generated at download time
- Uses **UTC**
- Ensures each download is uniquely versioned

Each completed directory contains:

- `profile.json`    
- The extracted animation PNG
- The extracted OGG sound file
- The original doorbell configuration `.conf` files (reference only)

# Included `.conf` Files

The download includes the doorbell’s original configuration files `ubnt_lcm_gui.conf` and `ubnt_leds_sounds.conf`.

These are provided for:

- Reference
- Inspection
- Troubleshooting
- Understanding how the firmware structured the configuration

They are **not required** to use the generated profile.

The service only depends on:

- `profile.json`    
- The referenced animation file
- The referenced sound file

The `.conf` files are informational and can be removed if not needed.

# Partial Downloads

If a download fails before completion, the partially retrieved assets are saved to:

```
/profiles/partial/YYYYMMDD_HHMMSS
```

This ensures:

- Completed profiles are never corrupted
- Incomplete transfers can be inspected
- Troubleshooting is possible without data loss

Partial downloads are not automatically converted into active profiles.

# Using a Downloaded Profile

After a successful download:

1. Review the generated directory under `/profiles/downloads/`.
2. Optionally rename or move it to a permanent location, for example:

```
/profiles/party_time/
```

3. Add the directory to the `presets` section in `config.json`:

```json
{
  "name": "Party Time",
  "directory": "party_time"
}
```

4. Restart the service to load the updated preset list.

The profile can now be selected and applied like any other preset.

# Design Intent

The Asset Download feature is designed to:

- Preserve firmware-generated configuration
- Standardize assets into service-compatible format
- Allow easy migration from Protect-managed customization
- Provide a safe, versioned workflow

Each download is timestamped and isolated to prevent accidental overwrites.
