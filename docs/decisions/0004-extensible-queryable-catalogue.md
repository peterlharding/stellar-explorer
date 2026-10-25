# 0004. An extensible, queryable catalogue that includes exoplanets

Date: 2026-10-03
Status: Accepted

## Context

The app is meant to answer questions as well as show pictures: "G dwarfs within 15 parsecs with known planets" should be something a user can ask.
New catalogues, user objects and fictional overlays will arrive over the life of the project.
Exoplanets are part of the exhibit, attached to their host stars.

## Decision

The catalogue is a database first.
It can be queried with SQL inside and outside the app, it accepts new sources, tables and layers without code changes, and it includes planetary systems from the NASA Exoplanet Archive.

## Alternatives considered

- **A flat file of render-ready points.** Fast to load, but cannot be queried and cannot carry provenance.
- **A server database.** More power than needed at this scale, and it cannot run on the Vision Pro.

## Consequences

- The storage choice is settled in [0007](0007-stack-core-sqlite-godot.md): SQLite, read-only for the catalogue and writable for user content.
- A `dataset` table tells the app how to draw and label kinds of data it has never seen.
- The pipeline cross-matches identifiers so names from different catalogues resolve to one object.
