import csv
import gzip
from pathlib import Path

import pytest

from stellar_pipeline import exoplanets, hipparcos, hyg
from stellar_pipeline.exoplanets import ArchiveFormatError, parse_reference

from .conftest import FIXTURES


def _hyg_rows() -> dict[int, hyg.HygStar]:
    with (FIXTURES / "hyg.csv").open(newline="") as stream:
        return {star.id: star for star in map(hyg.parse_row, csv.DictReader(stream))}


def test_hyg_placeholders_become_unknown() -> None:
    stars = _hyg_rows()
    no_parallax = stars[13]
    assert no_parallax.distance_pc is None
    assert no_parallax.absmag is None
    assert no_parallax.luminosity is None
    assert no_parallax.rv is None


def test_hyg_keeps_real_values() -> None:
    sirius = _hyg_rows()[32263]
    assert sirius.ra_deg == 6.752481 * 15
    assert sirius.distance_pc == 2.6371
    assert sirius.rv == -9.4
    assert sirius.primary_id is None


def test_hyg_sun_keeps_its_zeros() -> None:
    sun = _hyg_rows()[0]
    assert sun.is_sun
    assert sun.rv == 0.0
    assert sun.absmag == 4.85


def test_hyg_companion_points_at_its_primary() -> None:
    assert _hyg_rows()[71453].primary_id == 71456


def test_hipparcos_fixed_width_fields() -> None:
    with (FIXTURES / "hip2.dat").open() as stream:
        entries = {entry.hip: entry for entry in map(hipparcos.parse_line, stream)}
    barnard = entries[87937]
    assert barnard.ra_rad == 4.7028598776
    assert barnard.parallax == 548.31
    assert barnard.pmdec == 10328.12
    assert barnard.e_parallax == 1.51
    assert barnard.b_v == 1.570


def test_reference_link_is_unescaped() -> None:
    reference = parse_reference(
        "<a refstr=X href=https://ui.adsabs.harvard.edu/abs/2025A&A...700A..11M/abstract "
        "target=ref>Su&aacute;rez Mascare&ntilde;o et al. 2025</a>"
    )
    assert reference is not None
    assert reference.label == "Suárez Mascareño et al. 2025"
    assert reference.url == "https://ui.adsabs.harvard.edu/abs/2025A&A...700A..11M/abstract"


def test_reference_without_link() -> None:
    assert parse_reference("") is None
    reference = parse_reference("Calculated value")
    assert reference is not None
    assert reference.label == "Calculated value"
    assert reference.url is None


def test_archive_rows() -> None:
    planets = {planet.name: planet for planet in exoplanets.read(FIXTURES / "pscomppars.csv")}
    proxima_b = planets["Proxima Cen b"]
    assert proxima_b.hostname == "Proxima Cen"
    assert proxima_b.position_epoch == exoplanets.TIC_V8_EPOCH
    orbit = proxima_b.parameters["pl_orbsmax"]
    assert orbit.value == 0.04848
    assert orbit.err_upper == orbit.err_lower == 0.00029
    assert orbit.reference is not None
    assert planets["eps Eri b"].gaia_dr3 == "5164707970261890560"


def test_archive_error_page_is_rejected(tmp_path: Path) -> None:
    page = tmp_path / "pscomppars.csv"
    page.write_text('<?xml version="1.0"?><VOTABLE><INFO name="QUERY_STATUS" value="ERROR"/>')
    with pytest.raises(ArchiveFormatError, match="not the expected CSV"):
        list(exoplanets.read(page))


def test_hyg_reader_decompresses(tmp_path: Path) -> None:
    compressed = tmp_path / "hyg.csv.gz"
    compressed.write_bytes(gzip.compress((FIXTURES / "hyg.csv").read_bytes()))
    assert [star.id for star in hyg.read(compressed)] == sorted(_hyg_rows())
