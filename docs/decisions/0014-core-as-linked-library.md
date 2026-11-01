# 0014. The core as a standalone library linked into the Unreal front end

Date: 2026-11-02
Status: Accepted

Relates to [0008](0008-core-in-cpp.md) and [0012](0012-unreal-front-end.md).

## Context

[0008](0008-core-in-cpp.md) puts all the science in an engine-independent C++ core that can be unit-tested without the engine.
[0012](0012-unreal-front-end.md) makes Unreal the front end; the GDExtension mechanism that [0007](0007-stack-core-sqlite-godot.md) relied on to load the core into Godot is gone with Godot.
So how the core is consumed has to be decided afresh for Unreal.

Today there is no core. The [spike](../../spikes/unreal-stars/README.md) reads the catalogue with Unreal's own SQLite plugin and does the astrometry inline in the game module (`StarField`, `StellarSpikeGameMode`). Phase 2 of the [plan](../PLAN.md#roadmap) is to extract the core and define the boundary.

Two facts constrain the choice:

- Unreal builds with UnrealBuildTool (not CMake), unity builds, exceptions and RTTI off by default, and its own STL-free containers.
- Under [0013](0013-target-platforms.md) each target is a separate binary: Windows x64 (MSVC), macOS arm64 (clang), visionOS arm64 (clang + the visionOS SDK, device and simulator).

## Decision

- Build the core as a **standalone C++ static library with CMake** (`src/core`, as [0008](0008-core-in-cpp.md) names), with its own unit tests (CTest) run independently and as part of `make check`.
- Unreal links the prebuilt library through a **thin ThirdParty module** (a `Build.cs` with `PublicAdditionalLibraries` and `PublicIncludePaths`). The game module calls the core and owns no science.
- The boundary is a **C ABI**: the one header the engine includes is `extern "C"` — opaque handle types (incomplete structs, only pointers cross), POD structs and pointer-plus-count spans, and explicit create/destroy for ownership. No C++ types, STL or exceptions cross it, so the core links cleanly whatever compiler and flags built it — which matters most on visionOS, where matching Unreal's exact clang is the awkward part. C has a stable ABI on every target; C++ does not.
- On top of that seam, a **thin C++ wrapper lives in the Unreal module** (compiled by UnrealBuildTool, under Unreal's own flags), presenting the call site RAII and Unreal types (`TArray`, `FString`) over the C functions. So there is a stable C ABI at the link boundary and C++ ergonomics at the call site, with no ABI mismatch on either side.
- **SQLite is compiled into the core** (the amalgamation); Unreal's SQLite plugin is dropped from the app.
- The core does **64-bit** world maths; positions cross the boundary as doubles, and the Unreal side rebases them to the camera each frame and downcasts to 32-bit (the camera-relative note from [0007](0007-stack-core-sqlite-godot.md)).
- Build the library for **each [0013](0013-target-platforms.md) target**. Start by prebuilding per platform and pointing `Build.cs` at the matching artefact; automate it (let `Build.cs` drive the CMake build) later if the manual step chafes.

## Alternatives considered

- **Compile the core's sources directly as an Unreal module** (UBT builds them). Simpler wiring, but it couples the core's build to Unreal, loses the standalone CMake and test build — breaking [0008](0008-core-in-cpp.md)'s "testable without the engine" — and fights Unreal's build settings.
- **A C++ class at the seam** (a disciplined facade: non-virtual, non-inline, POD-only signatures). Pleasanter than C, but it forces the core to be built with the same compiler and ABI as Unreal on every platform, and leans on flag discipline rather than a guaranteed-stable ABI. The C seam plus an engine-side wrapper gives the same call-site ergonomics without that constraint.
- **Let C++/STL types cross the boundary** (no facade at all). Fewer wrappers, but fragile across the ABI, allocator and build-setting mismatch between a plain-C++ library and Unreal, and it leaks SQLite and STL into engine code.
- **Keep catalogue access in the engine** (the spike's status quo, on Unreal's SQLite plugin). Nothing to build now, but it leaves the science in the engine against [0008](0008-core-in-cpp.md), and would have to be redone for any other front end.
- **A dynamic library or plugin** (GDExtension-style). That was Godot's model; a static library avoids shipping and loading a separate binary and suits a single-front-end app.

## Consequences

- [0008](0008-core-in-cpp.md)'s promise holds: the core compiles and its reference tests (known star distances, known relativistic travel times, known sky positions) run without Unreal, inside `make check` and the phase-1 gate.
- One extra build artefact per platform (the core library), with the per-target toolchain matching that [0013](0013-target-platforms.md) already constrains; visionOS is the fiddly one.
- The C ABI costs some boilerplate at the seam and the engine-side wrapper, but keeps engine and science cleanly separable and lets the core be built with its own toolchain per platform, independent of Unreal's compiler — a second front end, or a command-line tool, could link the same core.
- First slice to prove it: move catalogue access and astrometry into `src/core` behind the facade, build on Win64 and macOS, wire the ThirdParty module, and make `StarField` call the core instead of Unreal's SQLite; visionOS next.
- Revisit if per-target build matching proves too costly (fall back to building the core as an Unreal module), or if a plain-C++ boundary turns out manageable within Unreal's settings.
