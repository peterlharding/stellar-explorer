# Stellar Explorer documentation

All project documentation lives in this directory. The repository root keeps only `README.md`, `RELEASING.md`, `CHANGELOG.md`, `VERSION` and `LICENSE`. Drafts of issues for other projects live in [../issues/](../issues/README.md).

| Document | What it covers |
| --- | --- |
| [ARCHITECTURE.md](ARCHITECTURE.md) | The design: goals, prior art, stack recommendation, system architecture, data model, rendering, time and motion, analysis views, starships and relativity, tours and gamification, VR targets, sources |
| [DATA-MODEL.md](DATA-MODEL.md) | The catalogue database: sources, what the pipeline does with each, preferred values, tables, querying, and the phase 1 gate |
| [PLAN.md](PLAN.md) | The roadmap with phase gates, risks, what has been decided, and the open questions |
| [decisions/](decisions/README.md) | Architecture decision records (ADRs), one per settled decision |
| [BACKLOG.md](BACKLOG.md) | Features wanted but not yet scheduled into a phase |
| [notes/](notes/) | Dated notes from planning discussions, whose outcomes are folded into the plan and the backlog |

Related files outside this directory:

- [../RELEASING.md](../RELEASING.md): the release process and commit conventions.
- [../CHANGELOG.md](../CHANGELOG.md): what changed in each release.
- [../release_notes/](../release_notes/README.md): user-facing notes, one file per release.

## Conventions

- **One topic per file**, named in capitals with hyphens (`ARCHITECTURE.md`, `DATA-MODEL.md`). Split a section out of `ARCHITECTURE.md` into its own file when it outgrows a screen or two, and leave a link behind.
- **Design in `ARCHITECTURE.md`, intentions in `PLAN.md`, settled choices in `decisions/`.** An open question lives in `PLAN.md` until it is settled; then it becomes an ADR and leaves the plan.
- **Diagrams are Mermaid blocks and formulas are `math` blocks**, so they render on GitHub and diff as text. No binary images unless a screenshot is the point.
- **Facts from the web carry a link and a date.** Engine and headset support changes quickly; a claim without a date cannot be rechecked.
- **Documentation changes are their own commits**, prefixed `docs:`, and are not folded into release commits. See [../RELEASING.md](../RELEASING.md#commit-conventions).
