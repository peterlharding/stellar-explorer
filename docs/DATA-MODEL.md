# Stellar Explorer: Data model and pipeline

How `catalogue.sqlite` is built, what is in it, and how to query it.
The principles behind it are in [ARCHITECTURE.md, Data and database](ARCHITECTURE.md#data-and-database); this file is the concrete version.
Facts about the source catalogues were checked on 25 October 2026.

## Building the catalogue

The pipeline is a Python package in `src/pipeline/`, run through [uv](https://docs.astral.sh/uv/).
From the repository root:

```sh
make catalogue   # download the sources into data/raw/, then build data/catalogue.sqlite
make gate        # check the built catalogue against published values
make check       # lint, type checks, unit tests, documentation links, version fields
```

`make catalogue` downloads about 25 MB the first time and builds in about 35 seconds.
The downloads are recorded in `data/raw/manifest.json` with their URL, SHA-256 and retrieval time.
HYG and Hipparcos are fixed files whose SHA-256 is pinned in the code, so a changed download fails rather than slipping in.
The exoplanet archive changes weekly, so it is downloaded once and kept; `cd src/pipeline && uv run stellar-pipeline fetch --refresh` downloads it again.

The build writes to a temporary file and moves it into place only after its integrity checks pass.
A failed build leaves the previous catalogue untouched.
The catalogue is never edited in place: a change to the sources or the pipeline means a rebuild.

## Sources

| Source | Version | What it supplies | Licence |
| --- | --- | --- | --- |
| [HYG Database](https://codeberg.org/astronexus/hyg) | 4.4 (12 July 2026) | 119,614 stars plus the Sun: names and identifiers, V magnitudes, colours, spectral types, radial velocities, multiple-star links, and astrometry for the 1,662 stars Hipparcos did not observe | CC BY-SA 4.0 |
| [Hipparcos, the New Reduction](https://cdsarc.cds.unistra.fr/viz-bin/cat/I/311) (van Leeuwen 2007) | CDS I/311, files of 15 September 2008 | Positions, parallaxes and proper motions with their uncertainties, for the 117,951 HYG stars it covers | Free for scientific use with citation |
| [NASA Exoplanet Archive](https://exoplanetarchive.ipac.caltech.edu/), Planetary Systems Composite Table ([doi:10.26133/NEA13](https://doi.org/10.26133/NEA13)) | Retrieved at build time; 6,451 planets on 25 October 2026 | Planets, with a reference for every parameter, and their host stars | Free to use with acknowledgement |

HYG has moved from GitHub to Codeberg; the GitHub repository stops at version 4.1.
Version 4.4 is the current release; the architecture's mention of 4.2 was out of date.
Why the astrometry comes from Hipparcos rather than HYG is in [ADR 0011](decisions/0011-astrometry-from-hipparcos.md).

## What the pipeline does with each source

### HYG

HYG uses placeholders that must not reach the catalogue as values:

- **A distance of 100,000 pc means "unknown".** HYG uses it for 10,224 stars whose parallax is 1 mas or less. Their absolute magnitudes and luminosities are computed from the placeholder, so they are dropped too.
- **A radial velocity of exactly 0 means "unknown"**, for all stars but the Sun. Only 22,635 stars have a real one.
- **Proper motions of ±9999.99 mas/yr are overflowed fields.** Only Barnard's Star has one: its declination proper motion is 10,328 mas/yr, which does not fit. HYG's value is kept, marked invalid, and never preferred.

Two more findings, which the pipeline works around rather than repairs:

- **HYG's J2000 right ascensions are wrong for fast stars far from the equator.** HYG moved the Hipparcos J1991.25 positions to J2000 by adding the proper motion in right ascension without dividing by cos(dec). Under that model HYG's positions match Hipparcos to a median of 0.01"; under the correct model they are out by up to 18" (Proxima Centauri, alpha Centauri A).
- **HYG overrides Hipparcos for 7 stars.** For alpha Centauri B, for example, the Hipparcos solution (796.92 mas) is known to be bad and HYG uses alpha Centauri A's distance. The pipeline detects these as a HYG distance that differs from the Hipparcos parallax by more than 0.1%, and prefers HYG's distance, with a note saying so.

### Hipparcos

Positions are at epoch J1991.25 and are propagated to J2000.0 by the pipeline, in three dimensions, including the change of distance where the radial velocity is known.
A colour index of 0.000 with an error of 0.000 means none was measured.
Distances are derived as 1000 / parallax, only for parallaxes above 1 mas, matching HYG's cut at 1,000 pc.
The uncertainties on a derived distance are the distances at parallax ∓ one standard error, so they are asymmetric; the upper one is missing when the parallax is within one error of zero.

### NASA Exoplanet Archive

Each planet is matched to a host star in HYG by, in order: HIP number, HD number (the primary, where a multiple system shares one), a Gliese number in the host name, then position.
A position match must be within 2" for a star with Hipparcos astrometry or 30" for one without (HYG's Gliese-catalogue positions can be tens of arcseconds out), and the V magnitudes must agree within 1.
On 25 October 2026 this matched 911 of the 4,847 hosts; the other 3,936, mostly faint Kepler and TESS stars, become stars of their own, described by the archive.
RR Cae is a doubtful case left unmatched: it lies 52" from GJ 2034 with the same magnitude.

The archive takes most positions from TIC v8, which takes them from Gaia DR2 at epoch J2015.5.
The pipeline takes them to be at J2015.5 and propagates them to J2000.
For the hosts also in Hipparcos, this agrees with the Hipparcos positions to a median of 0.01".
A few fast nearby stars are exceptions, because TIC v8 took their positions from 2MASS at about J2000.

## Preferred values

Every claim from every source is kept.
For each quantity of each object exactly one value is marked `preferred`, chosen by source in this order:

1. A value the pipeline derived from preferred inputs.
2. Hipparcos New Reduction.
3. HYG.
4. NASA Exoplanet Archive.

There are two exceptions.
The 7 HYG distance overrides described above win over the distance derived from the Hipparcos parallax.
And the temperature the pipeline estimates from B-V is a rough blackbody estimate, so it is made only where no source measured a temperature.
A value marked `invalid` is never preferred.
The build fails if any quantity ends up with no preferred value or with two.

## Tables

All positions and velocities are in the ICRS equatorial frame with the Sun at the origin: +x towards RA 0h Dec 0, +y towards RA 6h Dec 0, +z towards the north celestial pole.
Preferred positions are at the reference epoch J2000.0.

| Table | One row per | Notes |
| --- | --- | --- |
| `source` | Catalogue, plus one for the pipeline itself | Name, version, licence, URL, citation, download URL, retrieval time, SHA-256 |
| `object` | Star or planet | `kind`, display `name`, `parent_id` (a planet's host, or a companion star's primary), `layer_id`, `source_id` (who says it exists) |
| `identifier` | Designation of an object, per source | `catalogue` and `value`, case-insensitive; see below |
| `measurement` | Numeric claim | `value`, asymmetric `err_lower` and `err_upper`, `limit_flag` (1 upper limit, -1 lower limit), `epoch`, `derivation_id`, `preferred`, `invalid`, `note`, and for archive values `reference_label` and `reference_url` |
| `property` | Text claim | Spectral type, constellation, discovery method, mass provenance, planet letter |
| `quantity` | Measured quantity | Unit and description; the units are listed there, not repeated per row |
| `derivation` | Formula the pipeline uses | With a version number, bumped when the formula changes |
| `layer` | Layer of content | One so far: `catalogue`, of kind `observed`. Fiction and hypothetical layers come with scenarios |
| `dataset` | Kind of thing the app can show | `stars` and `exoplanets` |
| `object_kind` | Kind of object | `star`, `planet` |
| `build_info` | Fact about the build | Schema version, pipeline version, build time, reference epoch |

Identifier catalogues are written the way SIMBAD writes them, so that a name typed by a user, an archive host name and a HYG designation find the same star: `HIP`, `HD`, `HR`, `GJ` (Gl and GJ share one numbering), `Bayer` (`eps Eri`, `tet1 Ori`), `Flamsteed` (`61 Cyg`), `GCVS`, `Gaia DR3`, `HYG`, `name` (proper names, and "Sun"), and `NASA Exoplanet Archive` (host and planet names).

The schema version is stored as `PRAGMA user_version` and in `build_info`.
It is defined as `SCHEMA_VERSION` in `src/pipeline/stellar_pipeline/schema.py` and must be bumped whenever a table or column changes.

### Views

Two views give one row per object with preferred values only:

- `star`: `ra`, `dec`, `parallax`, `distance`, `pmra`, `pmdec`, `rv`, `x`, `y`, `z`, `vx`, `vy`, `vz`, `vmag`, `absmag`, `b_v`, `teff`, `spectral_type`, and `primary_id` for companions.
- `planet`: orbit, radius, mass, equilibrium temperature, discovery year and method, and the host's id and name.

The `star` view pivots about two million rows and takes about two seconds in full; filter on `measurement` directly when that matters.

## Querying

The file opens in the `sqlite3` shell, DBeaver or Python.
Some examples:

```sql
-- G dwarfs within 15 parsecs that have known planets.
SELECT s.name, round(s.distance, 2) AS pc, s.spectral_type, count(p.id) AS planets
FROM star AS s JOIN planet AS p ON p.host_id = s.id
WHERE s.distance < 15 AND s.spectral_type LIKE 'G%V%'
GROUP BY s.id ORDER BY s.distance;

-- Everything known about one star, with where each value came from.
SELECT m.quantity, m.value, m.err_lower, m.err_upper, m.epoch, src.name, m.preferred, m.note
FROM identifier AS i
JOIN measurement AS m ON m.object_id = i.object_id
JOIN source AS src ON src.id = m.source_id
WHERE i.catalogue = 'name' AND i.value = 'Barnard''s Star'
ORDER BY m.quantity, m.preferred DESC;

-- Resolve a designation: any of 'Ran', 'eps Eri' or HIP 16537 finds the same star.
SELECT DISTINCT o.id, o.name FROM identifier AS i JOIN object AS o ON o.id = i.object_id
WHERE (i.catalogue, i.value) IN (VALUES ('Bayer', 'eps Eri'), ('HIP', '16537'));
```

## Size

The catalogue is about 215 MB, mostly the 3.2 million measurement rows: every source's claim is kept with its uncertainty and provenance.
That is acceptable at this scale.
At AT-HYG scale (phase 7) the app will need a compact render table built from the preferred values, and the full measurement table may stay on the desktop only.

## The gate

The phase 1 gate in [PLAN.md](PLAN.md#roadmap) is `make gate`, which runs `src/pipeline/tests/test_gate.py` on the built catalogue:

- Positions of 27 well-known stars agree with SIMBAD's J2000 positions within 0.5" (Hipparcos stars) or 10" (stars with only HYG positions, such as Wolf 359).
- Their distances agree with SIMBAD's within 1%, or their parallaxes within three combined standard errors (needed for alpha Centauri A, where Hipparcos and SIMBAD's adopted value are 2.9σ apart).
- Every row of every table has a source; every source has a licence, a citation and the SHA-256 of what was downloaded; every derived value names its derivation.
- Every HYG star and every archive planet is present, and every planet has a host.
- Directions agree with HYG's own x, y, z for Hipparcos stars (median under 0.05"), apart from HYG's right ascension error.
- Archive host positions, moved from J2015.5 to J2000, agree with the Hipparcos positions.

The reference values are in `src/pipeline/tests/reference/simbad.csv`, written by `fetch_reference.py` beside it.
