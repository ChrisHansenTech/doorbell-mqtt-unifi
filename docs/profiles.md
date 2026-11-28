# Profiles

Profiles define the animation and sound assets that are uploaded to the UniFi G4 Doorbell Pro.

Each profile is a directory containing a `profile.json` file and the associated media assets.

Profiles live under:

```
/profiles/<profile-directory>/
```

The `<profile-directory>` must match the `directory` value configured in `config.json` under the `presets` section.

# Required Files

Each profile directory must contain:

- `profile.json`    
- One animation file (`.png`)
- One sound file (`.ogg` or `.wav`)

The filenames referenced in `profile.json` must exactly match the files present in the profile directory.

# Example `profile.json`

```json
{
    "schemaVersion": 1,
    "welcome": {
        "enabled": true,
        "file": "christmas.png",
        "count": 57,
        "durationMs": 3240,
        "loop": true,
        "guiId": "WELCOME"
    },
    "ringButton": {
        "enabled": true,
        "file": "christmas.ogg",
        "repeatTimes": 1,
        "volume": 100,
        "soundStateName": "RING_BUTTON_PRESSED"
    }
}
```

# Schema

## schemaVersion

Current version: `1`

Used to support future profile format changes. Profiles must match the supported schema version.

# Welcome Animation

Controls the animation shown on the doorbell display.

## welcome.enabled

- `true` → Upload and activate the custom animation.
- `false` → Remove the custom animation entry from the doorbell configuration.

When disabled, the doorbell falls back to its firmware default behavior.

## welcome.file

Name of the animation file in the profile directory.

Must be a `.png` file.

The PNG must be formatted correctly for the doorbell (sprite sheet style animation and each sprite is 240x240px).

## welcome.count

Number of frames in the animation.

This must match the number of frames encoded in the PNG sprite sheet.

Incorrect values may result in playback glitches.

## welcome.durationMs

Total animation duration in milliseconds (not per-frame).

The device distributes this time evenly across all frames.

Example:

- `3240` with 57 frames ≈ 17.6 FPS  
- `1900` with 57 frames ≈ 30 FPS  

To calculate a target frame rate:

totalDurationMs = (frameCount ÷ targetFPS) × 1000

Lower values increase animation speed.

## welcome.loop

- `true` → Animation loops continuously.    
- `false` → Animation plays once.

## welcome.guiId

GUI identifier used internally by the doorbell firmware.

This needs to be `WELCOME`

# Ring Button Sound

Controls the sound played when the doorbell button is pressed.

## ringButton.enabled

- `true` → Upload and activate custom ring sound.
- `false` → No sound configuration is modified.

Important:  
Disabling this does **not** restore a firmware default sound. The previously configured sound remains active.

UniFi Protect does not expose a true firmware default ring sound state.

## ringButton.file

Name of the audio file in the profile directory.

Supported formats:

- `.ogg` (recommended)    
- `.wav`

## ringButton.repeatTimes

Number of times the sound should repeat when triggered.

Example:

- `1` = play once    
- `2` = play twice

## ringButton.volume

Playback volume percentage.

Range:

- `1–100`
    
Actual perceived volume depends on doorbell hardware and firmware behavior.

## ringButton.soundStateName

Internal firmware state name for the sound trigger.

Must be set to `RING_BUTTON_PRESSED`

# Validation Rules

- `profile.json` must exist.
- All referenced files must exist in the same profile directory.
- Duplicate preset names are not allowed (enforced at config level).
- Missing required fields may cause profile load or upload failure.

# Design Philosophy

Profiles are intentionally:

- Self-contained
- Portable
- Versioned

You can copy a profile directory between systems without modifying the main configuration.

---

# Optional Default Assets & Precedence

When `defaultAssets` is configured and enabled in `config.json` (see [Configuration Reference](configuration.md#default-assets-section-defaultassets)), profiles no longer need to duplicate shared everyday assets.

## Example Shared Directory Structure

```text
profiles/
├── default-assets/
│   ├── default-welcome.png
│   └── default-ring.ogg
├── christmas/
│   ├── profile.json
│   └── christmas.png
├── halloween/
│   ├── profile.json
│   ├── halloween.png
│   └── halloween.ogg
└── birthday/
    ├── profile.json
    └── birthday.ogg
```

The shared default assets directory does not require a `profile.json`, and individual profile directories are never modified.

## Asset Selection & Precedence Rules

Welcome animation and ring button sound are resolved independently before validation, staging, and apply:

| Profile asset enabled | Global `defaultAssets.enabled` | Corresponding default enabled | Effective asset |
|---|---|---|---|
| Yes | Yes | Yes | Profile asset (profile takes precedence) |
| Yes | Yes | No | Profile asset |
| Yes | No | Any | Profile asset |
| No or omitted | Yes | Yes | Default asset (full default playback settings; disabled profile fields are not inherited) |
| No or omitted | Yes | No | Existing disabled-asset behavior |
| No or omitted | No | Any | Existing disabled-asset behavior |

### Important Notes

- **No fallback on broken profile assets**: If a profile explicitly sets `enabled: true` for an asset whose file is missing or invalid, the operation reports the profile asset error and does **not** fall back to the default asset.
- **Identical filenames**: A profile asset and a default asset may share the same filename with different contents; each apply uploads the selected file from its source directory with a freshly computed MD5.
- **Supported workflows**: Automatic substitution applies to **Presets**, **Custom directory**, **Test config**, and **Reapply last profile** in both **Legacy** and **IPC** apply modes. **SFX presets** and **Asset download** are unaffected.
- **Legacy vs. IPC when an asset remains disabled**: If neither the profile nor `defaultAssets` enables an asset, existing behavior is preserved: disabling `welcome` disables the custom welcome entry, whereas disabling `ringButton` leaves the doorbell's previously active ring sound unchanged in Legacy mode (and passes `enable: false` in IPC mode; UniFi Protect does not expose a factory ring sound restore). Configure both default assets for a predictable everyday appearance and ring tone.
