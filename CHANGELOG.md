# Changelog

All notable changes to Stellar Explorer are recorded in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html). The release process is in [RELEASING.md](RELEASING.md).

## [Unreleased]

### Fixed

- The app's control service listens again when the app is restarted soon after quitting with a console connected; before, the port stayed blocked for about half a minute and no console could reach the app. A second app on the same port is still refused.
- `scripts/make_macos.sh` builds the core libraries when run (`mac`, `visionos`, or both by default) and runs the macOS tests; before, it only defined the build functions, and was not executable.

## [0.1.0] - 2026-11-04

See [release_notes/v0.1.0.md](release_notes/v0.1.0.md) for details.

### Added

- Architecture document, plan and ADR convention under `docs/`.
- Architecture decision records 0001 to 0010: native app on three targets, bright-star scale, scientific exhibit with provenance, queryable catalogue, relativistic starships, optional game layer, the Godot and SQLite stack, a C++ core, a personal tool, and a public repository with private scenarios.
- Release process in `RELEASING.md`, with `VERSION`, this changelog and the release notes guide in `release_notes/`.
- Catalogue pipeline in `src/pipeline/`: `make catalogue` builds `data/catalogue.sqlite` from HYG 4.4, the Hipparcos New Reduction and the NASA Exoplanet Archive, with a source, and uncertainties where known, on every value.
- `make check` (lint, type checks, unit tests, documentation links, version fields) and `make gate`, which checks the catalogue against published positions and distances of 27 well-known stars.
- `docs/DATA-MODEL.md`, and ADR 0011: star astrometry from the Hipparcos New Reduction rather than HYG.
- ADRs 0012 to 0014: Unreal as the front end in place of Godot, the target platforms (Mac, Vision Pro, Windows with the Vive), and the core as a library linked into the app.
- The C++ core in `src/core/`, a static library behind a C ABI: astrometry, catalogue access through vendored SQLite, and voice matching, with its tests in `make check`.
- The Unreal app in `src/app/`, graduated from the spike: the catalogue's stars at their positions and colours, pointing and keeping a star by hand, controller or mouse, and a panel naming the star.
- Voice commands, by Apple's on-device recogniser on the Mac and the Vision Pro: "Stellar, go to Sirius", "go there", "back off", "closer", "return to the Sun"; spoken forms of star names in the catalogue.
- A desktop console in the app's window: the state, a command box, buttons, the log, Blender-style mouse navigation, and steering a headset's app over the local network, found by Bonjour.
- On the headset, the panel can be carried by a grab bar, and has an actions list for travel without speaking.
- The release version is shown beside the build stamp in the log, the window title, the console and the headset's panel, and set on the Mac and visionOS app bundles.
- `scripts/make_macos.sh`, with `build_macos` and `build_visionos` shell functions that build the core libraries for the Mac and visionOS, and notes on building and running the app on a Mac.
- The core refuses a catalogue built for another schema, and the app says which schema it found; `make version-check` fails if the pipeline and the core disagree on the schema.
- The app quits with an error when the prebuilt core library was built for another interface version than the app.

### Changed

- Stars load in under a second, reading the catalogue's tables directly rather than through the `star` view.

### Fixed

- The control service carries out a command sent just before the console closes its connection, such as a `quit` piped through `nc`.

[Unreleased]: https://github.com/peterlharding/stellar-explorer/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/peterlharding/stellar-explorer/releases/tag/v0.1.0
