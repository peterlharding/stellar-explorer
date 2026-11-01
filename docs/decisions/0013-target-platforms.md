# 0013. Target platforms: Windows x64, macOS arm64 and visionOS

Date: 2026-11-02
Status: Accepted

Refines [0001](0001-native-app-three-targets.md).

## Context

[0001](0001-native-app-three-targets.md) set three targets: desktop (Windows, Linux, macOS), the HTC Vive and the Apple Vision Pro. "Desktop" left the operating systems and CPU architectures open.

Under Unreal ([0012](0012-unreal-front-end.md)) each target is a separate binary, built per platform on its own operating system, so the build and test matrix has to be pinned down.
In practice the project is developed and used only on current Apple-silicon Macs and a Windows x64 PC (which also drives the Vive). There is no Linux machine and no Intel Mac in use.

## Decision

Build for these platforms only:

- **Windows x64** — the desktop build, which also drives the Vive through OpenXR (the same binary, not a separate artefact).
- **macOS on Apple silicon (arm64)** — the desktop build.
- **visionOS (arm64)** — the Vision Pro build.

Drop Linux, and drop Intel (x86-64) Macs. arm64 is assumed on the Apple platforms, x64 on Windows.

## Alternatives considered

- **Keep Linux as a desktop target** (0001's list). There is no Linux machine to build, run or test on, and the Vive runs on the Windows build, so Linux would add a third desktop binary with no user.
- **Universal macOS binaries** (arm64 + x86-64). No Intel Mac is in use; a universal build doubles the Mac build's time and size for a slice nobody runs.

## Consequences

- Three binaries — Windows x64, macOS arm64, visionOS — with the Vive folded into the Windows one; a smaller matrix for CI, releases and the headset checks.
- The code may assume arm64 on the Apple platforms and need carry no Intel-Mac or Linux build paths.
- Release artefacts ([RELEASING.md](../../RELEASING.md), step 13), the architecture diagram and the runtime notes list exactly these platforms.
- Reversible and additive: a Linux desktop build (it would run the Vive through OpenXR, as Windows does) or a universal Mac build can be added later without a redesign.
