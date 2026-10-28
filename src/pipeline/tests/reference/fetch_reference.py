"""Fetches the published reference values the gate tests compare the catalogue with.

Run by hand when the list of stars changes, and commit the CSV it writes:

    uv run python tests/reference/fetch_reference.py

Positions and parallaxes come from SIMBAD, which gives positions at J2000.0 in
ICRS and its preferred parallax (mostly Gaia DR3, Hipparcos for the brightest
stars). They are independent of the catalogues the pipeline reads, apart from
the Hipparcos parallaxes SIMBAD still uses for a few bright stars.
"""

import csv
import io
import urllib.parse
import urllib.request
from datetime import UTC, datetime
from pathlib import Path

# (designation in the catalogue, designation for SIMBAD, common name)
STARS = (
    ("HIP 32349", "HIP 32349", "Sirius"),
    ("HIP 30438", "HIP 30438", "Canopus"),
    ("HIP 71683", "HIP 71683", "Rigil Kentaurus"),
    ("HIP 70890", "HIP 70890", "Proxima Centauri"),
    ("HIP 69673", "HIP 69673", "Arcturus"),
    ("HIP 91262", "HIP 91262", "Vega"),
    ("HIP 24608", "HIP 24608", "Capella"),
    ("HIP 24436", "HIP 24436", "Rigel"),
    ("HIP 37279", "HIP 37279", "Procyon"),
    ("HIP 27989", "HIP 27989", "Betelgeuse"),
    ("HIP 97649", "HIP 97649", "Altair"),
    ("HIP 21421", "HIP 21421", "Aldebaran"),
    ("HIP 65474", "HIP 65474", "Spica"),
    ("HIP 80763", "HIP 80763", "Antares"),
    ("HIP 37826", "HIP 37826", "Pollux"),
    ("HIP 113368", "HIP 113368", "Fomalhaut"),
    ("HIP 102098", "HIP 102098", "Deneb"),
    ("HIP 11767", "HIP 11767", "Polaris"),
    ("HIP 87937", "HIP 87937", "Barnard's Star"),
    ("HIP 16537", "HIP 16537", "Epsilon Eridani"),
    ("HIP 8102", "HIP 8102", "Tau Ceti"),
    ("HIP 104214", "HIP 104214", "61 Cygni A"),
    ("HIP 99240", "HIP 99240", "Delta Pavonis"),
    ("HIP 108870", "HIP 108870", "Epsilon Indi"),
    ("HIP 54035", "HIP 54035", "Lalande 21185"),
    ("HIP 57548", "HIP 57548", "Ross 128"),
    ("GJ 406", "GJ 406", "Wolf 359"),
)

_TAP = "https://simbad.cds.unistra.fr/simbad/sim-tap/sync"
_QUERY = (
    "SELECT ident.id, basic.main_id, basic.ra, basic.dec, basic.plx_value, basic.plx_err, "
    "basic.plx_bibcode FROM basic JOIN ident ON ident.oidref = basic.oid WHERE ident.id IN ({})"
)


def main() -> None:
    names = ", ".join("'" + simbad.replace("'", "''") + "'" for _, simbad, _ in STARS)
    parameters = {"REQUEST": "doQuery", "LANG": "ADQL", "FORMAT": "csv",
                  "QUERY": _QUERY.format(names)}  # fmt: skip
    url = _TAP + "?" + urllib.parse.urlencode(parameters)
    with urllib.request.urlopen(url, timeout=120) as response:
        rows = {row["id"]: row for row in csv.DictReader(io.StringIO(response.read().decode()))}

    target = Path(__file__).with_name("simbad.csv")
    with target.open("w", newline="") as stream:
        stream.write(f"# SIMBAD TAP, retrieved {datetime.now(UTC).date().isoformat()}.\n")
        stream.write("# ra and dec: ICRS, J2000.0, degrees. parallax: mas.\n")
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(["designation", "name", "simbad_id", "ra", "dec", "parallax",
                         "parallax_error", "parallax_bibcode"])  # fmt: skip
        for designation, simbad, name in STARS:
            row = rows[simbad]
            writer.writerow([designation, name, row["main_id"], row["ra"], row["dec"],
                             row["plx_value"], row["plx_err"], row["plx_bibcode"]])  # fmt: skip


if __name__ == "__main__":
    main()
