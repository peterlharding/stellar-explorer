# HYG 4.4: Barnard's Star's proper motion in declination has overflowed

Draft issue for [astronexus/hyg](https://codeberg.org/astronexus/hyg/issues).
Not posted.
See [README.md](README.md) before posting.

---

**Title:** HYG 4.4: Barnard's Star pmdec is 9999.99 instead of 10326.93 mas/yr

**Body:**

In `hyg_v44.csv`, Barnard's Star (id 87665, HIP 87937) has `pmdec` = 9999.99 mas/yr.
The Hipparcos catalogue (ESA 1997, VizieR `I/239/hip_main`) gives 10326.93 mas/yr.
HYG's proper motions otherwise match that catalogue exactly: of the 1,015 Hipparcos stars moving faster than 500 mas/yr, 1,013 agree to the last digit, and the other two are this star and Toliman (HIP 71681), where HYG deliberately uses α Centauri A's motion.
It looks like the value overflowed a field limited to 9999.99 somewhere along the way.
It is the only row in the file with a proper motion of ±9999.99.

| | HYG 4.4 | Hipparcos 1997 |
| --- | --- | --- |
| `pmra` (mas/yr) | −797.84 | −797.84 |
| `pmdec` (mas/yr) | 9999.99 | 10326.93 |

The velocity columns are computed from the truncated value.
Recomputing them from HYG's own `ra`, `dec`, `dist` and `rv`:

| pc/yr | `vx` | `vy` | `vz` |
| --- | --- | --- | --- |
| In HYG 4.4 | −0.00000591 | 0.0001204 | 0.00007888 |
| Recomputed with pmdec 9999.99 | −0.00000590 | 0.00012044 | 0.00007884 |
| Recomputed with pmdec 10326.93 | −0.00000590 | 0.00012067 | 0.00008172 |

So `vz` is low by about 2.8 × 10⁻⁶ pc/yr (2.8 km/s), and anything that runs Barnard's Star's motion forward from HYG under-moves it by about 3%.

The fix would be `pmdec` = 10326.93 and recomputed `vx`, `vy`, `vz` for this row.
It may be worth checking whatever step produced the overflow, in case other numeric fields share the same limit; I found no other value at ±9999.99.

The script below reproduces both tables.
It needs only Python 3 and downloads `hyg_v44.csv.gz` from this repository and the Hipparcos row from VizieR.

<details>
<summary>check_barnard.py</summary>

```python
"""Barnard's Star (HIP 87937) in HYG v4.4 against the Hipparcos catalogue (ESA 1997)."""

import csv
import gzip
import io
import math
import sys
import urllib.parse
import urllib.request

HYG = "https://codeberg.org/astronexus/hyg/media/branch/main/data/hyg/CURRENT/hyg_v44.csv.gz"
TAP = "https://tapvizier.cds.unistra.fr/TAPVizieR/tap/sync"
QUERY = 'SELECT HIP, pmRA, pmDE FROM "I/239/hip_main" WHERE HIP = 87937'
KMS_TO_PC_PER_YEAR = 1 / 977_792.2  # 1 km/s in pc/yr
AU_PER_YEAR_KMS = 4.740470


def fetch(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=300) as response:
        return response.read()


rows = list(csv.DictReader(io.StringIO(gzip.decompress(fetch(HYG)).decode())))
overflowed = [r for r in rows for c in ("pmra", "pmdec") if abs(float(r[c])) == 9999.99]
print("Rows with a proper motion of +/-9999.99:", [(r["id"], r["hip"], r["proper"]) for r in overflowed])

barnard = next(r for r in rows if r["hip"] == "87937")
parameters = {"REQUEST": "doQuery", "LANG": "ADQL", "FORMAT": "csv", "QUERY": QUERY}
# Optionally read a saved copy of the query result instead: python3 check_barnard.py hip.csv
if len(sys.argv) > 1:
    text = open(sys.argv[1]).read()
else:
    text = fetch(TAP + "?" + urllib.parse.urlencode(parameters)).decode()
(hip,) = (row for row in csv.DictReader(io.StringIO(text)) if row["HIP"] == "87937")
print(f"pmra:  HYG {barnard['pmra']:>9}  Hipparcos 1997 {hip['pmRA']:>9}")
print(f"pmdec: HYG {barnard['pmdec']:>9}  Hipparcos 1997 {hip['pmDE']:>9}")


def velocity(pmra: float, pmdec: float) -> tuple[float, float, float]:
    ra, dec = math.radians(float(barnard["ra"]) * 15), math.radians(float(barnard["dec"]))
    distance, rv = float(barnard["dist"]), float(barnard["rv"])
    k = AU_PER_YEAR_KMS * distance / 1000  # km/s per mas/yr
    east = (-math.sin(ra), math.cos(ra), 0.0)
    north = (-math.sin(dec) * math.cos(ra), -math.sin(dec) * math.sin(ra), math.cos(dec))
    out = (math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec))
    return tuple(
        (pmra * k * e + pmdec * k * n + rv * o) * KMS_TO_PC_PER_YEAR
        for e, n, o in zip(east, north, out)
    )


print("vx, vy, vz (pc/yr):")
print("  in HYG                       ", barnard["vx"], barnard["vy"], barnard["vz"])
print("  recomputed from HYG's values  ", " ".join(f"{v:.8f}" for v in velocity(float(barnard["pmra"]), float(barnard["pmdec"]))))
print("  recomputed with 10326.93      ", " ".join(f"{v:.8f}" for v in velocity(float(barnard["pmra"]), float(hip["pmDE"]))))
```

</details>

Thank you for maintaining HYG.
