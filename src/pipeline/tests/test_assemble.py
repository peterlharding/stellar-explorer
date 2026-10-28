import csv
import math

import pytest

from stellar_pipeline import assemble, astrometry, exoplanets, hipparcos, hyg
from stellar_pipeline.records import Measurement, ObjectRecord
from stellar_pipeline.sources import EXOPLANETS, HIPPARCOS, HYG, PIPELINE_KEY

from .conftest import FIXTURES


@pytest.fixture(scope="module")
def hyg_stars() -> dict[int, hyg.HygStar]:
    with (FIXTURES / "hyg.csv").open(newline="") as stream:
        return {star.id: star for star in map(hyg.parse_row, csv.DictReader(stream))}


@pytest.fixture(scope="module")
def hip_entries() -> dict[int, hipparcos.HipparcosStar]:
    with (FIXTURES / "hip2.dat").open() as stream:
        return {entry.hip: entry for entry in map(hipparcos.parse_line, stream)}


@pytest.fixture(scope="module")
def planets() -> dict[str, exoplanets.ArchivePlanet]:
    return {planet.name: planet for planet in exoplanets.read(FIXTURES / "pscomppars.csv")}


def _star(
    hyg_stars: dict[int, hyg.HygStar],
    hip_entries: dict[int, hipparcos.HipparcosStar],
    hyg_id: int,
) -> ObjectRecord:
    star = hyg_stars[hyg_id]
    return assemble.star(star, None if star.hip is None else hip_entries.get(star.hip))


def _only(record: ObjectRecord, quantity: str, source: str) -> Measurement:
    (found,) = [m for m in record.measurements if m.quantity == quantity and m.source == source]
    return found


def _assert_one_preferred_each(record: ObjectRecord) -> None:
    quantities = {m.quantity for m in record.measurements if not m.invalid}
    for quantity in quantities:
        preferred = [m for m in record.measurements if m.quantity == quantity and m.preferred]
        assert len(preferred) == 1, quantity
    assert not any(m.preferred and m.invalid for m in record.measurements)


def test_barnards_star(
    hyg_stars: dict[int, hyg.HygStar],
    hip_entries: dict[int, hipparcos.HipparcosStar],
) -> None:
    record = _star(hyg_stars, hip_entries, 87665)
    _assert_one_preferred_each(record)
    assert record.name == "Barnard's Star"

    overflowed = _only(record, "pmdec", HYG.key)
    assert overflowed.value == 9999.99
    assert overflowed.invalid
    assert record.preferred("pmdec") == _only(record, "pmdec", HIPPARCOS.key)
    assert record.preferred_value("pmdec") == 10328.12

    ra = record.preferred("ra")
    assert ra is not None
    assert ra.source == PIPELINE_KEY
    assert ra.derivation == "epoch-propagation"
    assert ra.epoch == 2000.0
    dec = record.preferred_value("dec")
    assert dec is not None
    # SIMBAD, J2000 from Gaia DR3.
    assert astrometry.separation_arcsec(ra.value, dec, 269.4520769586, 4.6933649666) < 0.2

    distance = record.preferred("distance")
    assert distance is not None
    assert distance.derivation == "distance-from-parallax"
    assert distance.value == pytest.approx(1000 / 548.31)
    assert distance.err_lower is not None
    assert distance.err_upper is not None
    assert distance.err_lower < distance.err_upper

    x, y, z = (record.preferred_value(axis) for axis in ("x", "y", "z"))
    assert x is not None
    assert y is not None
    assert z is not None
    assert math.sqrt(x * x + y * y + z * z) == pytest.approx(distance.value)
    vx = record.preferred("vx")
    assert vx is not None
    assert vx.derivation == "space-velocity"


def test_hyg_override_of_a_bad_hipparcos_parallax_wins(
    hyg_stars: dict[int, hyg.HygStar],
    hip_entries: dict[int, hipparcos.HipparcosStar],
) -> None:
    # alpha Centauri B: Hipparcos gives 796.92 mas; HYG uses alpha Centauri A's distance.
    record = _star(hyg_stars, hip_entries, 71453)
    _assert_one_preferred_each(record)
    distance = record.preferred("distance")
    assert distance is not None
    assert distance.source == HYG.key
    assert distance.value == 1.3248
    assert distance.note is not None
    assert "796.92" in distance.note
    assert record.preferred_value("parallax") == 796.92


def test_star_without_usable_parallax_has_no_distance(
    hyg_stars: dict[int, hyg.HygStar],
    hip_entries: dict[int, hipparcos.HipparcosStar],
) -> None:
    record = _star(hyg_stars, hip_entries, 13)
    _assert_one_preferred_each(record)
    parallax = record.preferred_value("parallax")
    assert parallax is not None
    assert parallax <= assemble.MIN_PARALLAX_MAS
    for quantity in ("distance", "x", "vx", "absmag", "luminosity"):
        assert record.preferred(quantity) is None, quantity
    # The direction is still known and moved to J2000.
    ra = record.preferred("ra")
    assert ra is not None
    assert ra.epoch == 2000.0


def test_star_without_radial_velocity_moves_across_the_sky_only(
    hyg_stars: dict[int, hyg.HygStar],
    hip_entries: dict[int, hipparcos.HipparcosStar],
) -> None:
    record = _star(hyg_stars, hip_entries, 162)
    assert record.preferred("rv") is None
    vx = record.preferred("vx")
    assert vx is not None
    assert vx.derivation == "tangential-velocity"


def test_star_hipparcos_did_not_observe_keeps_hyg_astrometry(
    hyg_stars: dict[int, hyg.HygStar],
    hip_entries: dict[int, hipparcos.HipparcosStar],
) -> None:
    record = _star(hyg_stars, hip_entries, 118720)
    _assert_one_preferred_each(record)
    assert record.name == "Wolf 359"
    ra = record.preferred("ra")
    assert ra is not None
    assert ra.source == HYG.key
    distance = record.preferred("distance")
    assert distance is not None
    assert distance.source == HYG.key
    assert record.preferred("x") is not None


def test_sun_is_the_origin(hyg_stars: dict[int, hyg.HygStar]) -> None:
    record = assemble.star(hyg_stars[0])
    _assert_one_preferred_each(record)
    assert record.name == "Sun"
    for axis in ("x", "y", "z", "vx", "vy", "vz"):
        assert record.preferred_value(axis) == 0.0
    assert record.preferred("ra") is None
    names = {identifier.value for identifier in record.identifiers}
    assert {"Sun", "Sol"} <= names


def test_identifiers_and_name(
    hyg_stars: dict[int, hyg.HygStar],
    hip_entries: dict[int, hipparcos.HipparcosStar],
) -> None:
    record = _star(hyg_stars, hip_entries, 16496)
    found = {(identifier.catalogue, identifier.value) for identifier in record.identifiers}
    assert {("HIP", "16537"), ("HD", "22049"), ("GJ", "144"), ("name", "Ran")} <= found
    assert ("Bayer", "eps Eri") in found
    assert record.name == "Ran"


def test_display_name_falls_back_through_catalogues(
    hyg_stars: dict[int, hyg.HygStar],
    hip_entries: dict[int, hipparcos.HipparcosStar],
) -> None:
    assert _star(hyg_stars, hip_entries, 13).name == "HD 224728"
    assert _star(hyg_stars, hip_entries, 119144).name == "GJ 667C"


def test_matched_host_adds_the_archive_view_without_taking_over(
    hyg_stars: dict[int, hyg.HygStar],
    hip_entries: dict[int, hipparcos.HipparcosStar],
    planets: dict[str, exoplanets.ArchivePlanet],
) -> None:
    star = hyg_stars[16496]
    record = assemble.star(star, hip_entries[16537], planets["eps Eri b"])
    _assert_one_preferred_each(record)
    archive_distance = _only(record, "distance", EXOPLANETS.key)
    assert not archive_distance.preferred
    assert archive_distance.reference_label is not None
    # A measured temperature beats the rough estimate from colour, which is not made.
    teff = record.preferred("teff")
    assert teff is not None
    assert teff.source == EXOPLANETS.key
    assert not [m for m in record.measurements if m.derivation == "temperature-from-colour"]
    identifiers = {(i.catalogue, i.value) for i in record.identifiers}
    assert ("NASA Exoplanet Archive", "eps Eri") in identifiers
    assert ("Gaia DR3", "5164707970261890560") in identifiers


def test_host_star_from_the_archive_alone(planets: dict[str, exoplanets.ArchivePlanet]) -> None:
    record = assemble.host_star(planets["Kepler-22 b"])
    _assert_one_preferred_each(record)
    assert record.name == "Kepler-22"
    assert record.source == EXOPLANETS.key
    ra = record.preferred("ra")
    assert ra is not None
    assert ra.epoch == 2000.0
    assert record.preferred("x") is not None


def test_planet(planets: dict[str, exoplanets.ArchivePlanet]) -> None:
    record = assemble.planet(planets["Proxima Cen b"])
    _assert_one_preferred_each(record)
    assert record.kind == "planet"
    period = record.preferred("orbital_period")
    assert period is not None
    assert period.value == pytest.approx(11.18465)
    assert period.reference_url is not None
    methods = {p.name: p.value for p in record.properties}
    assert methods["discovery_method"] == "Radial Velocity"


def test_every_derivation_used_is_described(
    hyg_stars: dict[int, hyg.HygStar],
    hip_entries: dict[int, hipparcos.HipparcosStar],
    planets: dict[str, exoplanets.ArchivePlanet],
) -> None:
    records = [_star(hyg_stars, hip_entries, hyg_id) for hyg_id in hyg_stars]
    records += [assemble.host_star(planet) for planet in planets.values()]
    used = {m.derivation for r in records for m in r.measurements if m.derivation}
    assert used <= set(assemble.DERIVATIONS)
