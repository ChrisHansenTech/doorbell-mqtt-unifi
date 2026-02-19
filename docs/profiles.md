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
        "durationMs": 100,
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

Frame duration in milliseconds.

Example:

- `100` = each frame displays for 100ms
    

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
