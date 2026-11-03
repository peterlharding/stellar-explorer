# Spike: a Rust core behind the C ABI

**Status: parked.** Scaffolding only; revisit after the Vive work has progressed.

## The question

[ADR 0014](../../docs/decisions/0014-core-as-linked-library.md) makes the core a static library behind a **C ABI**, with a thin C++ wrapper on the Unreal side. Unreal links the library and does not care what language produced it, so Rust (`crate-type = ["staticlib"]`, `#[no_mangle] extern "C"`) is a drop-in alternative to a C++ core.

The only real uncertainty is **visionOS**. So the go/no-go is:

> Can a Rust static library with a C ABI be linked into the Unreal project and **cooked and run on all three targets — Windows x64, macOS arm64, and visionOS (device *and* simulator)**?

Windows and macOS arm64 are well-supported Rust targets; **visionOS is Tier 3** (no prebuilt `std`, built from source with a nightly toolchain and `-Z build-std`). If visionOS clears, Rust-for-the-core is genuinely on the table and would be settled by a new ADR revisiting [0008](../../docs/decisions/0008-core-in-cpp.md). If it fights, the C++ core stands and little is lost.

This is a feasibility probe, not the real core: the library here exists only to be linked and called.

## What's here

- `Cargo.toml` — a `staticlib` crate, `panic = "abort"` (panics must not unwind across the FFI boundary).
- `src/lib.rs` — two `extern "C"` functions: an ABI version, and parallax→distance (the phase-1 reference maths, as a non-trivial smoke test).
- `include/stellar_core.h` — the C header the Unreal side includes.
- `build.sh` — per-target `cargo` invocations, including the visionOS nightly/`build-std` form.

## Method

1. **Build the static library** for a target (see `build.sh`). Build `win` on Windows; `mac`, `visionos`, `visionos-sim` on the Mac.
   - visionOS needs a nightly toolchain and the std source: `rustup toolchain install nightly` and `rustup component add rust-src --toolchain nightly`.
2. **Wire it into Unreal** as a ThirdParty module (sketch below), pointing `PublicAdditionalLibraries` at the per-platform artefact and `PublicIncludePaths` at `include/`. Use the existing [unreal-stars](../unreal-stars/README.md) project, or a throwaway one.
3. **Call it** from a game module: `stc_distance_parsecs(100.0)` must return `10.0` (100 mas → 10 pc); log it at start-up.
4. **Cook and package** for each target and run — the **visionOS cook is the real test**, on both device and simulator, mirroring the steps in [VISIONOS.md](../unreal-stars/VISIONOS.md).

## Success criteria

- The library links into Unreal and the call returns the right value on **Windows x64, macOS arm64, and visionOS (device + sim)**.
- A packaged build on each target starts and logs the expected distance.

## What to record (as the work is done, like VISIONOS.md / VIVE.md)

- Toolchain versions (Rust stable + nightly, the visionOS SDK), and the exact `build-std` incantation that worked.
- Any **system libraries Unreal's link step needed adding** for Rust's `std` (Windows commonly needs `ntdll`, `userenv`, `ws2_32`, `bcrypt`; Apple platforms may need frameworks).
- Panic/unwind behaviour across the boundary with `panic = "abort"`.
- Binary-size and build-time deltas versus a C++ core.
- Anything visionOS-specific that broke or was fiddly.

## The Unreal ThirdParty module (sketch)

```csharp
using UnrealBuildTool;
using System.IO;

public class StellarCoreFFI : ModuleRules
{
    public StellarCoreFFI(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));
        string Lib = Path.Combine(ModuleDirectory, "lib", Target.Platform.ToString());
        if (Target.Platform == UnrealTargetPlatform.Win64)
            PublicAdditionalLibraries.Add(Path.Combine(Lib, "stellar_core_spike.lib"));
        else // Mac, VisionOS
            PublicAdditionalLibraries.Add(Path.Combine(Lib, "libstellar_core_spike.a"));
        // Record here any extra system libraries Rust's std pulls in at link time.
    }
}
```
