# Changelog

## [v1.1.0] - 2026-08-11

- Improved early reflection (ER) unit:
    - **Breaking:** ERs now decay at a fixed rate (previously, the ER decay was linked to the tail `decay` parameter, which could lead to very quiet/inaudible ERs). For certain settings, this change results in a higher level for the ER signal. Thus, the factory presets _Dark Room_ and _Live Room_ were updated.
    - Added a visualization of the ERs (additional page found in the envelope-gram display).
    - Fixed distorted/silent ER signals.
- Changed default values of parameters `pre delay`, `tail delay`, `echoes` and `decay` (now matching the _Default_ preset).
- Added support for fractional `drift` values.
- Fixed `tail delay` values below the minimum being written back to the parameter; the audio path clamps the values and the UI shows the effective value.
- Fixed AU validation (`auval` now succeeds, and the plugin is declared sandbox-safe), which some hosts require before loading the plugin.
- Fixed the minimum OS requirement for macOS: previously macOS 26.5, now macOS 11.0.
- Fixed behavior of the envelope follower in DAW bypass mode (fixes the mid-decay tail released on un-bypass).
- Fixed silent output when switching decay `mode` with extreme `tilt` values.
- Added CMake presets for macOS and Windows with debug and release configurations.


## [v1.0] - 2026-07-20
- Initial public release.

[Unreleased]: https://github.com/IIP-Group/FDverb/compare/v1.0...HEAD
[v1.1.0]: https://github.com/IIP-Group/FDverb/releases/tag/v1.1.0
[v1.0]: https://github.com/IIP-Group/FDverb/releases/tag/v1.0
