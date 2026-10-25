# 0008. The core library is written in C++

Date: 2026-10-25
Status: Accepted

## Context

The core from [0007](0007-stack-core-sqlite-godot.md) is loaded by Godot through GDExtension on every target, the Vision Pro included.
The two serious candidates were C++, using Godot's own `godot-cpp` bindings, and Rust, using [godot-rust](https://docs.rs/godot) (version 0.5.5 on 3 October 2026).
The Vision Pro is the riskiest target, and godot-rust describes its visionOS support as [experimental](https://github.com/godot-rust/gdext/pull/1696).

## Decision

Write the core in C++, built with CMake, and bind it to Godot with `godot-cpp`.

## Alternatives considered

- **Rust with godot-rust.** Memory safety and better tooling, and it has a double-precision option.
  It lost because its visionOS support is experimental, which would stack a second unknown on top of the phase 3 Vision Pro risk.
  C++ is the language of Godot itself and of Apple's Compositor Services examples, so it is the shortest path on every target.

## Consequences

- The core's build file is `src/core/CMakeLists.txt`, and its version lives in `project(... VERSION ...)`.
- C++ needs discipline that Rust would enforce: warnings as errors, sanitisers in the test build, and a formatter and linter from the first commit.
- If the core ever has to run outside Godot, for example in a native Metal shell, C++ links into it directly.
