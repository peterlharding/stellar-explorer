# 0006. A game layer, optional and on top

Date: 2026-10-03
Status: Accepted

## Context

Discovery is more engaging with goals and rewards: a log of things found, missions, navigation challenges.
But a game must not make the science less trustworthy, or hide data behind progress.

## Decision

Game mechanics are an optional layer played over the same engine as tours.
They watch events published by the core and never change the science layer.
Progress unlocks tools, never data, and exhibit mode runs with the game switched off.

## Alternatives considered

- **A game first.** Would push the design towards invented content and away from the catalogue.
- **No game at all.** Loses the navigation puzzles that the relativity core makes possible.

## Consequences

- The core publishes events (object visited, selection made, course plotted, time changed).
- Missions are saved queries plus a condition, so they are data, not code.
- Game work is phase 6, after the science it depends on.
