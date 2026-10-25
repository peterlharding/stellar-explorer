# 0005. Relativistic starships and scenarios

Date: 2026-10-03
Status: Accepted

## Context

One reason for the project is to model journeys like those in Alastair Reynolds' books, where slower-than-light ships cross between real nearby stars.
In those stories the gap between time on board and time in the wider world drives the plot.
That only works if the ship physics is right and the ships move against the real star map.

## Decision

The core simulates starships with exact special relativity.
A ship has a worldline made of legs, and the app tracks three clocks: master time, ship time and the time at which news arrives.
Journeys are described in scenario files that live in a fiction layer, attached to real host stars.

## Alternatives considered

- **Newtonian ships.** Simpler, but wrong at the speeds that matter and blind to time dilation, which is the point.
- **Hard-coded journeys.** Quicker for one story, but every new scenario would need a code change.

## Consequences

- The relativity module gets reference tests from day one, starting with the 10.5 light-year, 1 g trip in [ARCHITECTURE.md](../ARCHITECTURE.md#three-clocks).
- Scenario content based on published books is someone else's work. Where it is kept is settled in [0010](0010-public-repository-private-scenarios.md).
