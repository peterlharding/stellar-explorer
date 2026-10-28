"""The phase 1 gate (docs/PLAN.md): the built catalogue against published values.

These run on the real catalogue, not the fixtures: build it first with
`make catalogue`, then `make gate`. Set STELLAR_CATALOGUE to check another file.
"""

import csv
import gzip
import math
import os
import sqlite3
import statistics
from collections.abc import Iterator
from pathlib import Path

import pytest

from stellar_pipeline import astrometry, exoplanets, hyg, sources
from stellar_pipeline.cli import DEFAULT_DATA
from stellar_pipeline.schema import SCHEMA_VERSION

pytestmark = pytest.mark.gate

REFERENCE = Path(__file__).parent / "reference" / "simbad.csv"

# Hipparcos positions propagated to J2000 agree with SIMBAD's (from Gaia DR3)
# to 0.2" for the reference stars; stars with only HYG's Gliese-catalogue
# positions agree to several arcseconds (Wolf 359: 6.6").
POSITION_TOLERANCE_ARCSEC = 0.5
POSITION_TOLERANCE_HYG_ONLY_ARCSEC = 10.0
# The reference stars are near and well measured: distances must agree within
# 1%, or the parallaxes within three combined standard errors. Alpha Centauri A
# needs the second: Hipparcos gives 754.81 +/- 4.11 mas, SIMBAD's adopted value
# (Pourbaix et al. 1997) is 742.12 +/- 1.4 mas, 1.7% and 2.9 sigma apart.
DISTANCE_TOLERANCE = 0.01
DISTANCE_SIGMAS = 3.0


@pytest.fixture(scope="module")
def catalogue() -> Iterator[sqlite3.Connection]:
    path = Path(os.environ.get("STELLAR_CATALOGUE", DEFAULT_DATA / "catalogue.sqlite"))
    if not path.exists():
        pytest.fail(f"{path} does not exist: run `make catalogue` first")
    connection = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
    yield connection
    connection.close()


def _reference_stars() -> list[dict[str, str]]:
    with REFERENCE.open(newline="") as stream:
        return list(csv.DictReader(line for line in stream if not line.startswith("#")))


def _hyg_object(catalogue: sqlite3.Connection, designation: str) -> int:
    catalogue_name, value = designation.split(" ")
    rows = catalogue.execute(
        "SELECT i.object_id FROM identifier AS i JOIN source AS s ON s.id = i.source_id "
        "WHERE i.catalogue = ? AND i.value = ? AND s.key = 'hyg'",
        (catalogue_name, value),
    ).fetchall()
    assert len(rows) == 1, designation
    return int(rows[0][0])


def test_schema_version(catalogue: sqlite3.Connection) -> None:
    assert catalogue.execute("PRAGMA user_version").fetchone() == (SCHEMA_VERSION,)


@pytest.mark.parametrize("star", _reference_stars(), ids=lambda star: star["name"])
def test_position_matches_simbad(catalogue: sqlite3.Connection, star: dict[str, str]) -> None:
    object_id = _hyg_object(catalogue, star["designation"])
    rows = dict(
        catalogue.execute(
            "SELECT m.quantity, s.key FROM measurement AS m JOIN source AS s ON s.id = m.source_id "
            "WHERE m.object_id = ? AND m.quantity IN ('ra', 'dec') AND m.preferred",
            (object_id,),
        )
    )
    ra, dec, epoch = catalogue.execute(
        "SELECT s.ra, s.dec, m.epoch FROM star AS s JOIN measurement AS m "
        "ON m.object_id = s.id AND m.quantity = 'ra' AND m.preferred WHERE s.id = ?",
        (object_id,),
    ).fetchone()
    assert epoch == astrometry.REFERENCE_EPOCH
    separation = astrometry.separation_arcsec(ra, dec, float(star["ra"]), float(star["dec"]))
    hyg_only = rows["ra"] == sources.HYG.key
    tolerance = POSITION_TOLERANCE_HYG_ONLY_ARCSEC if hyg_only else POSITION_TOLERANCE_ARCSEC
    assert separation < tolerance


@pytest.mark.parametrize("star", _reference_stars(), ids=lambda star: star["name"])
def test_distance_matches_simbad(catalogue: sqlite3.Connection, star: dict[str, str]) -> None:
    object_id = _hyg_object(catalogue, star["designation"])
    (distance,) = catalogue.execute(
        "SELECT distance FROM star WHERE id = ?", (object_id,)
    ).fetchone()
    parallax, parallax_error = float(star["parallax"]), float(star["parallax_error"])
    published = 1000.0 / parallax
    if distance == pytest.approx(published, rel=DISTANCE_TOLERANCE):
        return
    # Otherwise the two parallaxes must agree within their combined errors.
    (our_error,) = catalogue.execute(
        "SELECT err_upper FROM measurement WHERE object_id = ? AND quantity = 'parallax' "
        "AND preferred",
        (object_id,),
    ).fetchone() or (None,)
    assert our_error is not None, "differs by more than 1% and has no parallax error"
    combined = math.hypot(our_error, parallax_error)
    assert abs(1000.0 / distance - parallax) < DISTANCE_SIGMAS * combined


def test_directions_agree_with_hyg(catalogue: sqlite3.Connection) -> None:
    """Our x, y, z against HYG's, for Hipparcos stars: the same sky up to HYG's RA error.

    HYG propagated Hipparcos positions to J2000 without cos(dec) in right
    ascension, which puts fast stars far from the equator up to 18" out; the
    pipeline propagates them itself. Everywhere else the two should agree.
    """
    ours = {
        int(hyg_id): (x, y, z)
        for hyg_id, x, y, z in catalogue.execute(
            "SELECT i.value, s.x, s.y, s.z FROM star AS s JOIN identifier AS i "
            "ON i.object_id = s.id AND i.catalogue = 'HYG' WHERE s.x IS NOT NULL"
        )
    }
    angles = []
    with gzip.open(DEFAULT_DATA / "raw" / sources.HYG.filename, "rt", newline="") as stream:
        for row in csv.DictReader(stream):
            star = hyg.parse_row(row)
            if star.id in ours and star.hip is not None and star.distance_pc is not None:
                theirs = (float(row["x"]), float(row["y"]), float(row["z"]))
                ra, dec, _ = astrometry.to_spherical(ours[star.id])
                hyg_ra, hyg_dec, _ = astrometry.to_spherical(theirs)
                angles.append(astrometry.separation_arcsec(ra, dec, hyg_ra, hyg_dec))
    angles.sort()
    assert len(angles) > 100_000
    assert angles[len(angles) // 2] < 0.05
    assert angles[int(0.999 * len(angles))] < 20.0


def test_every_row_has_a_source(catalogue: sqlite3.Connection) -> None:
    assert catalogue.execute("PRAGMA foreign_key_check").fetchall() == []
    for table in ("object", "identifier", "measurement", "property"):
        (missing,) = catalogue.execute(
            f"SELECT count(*) FROM {table} AS t LEFT JOIN source AS s ON s.id = t.source_id "
            "WHERE s.id IS NULL"
        ).fetchone()
        assert missing == 0, table
    for key, version, licence, citation, sha256 in catalogue.execute(
        "SELECT key, version, licence, citation, sha256 FROM source"
    ):
        assert version, key
        assert licence, key
        assert citation, key
        if key != sources.PIPELINE_KEY:
            assert sha256 is not None
            assert len(sha256) == 64, key


def test_every_derived_value_names_its_derivation(catalogue: sqlite3.Connection) -> None:
    (unexplained,) = catalogue.execute(
        "SELECT count(*) FROM measurement AS m JOIN source AS s ON s.id = m.source_id "
        "WHERE s.key = ? AND m.derivation_id IS NULL",
        (sources.PIPELINE_KEY,),
    ).fetchone()
    assert unexplained == 0


def test_one_preferred_value_each(catalogue: sqlite3.Connection) -> None:
    (bad,) = catalogue.execute(
        "SELECT count(*) FROM (SELECT 1 FROM measurement WHERE NOT invalid "
        "GROUP BY object_id, quantity HAVING sum(preferred) != 1)"
    ).fetchone()
    assert bad == 0
    (invalid_preferred,) = catalogue.execute(
        "SELECT count(*) FROM measurement WHERE preferred AND invalid"
    ).fetchone()
    assert invalid_preferred == 0


def test_all_hyg_stars_and_all_planets_are_present(catalogue: sqlite3.Connection) -> None:
    (hyg_stars,) = catalogue.execute(
        "SELECT count(*) FROM object AS o JOIN source AS s ON s.id = o.source_id "
        "WHERE o.kind = 'star' AND s.key = 'hyg'"
    ).fetchone()
    assert hyg_stars == 119_614
    archive = DEFAULT_DATA / "raw" / sources.EXOPLANETS.filename
    expected = sum(1 for _ in exoplanets.read(archive))
    (planets,) = catalogue.execute("SELECT count(*) FROM planet").fetchone()
    assert planets == expected
    (orphans,) = catalogue.execute(
        "SELECT count(*) FROM object WHERE kind = 'planet' AND parent_id IS NULL"
    ).fetchone()
    assert orphans == 0


def test_archive_positions_agree_once_moved_to_j2000(catalogue: sqlite3.Connection) -> None:
    """Confirms the J2015.5 epoch taken for TIC v8 positions, on hosts also in Hipparcos."""
    separations = []
    for ra, dec, archive_ra, archive_dec, pmra, pmdec in catalogue.execute(
        "SELECT s.ra, s.dec, a.value, d.value, pa.value, pd.value FROM star AS s "
        "JOIN source AS src ON src.key = 'nasa-exoplanet-archive' "
        "JOIN measurement AS a ON a.object_id = s.id AND a.quantity = 'ra' "
        "    AND a.source_id = src.id AND a.epoch = 2015.5 "
        "JOIN measurement AS d ON d.object_id = s.id AND d.quantity = 'dec' "
        "    AND d.source_id = src.id "
        "JOIN measurement AS pa ON pa.object_id = s.id AND pa.quantity = 'pmra' "
        "    AND pa.source_id = src.id "
        "JOIN measurement AS pd ON pd.object_id = s.id AND pd.quantity = 'pmdec' "
        "    AND pd.source_id = src.id "
        "JOIN measurement AS h ON h.object_id = s.id AND h.quantity = 'parallax'"
    ):
        moved = astrometry.propagate(archive_ra, archive_dec, pmra, pmdec, 2015.5, 2000.0)
        separations.append(astrometry.separation_arcsec(ra, dec, *moved))
    assert len(separations) > 500
    assert statistics.median(separations) < 0.05
    assert sorted(separations)[int(0.9 * len(separations))] < 1.0


@pytest.mark.parametrize(
    ("catalogue_name", "value"),
    [("name", "Ran"), ("Bayer", "eps Eri"), ("HIP", "16537"), ("HD", "22049"), ("GJ", "144")],
)
def test_names_resolve_to_one_star(
    catalogue: sqlite3.Connection, catalogue_name: str, value: str
) -> None:
    objects = {
        row[0]
        for row in catalogue.execute(
            "SELECT object_id FROM identifier WHERE catalogue = ? AND value = ?",
            (catalogue_name, value),
        )
    }
    (ran,) = catalogue.execute("SELECT id FROM object WHERE name = 'Ran'").fetchone()
    assert objects == {ran}


def test_the_sun_is_at_the_origin(catalogue: sqlite3.Connection) -> None:
    row = catalogue.execute("SELECT x, y, z, vx, vy, vz FROM star WHERE name = 'Sun'").fetchone()
    assert row == (0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
