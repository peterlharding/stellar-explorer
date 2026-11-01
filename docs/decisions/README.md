# Architecture decision records

One numbered file per settled decision: `NNNN-short-title.md`, for example `0001-native-app-three-targets.md`. Numbers are never reused, and a record is never rewritten once accepted; a later decision that changes it gets a new record that says which one it supersedes.

An ADR is added when a decision is settled, not when it is proposed. Open questions live in [../PLAN.md](../PLAN.md#open-questions) until then.

## Template

```markdown
# NNNN. Title

Date: YYYY-MM-DD
Status: Accepted | Superseded by NNNN

## Context

What forced a choice, and the constraints that applied.

## Decision

What was chosen, in one or two sentences.

## Alternatives considered

Each option and why it lost.

## Consequences

What this makes easy, what it makes hard, and what would cause it to be revisited.
```

## Records

| Number | Title | Status |
| --- | --- | --- |
| [0001](0001-native-app-three-targets.md) | A native application for desktop, HTC Vive and Apple Vision Pro | Accepted |
| [0002](0002-bright-star-scale-designed-to-grow.md) | Start at bright-star scale, designed to grow | Accepted |
| [0003](0003-scientific-exhibit-with-provenance.md) | A scientific exhibit, with provenance on every value | Accepted |
| [0004](0004-extensible-queryable-catalogue.md) | An extensible, queryable catalogue that includes exoplanets | Accepted |
| [0005](0005-relativistic-starships-and-scenarios.md) | Relativistic starships and scenarios | Accepted |
| [0006](0006-optional-game-layer.md) | A game layer, optional and on top | Accepted |
| [0007](0007-stack-core-sqlite-godot.md) | Engine-independent core, SQLite catalogue, Godot 4 front end | Accepted; its Godot front end to be superseded by 0012 (Unreal). The Vive test has now passed (2 November 2026); 0012 is written when `feature-star-panel` merges: see [PLAN.md](../PLAN.md) |
| [0008](0008-core-in-cpp.md) | The core library is written in C++ | Accepted |
| [0009](0009-personal-tool.md) | A personal tool, not a public installation | Accepted |
| [0010](0010-public-repository-private-scenarios.md) | A public repository, with book-based scenarios kept outside it | Accepted |
| [0011](0011-astrometry-from-hipparcos.md) | Star astrometry from the Hipparcos New Reduction, not HYG | Accepted |
