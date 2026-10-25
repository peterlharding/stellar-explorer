# stellar-explorer

An application to engage in stellar exploration both from a scientific perspective but also a gaming one.

I would like to be able to concoct scenarios to replicate some of the trips imagined in Alastair Reynolds books.

## Status

Design stage. The repository holds documentation and the release process; there is no code yet, and `VERSION` is `0.0.0`. The first phase of the [plan](docs/PLAN.md#roadmap) is the data pipeline.

The intended shape: a native 3D and VR application for desktop, HTC Vive and Apple Vision Pro, built on a queryable catalogue of real stars and exoplanets, with a relativistic starship simulator, guided tours and an optional game layer on top.

## Documentation

All documentation lives in [`docs/`](docs/README.md). New documents go there, not in the repository root.

| Document | What it covers |
| --- | --- |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | The design: goals, stack recommendation, system architecture, data model, rendering, time and motion, starships and relativity, tours, VR targets |
| [docs/PLAN.md](docs/PLAN.md) | Roadmap with phase gates, risks, decisions made, and open questions |
| [docs/decisions/](docs/decisions/README.md) | Architecture decision records, one per settled decision |
| [docs/README.md](docs/README.md) | Index of the above and the documentation conventions |

How the documents relate: the design is in `ARCHITECTURE.md`, intentions and open questions are in `PLAN.md`, and a question that gets settled becomes a numbered record in `docs/decisions/`. Diagrams are Mermaid and formulas are `math` blocks, so both render on GitHub.

## Repository layout

| Path | Holds |
| --- | --- |
| `docs/` | All documentation |
| `src/` | Source code, to come: `src/pipeline/` (Python ingest), `src/core/` (core library), `src/app/` (Godot project) |
| `release_notes/` | One user-facing file per release |
| `RELEASING.md` | The release process and commit conventions |
| `CHANGELOG.md` | What changed in each release |
| `VERSION` | The canonical version number |

## Releasing

The process is in [RELEASING.md](RELEASING.md). In short: one semantic version for the whole repository in `VERSION`, a hand-maintained [CHANGELOG.md](CHANGELOG.md), user-facing notes in [`release_notes/`](release_notes/README.md), and an annotated tag `v<version>` on a commit that has passed every check. Commit messages carry a short prefix (`feat:`, `fix:`, `docs:`, `data:` and so on); documentation changes are committed on their own with `docs:`.

## Licence

The code is under the MIT licence; see [LICENSE](LICENSE). Catalogue data is not stored in this repository. The HYG and AT-HYG star catalogues are CC BY-SA 4.0, so any database built from them and redistributed carries that licence and their attribution.
