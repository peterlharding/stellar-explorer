# Changelog

All notable changes to Stellar Explorer are recorded in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html). The release process is in [RELEASING.md](RELEASING.md).

## [Unreleased]

### Added

- Architecture document, plan and ADR convention under `docs/`.
- Architecture decision records 0001 to 0010: native app on three targets, bright-star scale, scientific exhibit with provenance, queryable catalogue, relativistic starships, optional game layer, the Godot and SQLite stack, a C++ core, a personal tool, and a public repository with private scenarios.
- Release process in `RELEASING.md`, with `VERSION`, this changelog and the release notes guide in `release_notes/`.
- Catalogue pipeline in `src/pipeline/`: `make catalogue` builds `data/catalogue.sqlite` from HYG 4.4, the Hipparcos New Reduction and the NASA Exoplanet Archive, with a source, and uncertainties where known, on every value.
- `make check` (lint, type checks, unit tests, documentation links, version fields) and `make gate`, which checks the catalogue against published positions and distances of 27 well-known stars.
- `docs/DATA-MODEL.md`, and ADR 0011: star astrometry from the Hipparcos New Reduction rather than HYG.

[Unreleased]: https://github.com/peterlharding/stellar-explorer/commits/main
