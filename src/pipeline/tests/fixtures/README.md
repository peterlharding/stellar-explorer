# Test fixtures

Small excerpts of the real source files, chosen to cover the cases the pipeline must handle: the Sun, Barnard's Star (an overflowed proper motion in HYG), alpha Centauri B (HYG overrides a bad Hipparcos parallax), a star with no usable parallax, companions, a shared HD number, and planet hosts matched by HIP, HD, Gliese number, or not at all.

| File | Excerpt of | Licence |
| --- | --- | --- |
| `hyg.csv` | HYG Database 4.4, Astronexus (uncompressed) | CC BY-SA 4.0 |
| `hip2.dat` | Hipparcos, the New Reduction, van Leeuwen 2007, CDS I/311 | VizieR rules of usage: free for scientific use with citation |
| `pscomppars.csv` | NASA Exoplanet Archive, Planetary Systems Composite Table, doi:10.26133/NEA13, retrieved 25 October 2026 | Free to use with acknowledgement |

`../reference/simbad.csv` holds published reference values from SIMBAD for the gate tests; `../reference/fetch_reference.py` regenerates it.
