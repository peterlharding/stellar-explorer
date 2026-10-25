# 0001. A native application for desktop, HTC Vive and Apple Vision Pro

Date: 2026-10-03
Status: Accepted

## Context

Stellar Explorer is meant to be flown through, both on a screen and in a headset.
The two headsets that matter are an HTC Vive, driven by a PC through SteamVR, and an Apple Vision Pro.
They share no XR runtime: the Vive speaks OpenXR, and the Vision Pro does not support OpenXR at all.
The app must also handle 64-bit positions, steady frame timing and a local database of a few hundred megabytes.

## Decision

Build a native application with three targets: desktop (Windows, Linux, macOS), HTC Vive and Apple Vision Pro.

## Alternatives considered

- **A browser build on WebXR.** One code base would reach both headsets, but it gives weaker control of numerical precision, frame timing and local data, and the requirement is a native app.
- **Desktop only.** Simpler, but loses the main point of the project, which is standing among the stars.
- **One headset only.** Halves the platform work, but the owner has both and wants both.

## Consequences

- Every feature is written against interaction intents, with a thin input layer per platform. See [ARCHITECTURE.md, VR targets](../ARCHITECTURE.md#vr-targets).
- The choice of engine must hide two XR back ends. That drove [0007](0007-stack-core-sqlite-godot.md).
- Vision Pro builds need a Mac with Xcode.
- Frame rate is measured in each headset before a feature is accepted.
