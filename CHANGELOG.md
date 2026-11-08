# Changelog

All notable changes to Stellar Explorer are recorded in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html). The release process is in [RELEASING.md](RELEASING.md).

## [Unreleased]

## [0.5.0] - 2026-11-08

See [release_notes/v0.5.0.md](release_notes/v0.5.0.md) for details.

### Added

- Drive the Vive's actions menu from the controller, without pointing at the panel. The right controller's menu button opens the list of commands (Go there, Back off, Closer, Return to the Sun, Let go, Hide info, Quit); scroll it with the trackpad and choose with the trackpad's click, or point the ray at an item and click. Quit is now on the menu, so holding both triggers is no longer the only way out.
- A visible wand on each Vive controller: a thin rod along the controller's aim, with the ray running on from its tip, so the held hand is easy to keep track of. Dimmer than the ray and colour-coded like a ship's navigation lights — the left hand red for port, the right green for starboard. Its length and thickness are tunable with `-WandLength=` and `-WandRadius=`, and `-NoWand` hides it.
- A view-radius dial on the Vive's left controller: its menu button or grip opens a block, opposite the star panel, with a slider showing how far out stars are shown and the cutoff in words. The left trackpad slides it — eased for fine control and holding still when you lift off — and the trackpad's click shows every star or returns to the last radius. The Vive now reaches the radius of visibility that the Vision Pro's hand slider and voice already did.

### Fixed

- The info panel's opening prompts ("Pull the trigger", and the like) no longer run past the right edge of the frame.

See [release_notes/v0.4.1.md](release_notes/v0.4.1.md) for details.

### Fixed

- The app now builds on Windows. The 0.4.0 source compiled on the Mac (Clang) but not on Windows (MSVC), where stricter warnings are treated as errors: two names in the console and a voice command left uninitialised. No change to how the app behaves or looks; the Mac and Vision Pro builds are unaffected.

## [0.4.0] - 2026-11-06

See [release_notes/v0.4.0.md](release_notes/v0.4.0.md) for details.

### Added

- "Stellar, backtrack" (or "go back", "take me back", "undo"): back to where the last journey began, with the star kept then kept again; said again, to where the one before began, for up to 50 journeys. Journeys are "go to", "go there", "back off", "closer" and "return to the Sun", by voice, the menus or the console, which has a Backtrack button.

### Changed

- The core's interface version is 7: rebuild the core libraries after pulling (`scripts/make_macos.sh`, and `build-msvc` on Windows).

## [0.3.0] - 2026-11-05

See [release_notes/v0.3.0.md](release_notes/v0.3.0.md) for details.

### Added

- Show only the stars near you: everything further than a radius you choose is hidden, and the stars near its edge fade, so the space around you can be seen on its own. The radius moves with you as you travel, and stars beyond it cannot be pointed at.
  - On the Vision Pro, a slider under the hand menu's buttons, set by touching it or by looking at it, pinching and sliding the hand; or pinch with both hands and move them apart to see further out, or together to see less. Holding both pinches still quits, as before.
  - By voice: "Stellar, show more" (twice as far out), "show less" (half as far), "show within 20 light years" (or parsecs, or just "show 20 light years"), "show everything". After "show within" it waits for the distance, and a moment for its unit.
  - In the desktop console, a slider and Show more, Show less and Show everything buttons. Steering a headset, the console shows the same stars as it does.
- On the Vision Pro, the window shown while the app starts is this app's: "Stellar Explorer", the version and build, and "Opening the sky…", in place of Unreal's developer text (its name for the window, the immersion settings and a "Go Immersive!" countdown).
- "Stellar, quit" or "Stellar, exit" by voice (also "close the app"): the app asks for it again, and quits if "quit" is said again within 8 seconds, with or without "Stellar".

### Changed

- The core's interface version is 6: rebuild the core libraries after pulling (`scripts/make_macos.sh`, and `build-msvc` on Windows).
- The panel's build line is shorter ("local-space, 5 Nov 12:15"), so a long branch name no longer runs past the frame.
- In a headset the countdown to quitting shows once both hands have been pinched for half a second, so it does not flash up as a two-hand gesture begins.

### Fixed

- Quitting the desktop console no longer takes a minute or more after the headset app has closed: it waited for an attempt to reconnect, which now gives up after 2 seconds.

## [0.2.1] - 2026-11-05

See [release_notes/v0.2.1.md](release_notes/v0.2.1.md) for details.

### Changed

- On the Vision Pro the pointing hand no longer casts a ray: look at a star and pinch to keep it, and look at it and pinch twice to let it go. `bHandRay=True` in `DefaultGame.ini`, or `-HandRay` on the command line, brings the ray back. The Vive's controllers keep theirs.

## [0.2.0] - 2026-11-05

See [release_notes/v0.2.0.md](release_notes/v0.2.0.md) for details.

### Added

- On the Vision Pro, a hand menu: touch the button on the back of the other hand's wrist with the pointing hand's index finger, and a column of buttons opens beside it, pressed the same way: Go there, Back off, Closer, Return to the Sun, Let go, and Hide info or Show info. A dot shows where the fingertip is. The menu stays where it opened, upright and facing the viewer, until the wrist button is touched again. (Not the palm turned up, which brings up visionOS's Home button.)
- On the Vision Pro, look and pinch: look at a star and pinch to keep it, at a hand menu button to press it, or at the panel, its bar or the hand menu's bar and pinch and move the hand to move it, as visionOS's own windows work.
- A button beside the panel's bar that hides the panel, and under the hand menu a bar to move it by and a button that closes it, pressed by look and pinch (or, under the hand menu, by a touch).
- "Hide info" and "Show info", spoken, typed in the console, on its buttons, or on the menus: the panel describing the star stays hidden until shown again. Also "hide the panel", "show details" and other ways of saying it.

### Changed

- In a headset the panel goes after 3 seconds with no star pointed at or kept, and comes back as soon as one is.
- On the Vision Pro the "..." button and its list above the panel are gone, replaced by the hand menu; with the Vive's controllers they stay, with Hide info added.
- A spoken command after the wake word is acted on once it has stood for 0.4 seconds (0.8 with a star's name), even while other people nearby go on talking; before, it waited for all speech to stop.
- The panel's build line is shorter ("v0.1.0, build f2dd258+changes"), so it fits inside the frame.
- The core's interface version is 5: rebuild the core libraries after pulling (`scripts/make_macos.sh`, and `build-msvc` on Windows).

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

[Unreleased]: https://github.com/peterlharding/stellar-explorer/compare/v0.5.0...HEAD
[0.5.0]: https://github.com/peterlharding/stellar-explorer/releases/tag/v0.5.0
[0.4.1]: https://github.com/peterlharding/stellar-explorer/releases/tag/v0.4.1
[0.4.0]: https://github.com/peterlharding/stellar-explorer/releases/tag/v0.4.0
[0.3.0]: https://github.com/peterlharding/stellar-explorer/releases/tag/v0.3.0
[0.2.1]: https://github.com/peterlharding/stellar-explorer/releases/tag/v0.2.1
[0.2.0]: https://github.com/peterlharding/stellar-explorer/releases/tag/v0.2.0
[0.1.0]: https://github.com/peterlharding/stellar-explorer/releases/tag/v0.1.0
