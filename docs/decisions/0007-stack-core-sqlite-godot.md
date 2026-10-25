# 0007. Engine-independent core, SQLite catalogue, Godot 4 front end

Date: 2026-10-25
Status: Accepted

## Context

[0001](0001-native-app-three-targets.md) requires one app on desktop, the Vive (OpenXR) and the Vision Pro (Compositor Services and Metal, no OpenXR).
[0004](0004-extensible-queryable-catalogue.md) requires a queryable database that runs on all of those platforms, the Vision Pro included.
The engines were compared on 3 October 2026, and the comparison is in [ARCHITECTURE.md, Stack recommendation](../ARCHITECTURE.md#stack-recommendation).

## Decision

Build three parts:

- A core library with no engine dependency, which owns all the science.
- A catalogue in SQLite, built offline by a Python pipeline.
- A front end in Godot 4, which loads the core through GDExtension and only draws and listens.

Use stock Godot builds: the core does 64-bit maths and hands the renderer camera-relative 32-bit positions.

## Alternatives considered

- **Unity 6.** The most established Vision Pro path, but immersive apps need a paid Pro, Enterprise or Industry licence, and the engine is closed.
- **Unreal 5.** Too heavy for a point-cloud app, and sources disagree on how mature its Vision Pro support is.
- **A custom renderer.** Most control, but the UI, input and tooling would all have to be written too.
- **Bevy.** No Vision Pro path was found.
- **PostgreSQL or Parquet for the catalogue.** Right for Gaia scale on a workstation, but they cannot run on the headset.

## Consequences

- The core can be unit-tested on its own, without the engine.
- Godot's fully immersive Vision Pro support is new in 2026, which is the stack's one real risk.
  Phase 3 of the [roadmap](../PLAN.md#roadmap) is a go or no-go on it.
- If Godot fails on the Vision Pro, the core, the database and the tours carry over to a native Metal shell, or to Unity.
- Revisit for the bulk catalogue at Gaia scale, which may move to Parquet or PostgreSQL while SQLite keeps the metadata and overlays.
