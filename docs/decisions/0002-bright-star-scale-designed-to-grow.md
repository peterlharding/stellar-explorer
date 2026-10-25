# 0002. Start at bright-star scale, designed to grow

Date: 2026-10-03
Status: Accepted

## Context

The experience to match is the 100,000 Stars Chrome experiment, built on about 120,000 stars from the HYG database.
Catalogues go far beyond that: AT-HYG holds 2.55 million stars, and Gaia holds about 1.8 billion sources.
Handling the full scale from the start would make level of detail and streaming the first problems to solve, ahead of anything a user can see.

## Decision

Version 1 uses the HYG catalogue, about 120,000 stars, held in memory as one buffer.
The data model, the coordinate handling and the core interface are designed so the same code grows to AT-HYG and then to a Gaia octree without being rewritten.

## Alternatives considered

- **Start with AT-HYG.** Ten times the data for little gain in the first phases, and spatial chunking would be needed from day one.
- **Start with Gaia.** Makes streaming level of detail the first problem, and Gaia Sky's own notes report that this performs poorly in a headset.

## Consequences

- No level of detail is needed in phases 1 to 6.
- 64-bit positions and camera-relative rendering are needed from the start anyway, because precision is a problem at every scale.
- Scale-up is phase 7 of the [roadmap](../PLAN.md#roadmap), with its own gate.
