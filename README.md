# HOT4D-C4D2026Plus

HOT4D has now been **adapted and brought up successfully for Cinema 4D 2026** in this repository.

This repository is a continuation/adaptation of the original HOT4D project, with the goal of keeping the plugin usable on newer Cinema 4D SDKs while preserving the original architecture and license.

## Upstream source

This work is based on the original HOT4D project:

- Upstream repository: https://github.com/Valkaari/HOT4D
- Original repository owner: Valkaari

HOT4D itself is based on the Houdini Ocean Toolkit and related upstream work referenced by the original project.

## License

This repository remains available under **GNU GPL v2.0**, following the upstream license.

See:

- `LICENSE.txt` for the retained license text
- the upstream repository for original notices and attribution

## Current status

**Cinema 4D 2026 adaptation: complete and working.**

The current port has been verified to the point where:

- the plugin builds successfully against the current Cinema 4D 2026 SDK-based setup,
- the ocean simulation object creation path works again,
- the deformer updates correctly during playback,
- Jacobian / foam related functionality has been restored to a working state,
- the point-selection crash path has been fixed,
- the Maxon registration bootstrap has been fixed and made reproducible.

In short: this repo is no longer just an investigation branch; it is a working Cinema 4D 2026 adaptation of HOT4D.

For a deeper engineering write-up of the lessons learned during the port, see:

- `BUILDING.md`

## Repository layout

The code structure remains close to upstream:

- `source/` — plugin and ocean simulation source code
- `res/` — Cinema 4D resources, descriptions, strings, icons
- `project/` — Cinema 4D module build metadata and project-local CMake override
- `scripts/` — local helper scripts for SDK-backed bring-up
- `docs/` — migration and analysis notes

## Build workflow

The intended workflow is the Cinema 4D SDK's **top-level CMake** flow, using this repo as an external module through `MAXON_SDK_CUSTOM_PATHS_FILE`.

A helper script is included for Windows:

- `scripts/Configure-HOT4D-C4D2026SDK.ps1`

Typical usage:

```powershell
pwsh -File .\scripts\Configure-HOT4D-C4D2026SDK.ps1 `
  -SdkRoot 'C:\path\to\Cinema_4D_CPP_SDK_2026_1_0' `
  -Build
```

For detailed build notes, registration details, and porting lessons, see:

- `BUILDING.md`

## Notes

This repository is an adaptation/maintenance continuation and is **not** the original upstream project.
If you need original history and context, start here:

https://github.com/Valkaari/HOT4D
