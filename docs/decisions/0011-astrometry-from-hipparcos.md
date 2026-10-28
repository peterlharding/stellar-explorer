# 0011. Star astrometry from the Hipparcos New Reduction, not HYG

Date: 2026-10-25
Status: Accepted

## Context

The plan was to build the catalogue from HYG alone.
Profiling HYG 4.4 while writing the pipeline turned up three problems with its astrometry:

- Its J2000 positions were propagated from the Hipparcos epoch J1991.25 without cos(dec) in right ascension. Fast stars far from the equator are up to 18" out; Proxima Centauri and alpha Centauri A are among them.
- Barnard's Star's proper motion in declination has overflowed to 9999.99 mas/yr; the real value is about 10,330.
- It gives no uncertainties, and [0003](0003-scientific-exhibit-with-provenance.md) asks for them on every value.

HYG's distances come from the Hipparcos New Reduction (van Leeuwen 2007), and its proper motions from the original 1997 catalogue.

## Decision

For the HYG stars that Hipparcos observed, take positions, parallaxes and proper motions from the Hipparcos New Reduction (CDS I/311), with their uncertainties, and propagate the positions to J2000 in the pipeline.
HYG remains the source of names, identifiers, magnitudes, colours, spectral types and radial velocities, and of the astrometry of the 1,662 stars Hipparcos did not observe.
Where HYG deliberately departs from the Hipparcos parallax (7 stars, alpha Centauri B among them), HYG's distance is preferred.

## Alternatives considered

- **HYG alone, with corrections.** The right ascension error can be undone arithmetically and Barnard's Star patched by hand, but that leaves no uncertainties and makes the pipeline second-guess HYG instead of reading the measurements HYG was built from.
- **Gaia DR3 now.** Far better astrometry for most stars, but saturated for the brightest and a much larger ingest; it belongs with the scale-up in phase 7, and Gaia DR4 is due on 2 December 2026.

## Consequences

- Positions of Hipparcos stars agree with SIMBAD's J2000 positions to 0.2" or better, and every parallax and proper motion has an uncertainty.
- The pipeline reads one more file, 9 MB, unchanged since 2008 and pinned by SHA-256.
- Every source's claim is kept, so the catalogue still shows what HYG says, with the overflowed value marked invalid.
- Revisit when Gaia DR4 is ingested: it should replace Hipparcos as the preferred astrometry for the stars it covers.
