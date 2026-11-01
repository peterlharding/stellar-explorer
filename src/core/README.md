# Stellar Explorer core

The engine-independent core ([ADR 0008](../../docs/decisions/0008-core-in-cpp.md)): the science and the catalogue, with no dependency on the engine. The front end links it as a static library through a **C ABI** ([ADR 0014](../../docs/decisions/0014-core-as-linked-library.md)); a thin C++ wrapper over that ABI lives on the Unreal side, not here.

## Layout

| Path | What |
| --- | --- |
| `include/stellar_core.h` | The public **C ABI** — the whole boundary. Nothing C++/STL/exception crosses it. |
| `src/` | Internal C++ (`stellar::core`): astrometry, catalogue access, and the ABI wrappers. |
| `tests/` | CTest reference cases, run without the engine. |
| `third_party/sqlite/` | The vendored SQLite amalgamation, compiled in. |

## Build and test (any platform)

Needs CMake ≥ 3.20 and a C++17 compiler. From the repo root:

```sh
make core        # configure, build, and run the tests (ctest)
```

or directly:

```sh
cmake -S src/core -B src/core/build
cmake --build src/core/build
ctest --test-dir src/core/build --output-on-failure
```

This is the standalone build the tests run in, and it is part of `make check`.

## Win64 static library (for Unreal)

The front end is Windows x64 ([ADR 0013](../../docs/decisions/0013-target-platforms.md)) built with MSVC, so the library it links must be MSVC x64. From WSL, drive the Windows CMake and the VS 2022 toolset (the VS generator finds the toolchain itself — no `vcvars` needed):

```sh
CMAKE="/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
"$CMAKE" -S "$(wslpath -w src/core)" -B "$(wslpath -w src/core/build-msvc)" -G "Visual Studio 17 2022" -A x64
"$CMAKE" --build "$(wslpath -w src/core/build-msvc)" --config Release
```

This produces, under `src/core/build-msvc/Release/`:

- `stellar_core.lib` — the core
- `sqlite3.lib` — the vendored SQLite it depends on

The ThirdParty Unreal module links **both** and adds `include/` to its include paths. (Matching Unreal's exact CRT and build flags is settled when that module is wired up.)

## macOS / visionOS

Built on the Mac with CMake and the Apple toolchain (`aarch64-apple-darwin`, and the visionOS SDK). Deferred until the Unreal wiring moves to the Mac.

## The boundary

`stellar_core.h` is the only public surface. Build the core with whatever toolchain suits each target; because the seam is C, the linked result is ABI-stable regardless (see [ADR 0014](../../docs/decisions/0014-core-as-linked-library.md)).
