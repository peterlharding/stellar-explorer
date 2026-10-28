# HYG 4.4: right ascensions propagated to J2000 without cos(dec)

Draft issue for [astronexus/hyg](https://codeberg.org/astronexus/hyg/issues).
Not posted.
See [README.md](README.md) before posting.

---

**Title:** HYG 4.4: J2000 right ascensions of Hipparcos stars are propagated without cos(dec)

**Body:**

The `ra` values of Hipparcos stars in `hyg_v44.csv` appear to have been moved from the Hipparcos epoch (J1991.25) to J2000 by adding the proper motion in right ascension directly to RA.
Hipparcos gives that proper motion as μα* = μα cos δ, so it needs dividing by cos δ before it is added to RA.
Without the division the error grows with the star's proper motion and with |dec|, and reaches about 18" for the fastest stars far from the equator.
Declinations are not affected.

To test it I took the 1,015 Hipparcos stars with total proper motion above 500 mas/yr (ESA 1997, VizieR `I/239/hip_main`, the catalogue HYG's proper motions match) and propagated their J1991.25 positions to J2000 both ways:

| HYG minus Hipparcos at J2000 | Median | 95th percentile | Worst |
| --- | --- | --- | --- |
| Propagated correctly (dividing by cos δ) | 0.411" | 4.727" | 17.882" |
| Propagated without dividing by cos δ | 0.010" | 0.024" | 4.127" |

HYG matches the second to rounding, so that seems to be how its positions were made.
The one star that fits neither model closely, Toliman (HIP 71681, 4.1"), is one where HYG uses α Centauri A's proper motion instead of the Hipparcos one, so it was propagated with a different motion.
The stars furthest from their correct J2000 positions:

| Star | Dec | HYG offset |
| --- | --- | --- |
| Proxima Centauri (HIP 70890) | −62.68° | 17.9" |
| Kapteyn's Star (HIP 24186) | −45.00° | 16.7" |
| Rigil Kentaurus (HIP 71683) | −60.84° | 16.5" |
| Toliman (HIP 71681) | −60.84° | 16.4" |
| HIP 108870 (ε Indi) | −56.78° | 15.7" |

As a cross-check, Proxima Centauri's J2000 position in SIMBAD (from Gaia DR3) is RA 217.42894°; HYG gives 14.495985 h = 217.43978°, which is 17.9" away along the sky, matching the table.
The `x`, `y`, `z` columns are computed from these positions, so they carry the same error; for Proxima it is about 23 AU.

The fix would be to divide by cos δ when propagating RA: `ra_2000 = ra_1991.25 + pmra * 8.75 / cos(dec)` (with units converted).
For most stars the difference is far below a pixel in any view of the sky; it matters for the nearest, fastest stars, and for anyone comparing HYG positions with another catalogue at the arcsecond level.

The script below reproduces the table.
It needs only Python 3 and downloads `hyg_v44.csv.gz` from this repository and the fast Hipparcos stars from VizieR.

<details>
<summary>check_ra.py</summary>

```python
"""Compare HYG v4.4 right ascensions with Hipparcos (ESA 1997) positions moved to J2000."""

import csv
import gzip
import io
import math
import statistics
import sys
import urllib.parse
import urllib.request

HYG = "https://codeberg.org/astronexus/hyg/media/branch/main/data/hyg/CURRENT/hyg_v44.csv.gz"
TAP = "https://tapvizier.cds.unistra.fr/TAPVizieR/tap/sync"
# Hipparcos stars with total proper motion above 500 mas/yr, where the effect shows.
QUERY = (
    'SELECT HIP, RAICRS, DEICRS, pmRA, pmDE FROM "I/239/hip_main" '
    "WHERE pmRA * pmRA + pmDE * pmDE > 250000"
)
YEARS = 2000.0 - 1991.25


def fetch(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=300) as response:
        return response.read()


hyg = {
    row["hip"]: row
    for row in csv.DictReader(io.StringIO(gzip.decompress(fetch(HYG)).decode()))
    if row["hip"]
}
parameters = {"REQUEST": "doQuery", "LANG": "ADQL", "FORMAT": "csv", "QUERY": QUERY}
# Optionally read a saved copy of the query result instead: python3 check_ra.py hip.csv
if len(sys.argv) > 1:
    text = open(sys.argv[1]).read()
else:
    text = fetch(TAP + "?" + urllib.parse.urlencode(parameters)).decode()
hipparcos = csv.DictReader(io.StringIO(text))

results = {"with cos(dec)": [], "without cos(dec)": []}
examples = []
for star in hipparcos:
    row = hyg.get(star["HIP"])
    if row is None:
        continue
    ra, dec = float(star["RAICRS"]), float(star["DEICRS"])
    pmra, pmdec = float(star["pmRA"]), float(star["pmDE"])  # pmRA includes cos(dec)
    dec2000 = dec + pmdec / 3.6e6 * YEARS
    hyg_ra, hyg_dec = float(row["ra"]) * 15, float(row["dec"])
    for model, divisor in (("with cos(dec)", math.cos(math.radians(dec))), ("without cos(dec)", 1.0)):
        ra2000 = ra + pmra / 3.6e6 * YEARS / divisor
        offset = math.hypot((hyg_ra - ra2000) * math.cos(math.radians(dec)), hyg_dec - dec2000) * 3600
        results[model].append(offset)
        if model == "with cos(dec)":
            examples.append((offset, star["HIP"], row["proper"], dec))

for model, offsets in results.items():
    offsets.sort()
    print(
        f"HYG minus Hipparcos propagated {model}: {len(offsets)} stars, "
        f"median {statistics.median(offsets):.3f}\", "
        f"95th percentile {offsets[int(0.95 * len(offsets))]:.3f}\", worst {offsets[-1]:.3f}\""
    )
print("Largest offsets from the correct propagation:")
for offset, hip, name, dec in sorted(examples)[-5:]:
    print(f"  HIP {hip} {name or '':18} dec {dec:+7.2f}  {offset:5.1f}\"")
```

</details>

Thank you for maintaining HYG; it is a great resource.
