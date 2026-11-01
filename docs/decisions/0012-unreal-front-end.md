# 0012. Unreal Engine 5.8 as the front end, replacing Godot

Date: 2026-11-02
Status: Accepted

Supersedes [0007](0007-stack-core-sqlite-godot.md).

## Context

[0007](0007-stack-core-sqlite-godot.md) chose a Godot 4 front end on the engine-independent C++ core ([0008](0008-core-in-cpp.md)) and the SQLite catalogue ([0004](0004-extensible-queryable-catalogue.md)).
Its one real risk was that Godot's fully immersive Vision Pro support was new in 2026, so phase 3 of the [roadmap](../PLAN.md#roadmap) was set as a go or no-go on it.

Rather than carry that risk to phase 3, an [Unreal spike](../../spikes/unreal-stars/README.md) built the real thing — the whole catalogue rendered, with a pointer and a provenance panel — in Unreal Engine 5.8.4, C++ only, across all three targets.
It reached full immersion on the Vision Pro (28–30 October 2026, logged with its evidence in [VISIONOS.md](../../spikes/unreal-stars/VISIONOS.md)) and then ran on the HTC Vive Pro 2, controllers pointing and selecting (30 October–2 November 2026, in [VIVE.md](../../spikes/unreal-stars/VIVE.md)).
Unreal was agreed as the engine on 30 October; this record waited for the Vive test so it could cite that evidence too.

## Decision

Unreal Engine 5.8 is the front end, replacing Godot 4.
The other two parts of [0007](0007-stack-core-sqlite-godot.md) stand unchanged: the engine-independent C++ core owns all the science, and the catalogue stays in SQLite, built offline by the Python pipeline.
Only the front-end engine changes.

Use stock launcher builds of the engine for now, not a source build.

## Alternatives considered

- **Godot 4 ([0007](0007-stack-core-sqlite-godot.md))'s choice.** Its fully immersive Vision Pro support was unproven; keeping it meant betting phase 3 on a risk the Unreal spike has already cleared.
- **Unity 6.** The most established Vision Pro path, but immersive apps need a paid licence and the engine is closed (as in 0007).
- **A native Metal shell.** Most control, but the UI, input and tooling would all have to be written.
- **A source build of Unreal, now.** It would unlock foveation, the system control, the splash window and faster fixes, but at the cost of a large download, long builds and keeping up with Epic's releases. Deferred (see Consequences), not rejected.

## Consequences

- Phase 3's go/no-go on Godot's Vision Pro support is retired: the spike reached full immersion at about 95 fps with every star, and the Vive holds the rest of phase 3's gate.
- The engine-independent core ([0008](0008-core-in-cpp.md)) and the SQLite catalogue carry over untouched; 0007 always intended them to survive a change of shell, and this is that change.
- The launcher engine ships precompiled, so foveation, the system's in-view control, the splash window and multiview need engine-source changes. Accepted for now, with a source build held in reserve if sharpness proves to matter in use; tracked in [PLAN.md](../PLAN.md#risks) and [BACKLOG.md](../BACKLOG.md#known-engine-limits-on-vision-pro).
- Parts of [ARCHITECTURE.md](../ARCHITECTURE.md) still describe the Godot stack; they are updated as the application is built from the spike (phase 2), not all at once here.
- Revisit if standalone Vision Pro performance at full catalogue scale proves inadequate, or if the launcher engine's limits become unacceptable before a source build is justified.
