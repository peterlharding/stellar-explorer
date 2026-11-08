# Stellar Explorer - the application (`src/app`)

The Unreal Engine 5.8 front end ([ADR 0012](../../docs/decisions/0012-unreal-front-end.md)), grown from the [Unreal spike](../../spikes/unreal-stars/README.md). C++ only (no Blueprints): a game mode spawns the star field into the engine's empty map, and a pointer names and keeps stars. Targets Windows x64 (which also drives the Vive), macOS and visionOS ([ADR 0013](../../docs/decisions/0013-target-platforms.md)).

The spike's bring-up - how it reached full immersion on the Vision Pro and the Vive - is kept as a record in [`spikes/unreal-stars/`](../../spikes/unreal-stars/README.md), with [VISIONOS.md](../../spikes/unreal-stars/VISIONOS.md) and [VIVE.md](../../spikes/unreal-stars/VIVE.md).

## Build and run

`stellar.sh` drives the Unreal tools - on a Mac, or on Windows from WSL (it calls the Windows programs with paths translated by `wslpath`). The checkout must be on a Windows drive (`/mnt/c`, `/mnt/d`).

```sh
cd /mnt/d/src/Projects/stellar-explorer   # on a Windows drive, not WSL's own filesystem
make catalogue                            # build data/catalogue.sqlite (once)
cd src/app
./stellar.sh build && ./stellar.sh material   # compile the editor target; generate the star material
./stellar.sh run                            # the catalogue in a window: click a star, drag to look, scroll to move; Tab for the console
./stellar.sh console HOST                   # the same, its console panel steering the app on HOST (a headset)
./stellar.sh vive                           # run in the headset (Windows, through SteamVR/VIVE runtime)
./stellar.sh package                        # cook a standalone Win64 build (ships shaders; no first-run wait)
```

`./stellar.sh` with no argument lists every command (`measure`, `look`, `visionos`, `install`, …). `UE_ROOT` overrides where the engine is.

## Data and the core

The app reads `data/catalogue.sqlite` (built by the pipeline in `src/pipeline`) through the engine-independent core in [`src/core`](../core/README.md), which it links as a static library behind a C ABI ([ADR 0014](../../docs/decisions/0014-core-as-linked-library.md)): `StarField` loads stars via the core (the `StellarCore` module and the `FStarCatalogue` wrapper), not the engine's SQLite plugin.

**Build the core's static library for the target platform before building the app** - Win64, macOS or visionOS - following [src/core/README.md](../core/README.md). The `StellarCore` module links it and fails with a clear message if it is missing.

## Naming

The Unreal project, module and game classes are named `StellarExplorer` - the `.uproject`, the `Source/StellarExplorer/` module, `AStellarExplorerGameMode` and the rest - matching the app and the repository. They were renamed from the `StellarSpike` of the original spike on 8 November 2026; the build script is `stellar.sh`.
