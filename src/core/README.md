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

## macOS and visionOS static libraries (for Unreal)

Built on the Mac with the **Xcode generator**, into `<build>/Release/` to match the Win64 layout. The app's ThirdParty module (`src/app/Source/StellarCore/StellarCore.Build.cs`) expects, per [ADR 0013](../../docs/decisions/0013-target-platforms.md):

- macOS (Apple silicon): `src/core/build-mac/Release/{libstellar_core.a, libsqlite3.a}`
- visionOS (device): `src/core/build-visionos/Release/{libstellar_core.a, libsqlite3.a}`

### macOS (arm64)

```sh
cmake -S src/core -B src/core/build-mac -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build src/core/build-mac --config Release
ctest --test-dir src/core/build-mac -C Release --output-on-failure   # tests run natively
```

### visionOS (device)

```sh
cmake -S src/core -B src/core/build-visionos -G Xcode \
  -DCMAKE_SYSTEM_NAME=visionOS -DCMAKE_OSX_SYSROOT=xros -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DSTELLAR_CORE_TESTS=OFF
cmake --build src/core/build-visionos --config Release
```

Needs CMake ≥ 3.28 (visionOS support) and the visionOS SDK. Tests are off because their executables are device binaries that cannot run on the host. If the Unreal link later complains, match UE's visionOS **deployment target** (add `-DCMAKE_OSX_DEPLOYMENT_TARGET=<version>`). For the **simulator**, use `-DCMAKE_OSX_SYSROOT=xrsimulator` into `build-visionos-sim/` and wire that path in `StellarCore.Build.cs` when the simulator is targeted.

This is the least-trodden path — expect some iteration — but the C ABI seam ([ADR 0014](../../docs/decisions/0014-core-as-linked-library.md)) means it is only a matter of producing a valid arm64 archive; nothing in the boundary depends on the toolchain.

## The boundary

`stellar_core.h` is the only public surface. Build the core with whatever toolchain suits each target; because the seam is C, the linked result is ABI-stable regardless (see [ADR 0014](../../docs/decisions/0014-core-as-linked-library.md)).
