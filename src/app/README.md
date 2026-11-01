# Stellar Explorer — the application (`src/app`)

The Unreal Engine 5.8 front end ([ADR 0012](../../docs/decisions/0012-unreal-front-end.md)), grown from the [Unreal spike](../../spikes/unreal-stars/README.md). C++ only (no Blueprints): a game mode spawns the star field into the engine's empty map, and a pointer names and keeps stars. Targets Windows x64 (which also drives the Vive), macOS and visionOS ([ADR 0013](../../docs/decisions/0013-target-platforms.md)).

The spike's bring-up — how it reached full immersion on the Vision Pro and the Vive — is kept as a record in [`spikes/unreal-stars/`](../../spikes/unreal-stars/README.md), with [VISIONOS.md](../../spikes/unreal-stars/VISIONOS.md) and [VIVE.md](../../spikes/unreal-stars/VIVE.md).

## Build and run

`spike.sh` drives the Unreal tools — on a Mac, or on Windows from WSL (it calls the Windows programs with paths translated by `wslpath`). The checkout must be on a Windows drive (`/mnt/c`, `/mnt/d`).

```sh
cd /mnt/d/src/Projects/stellar-explorer   # on a Windows drive, not WSL's own filesystem
make catalogue                            # build data/catalogue.sqlite (once)
cd src/app
./spike.sh build && ./spike.sh material   # compile the editor target; generate the star material
./spike.sh run                            # fly the catalogue in a window (WASD + mouse)
./spike.sh vive                           # run in the headset (Windows, through SteamVR/VIVE runtime)
./spike.sh package                        # cook a standalone Win64 build (ships shaders; no first-run wait)
```

`./spike.sh` with no argument lists every command (`measure`, `look`, `visionos`, `install`, …). `UE_ROOT` overrides where the engine is.

## Data and the core

The app reads `data/catalogue.sqlite` (built by the pipeline in `src/pipeline`). It currently queries the `star` view through Unreal's own SQLite plugin; [ADR 0014](../../docs/decisions/0014-core-as-linked-library.md) moves catalogue access and the astrometry to the engine-independent core in [`src/core`](../core/README.md), which the app will link as a static library behind a C ABI. That wiring (a ThirdParty module + a C++ wrapper, swapping `StarField` onto the core) is the next step; the core itself is built and tested.

## Naming

The Unreal project and module are still called `StellarSpike` (the `.uproject`, the `Source/StellarSpike/` module, the game classes), carried over from the spike. Renaming them is a separate, optional step, kept apart from the graduation to avoid churn.
