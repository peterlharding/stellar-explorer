# 0003. A scientific exhibit, with provenance on every value

Date: 2026-10-03
Status: Accepted

## Context

The 100,000 Stars experiment describes itself as an artist's interpretation.
Stellar Explorer is meant to be trustworthy enough to learn from and to check against the literature, while also hosting fiction and a game.
Those two aims conflict unless measured data is kept visibly apart from everything else.

## Decision

Stellar Explorer is a scientific exhibit first.
Stars are placed, coloured and sized from catalogue values, every value links to its source, and the exhibit offers time and motion, analysis views and guided tours.

## Alternatives considered

- **An artistic visualisation.** Easier to make beautiful, but nothing in it could be relied on, and the starship simulator would lose its point.
- **A research tool without the exhibit.** Gaia Sky and TOPCAT already serve that need.

## Consequences

- The schema has a `source` table, and measurements are separate rows from objects. See [ARCHITECTURE.md, Schema principles](../ARCHITECTURE.md#schema-principles).
- Brightness and colour come from physics in the shader, not from per-star tuning.
- Where data is missing, for example radial velocity, the app says so rather than inventing a value.
- Fiction and hypothetical content live in separate layers that can be switched off.
