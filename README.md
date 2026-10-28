# stellar-explorer

An application to engage in stellar exploration both from a scientific perspective but also a gaming one.

I would like to be able to concoct scenarios to replicate some of the trips imagined in Alastair Reynolds books.

## Status

Phase 1 of the [plan](docs/PLAN.md#roadmap), the data foundation, has passed its gate. The pipeline builds a SQLite catalogue of 123,550 stars and 6,451 exoplanets from HYG, the Hipparcos New Reduction and the NASA Exoplanet Archive, with a source on every value. Nothing has been released yet, and `VERSION` is `0.0.0`. Phase 2, the desktop app, is next.

The intended shape: a native 3D and VR application for desktop, HTC Vive and Apple Vision Pro, built on a queryable catalogue of real stars and exoplanets, with a relativistic starship simulator, guided tours and an optional game layer on top.

## Documentation

All documentation lives in [`docs/`](docs/README.md). New documents go there, not in the repository root.

| Document | What it covers |
| --- | --- |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | The design: goals, stack recommendation, system architecture, data model, rendering, time and motion, starships and relativity, tours, VR targets |
| [docs/DATA-MODEL.md](docs/DATA-MODEL.md) | The catalogue database: sources, how the pipeline treats them, tables, querying |
| [docs/PLAN.md](docs/PLAN.md) | Roadmap with phase gates, risks, decisions made, and open questions |
| [docs/decisions/](docs/decisions/README.md) | Architecture decision records, one per settled decision |
| [docs/README.md](docs/README.md) | Index of the above and the documentation conventions |

How the documents relate: the design is in `ARCHITECTURE.md`, intentions and open questions are in `PLAN.md`, and a question that gets settled becomes a numbered record in `docs/decisions/`. Diagrams are Mermaid and formulas are `math` blocks, so both render on GitHub.

## Repository layout

| Path | Holds |
| --- | --- |
| `docs/` | All documentation |
| `src/pipeline/` | The Python ingest pipeline that builds the catalogue |
| `src/` | Still to come: `src/core/` (C++ core library), `src/app/` (Godot project) |
| `scripts/` | Repository tooling: documentation link check, version fields |
| `data/` | Downloaded catalogues and the built `catalogue.sqlite`; git-ignored |
| `issues/` | Drafts of issues to report to other projects, such as errors found in source catalogues |
| `release_notes/` | One user-facing file per release |
| `RELEASING.md` | The release process and commit conventions |
| `CHANGELOG.md` | What changed in each release |
| `VERSION` | The canonical version number |

## Building the catalogue

You need [uv](https://docs.astral.sh/uv/) and `make`. From the repository root:

```sh
make catalogue   # download the sources (about 25 MB) and build data/catalogue.sqlite
make gate        # check it against published positions and distances
make check       # lint, type checks, unit tests, documentation links, version fields
```

The result opens in any SQLite tool; [docs/DATA-MODEL.md](docs/DATA-MODEL.md#querying) has example queries.

## Releasing

The process is in [RELEASING.md](RELEASING.md). In short: one semantic version for the whole repository in `VERSION`, a hand-maintained [CHANGELOG.md](CHANGELOG.md), user-facing notes in [`release_notes/`](release_notes/README.md), and an annotated tag `v<version>` on a commit that has passed every check. Commit messages carry a short prefix (`feat:`, `fix:`, `docs:`, `data:` and so on); documentation changes are committed on their own with `docs:`.

## Licence

The code is under the MIT licence; see [LICENSE](LICENSE). Catalogue data is not stored in this repository, apart from small excerpts used as test fixtures (see `src/pipeline/tests/fixtures/README.md`). The HYG and AT-HYG star catalogues are CC BY-SA 4.0, so any database built from them and redistributed carries that licence and their attribution.
