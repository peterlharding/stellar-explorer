# Stellar Explorer: Plan

The roadmap, the risks and the questions still open. The design itself is in [ARCHITECTURE.md](ARCHITECTURE.md). When a question here is settled it becomes an ADR in [decisions/](decisions/README.md) and is removed from this file. Features wanted but not yet placed in a phase are in [BACKLOG.md](BACKLOG.md).

> **Engine: Unreal, pending ADR 0012.** On 30 October 2026 Unreal Engine was agreed as the engine, replacing Godot ([ADR 0007](decisions/0007-stack-core-sqlite-godot.md)), on the evidence of the [Unreal spike](../spikes/unreal-stars/README.md). ADR 0012 is written after the spike's Vive test, so it can cite that too; until then ADR 0007 stays on record, and parts of [ARCHITECTURE.md](ARCHITECTURE.md) still describe Godot. See the [direction notes](notes/2026-10-30-DIRECTION.md).

## Roadmap

Seven phases, each ending in something usable. A phase is finished when its gate passes, not when its features exist.

```mermaid
flowchart LR
    p1["1 Data<br/>foundation"] --> p2["2 Desktop<br/>app"] --> p3["3 Headsets"] --> p4["4 Time and<br/>analysis"] --> p5["5 Ships"] --> p6["6 Tours<br/>and play"] --> p7["7 Scale-up"]
```

| Phase | Delivers | Gate |
| --- | --- | --- |
| 1 Data foundation | Ingest pipeline; `catalogue.sqlite` with stars, planets and sources | Distances and positions for a test list of well-known stars match published values; every row has a source |
| 2 Desktop app | The Unreal spike grown into the application, on the C++ core: star renderer, fly-through, picking, provenance panel | The full 120,000-star catalogue renders smoothly |
| 3 Headsets | Vive through OpenXR; the Vision Pro build | The Vive build holds its refresh rate; the Vision Pro build reaches feature parity with the desktop |
| 4 Time and analysis | Star motion, filters, HR diagram, SQL console | Time scrubbing, HR diagram, filters and SQL console all drive one shared selection |
| 5 Ships | Worldlines, three clocks, course plotter, bridge view | Core tests reproduce known relativistic travel times; a ship follows a multi-leg course on the time slider |
| 6 Tours and play | Tours, missions, discovery log | One complete narrated tour and one mission chain run on all three platforms |
| 7 Scale-up | AT-HYG at 2.5 million stars, then a Gaia octree | 2.5 million stars hold headset frame rate; the octree design is proven on a Gaia sample |

**Progress.** Phase 1's gate passes as of 25 October 2026: `make catalogue` builds the catalogue and `make gate` checks it (see [DATA-MODEL.md](DATA-MODEL.md#the-gate)).
The [Unreal spike](../spikes/unreal-stars/README.md) renders the whole catalogue at 120 fps on the Mac and about 95 fps in full immersion on the Vision Pro (30 October 2026), which retires phase 3's old go/no-go on the Vision Pro. Its Vive test is next, then a spike on voice commands.

## Risks

With the Vision Pro shown working in the spike, no remaining risk is expected to change the stack; what is left is scope, tuning, and the limits of the engine as Epic ships it.

| Risk | Why it matters | Mitigation |
| --- | --- | --- |
| The launcher engine's limits on Vision Pro | Foveation (sharpness), the system's control in view, the splash window and multiview are in engine code that ships precompiled ([spike history](../spikes/unreal-stars/VISIONOS.md)) | Accept them for now; build Unreal from source if sharpness proves to matter in use |
| Standalone Vision Pro performance | Far less GPU than a PC | Per-platform star budget; Mac-rendered streaming as a second route |
| Scope | Exhibit, database, simulator and game are four products | Phases with gates; each phase ships something usable on its own |
| Catalogue licence | HYG and AT-HYG are CC BY-SA 4.0 | Keep attribution in the `source` table and show it in the app; publish any redistributed database under the same licence |
| Fiction content | Book names and events are someone else's work | Scenarios live in a separate private repository ([ADR 0010](decisions/0010-public-repository-private-scenarios.md)) |

## Decided

Each settled decision is an ADR in [decisions/](decisions/README.md).

| ADR | Decision | Date |
| --- | --- | --- |
| [0001](decisions/0001-native-app-three-targets.md) | A native application for desktop, HTC Vive and Apple Vision Pro | 3 October 2026 |
| [0002](decisions/0002-bright-star-scale-designed-to-grow.md) | Start at bright-star scale (HYG, about 120,000 stars), designed to grow | 3 October 2026 |
| [0003](decisions/0003-scientific-exhibit-with-provenance.md) | A scientific exhibit, with provenance on every value | 3 October 2026 |
| [0004](decisions/0004-extensible-queryable-catalogue.md) | An extensible, queryable catalogue that includes exoplanets | 3 October 2026 |
| [0005](decisions/0005-relativistic-starships-and-scenarios.md) | Relativistic starships and scenarios | 3 October 2026 |
| [0006](decisions/0006-optional-game-layer.md) | A game layer, optional and on top | 3 October 2026 |
| [0007](decisions/0007-stack-core-sqlite-godot.md) | Engine-independent core, SQLite catalogue, Godot 4 front end | 25 October 2026 |
| [0008](decisions/0008-core-in-cpp.md) | The core library is written in C++ | 25 October 2026 |
| [0009](decisions/0009-personal-tool.md) | A personal tool, not a public installation | 25 October 2026 |
| [0010](decisions/0010-public-repository-private-scenarios.md) | A public repository, with book-based scenarios kept outside it | 25 October 2026 |
| [0011](decisions/0011-astrometry-from-hipparcos.md) | Star astrometry from the Hipparcos New Reduction, not HYG | 25 October 2026 |

## Open questions

- [ ] **Vision Pro mode.** Must the build run standalone, or is rendering from a Mac acceptable?
- [ ] **Vive hardware.** Which Vive model, and what graphics card drives it?
- [ ] **Display units.** Light-years for visitors and parsecs in analysis views, or one unit throughout?
