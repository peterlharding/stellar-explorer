# 0009. A personal tool, not a public installation

Date: 2026-10-25
Status: Accepted

## Context

An exhibit for the public needs a kiosk mode, an attract loop that plays when nobody is using it, protection against visitors changing settings, and careful licence handling for anything shown.
A tool for its owner needs none of that.

## Decision

Stellar Explorer is a personal tool, for its owner's own exploration and scenarios.

## Alternatives considered

- **Design for a public installation from the start.** Adds a kiosk mode, an attract loop and unattended operation to every phase, for an audience that is not planned.

## Consequences

- No kiosk mode and no attract loop.
- "Exhibit mode" still means the app with the game layer switched off, as in [0006](0006-optional-game-layer.md).
- Licence obligations still apply when a database built from HYG or AT-HYG is redistributed (CC BY-SA 4.0), but not to the app running locally.
- Revisit if the app is ever to be shown unattended.
