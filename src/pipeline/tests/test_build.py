import sqlite3
from collections.abc import Iterator
from pathlib import Path

import pytest

from stellar_pipeline import build
from stellar_pipeline.build import Inputs
from stellar_pipeline.schema import SCHEMA_VERSION


@pytest.fixture
def catalogue(inputs: Inputs, tmp_path: Path) -> Iterator[sqlite3.Connection]:
    target = tmp_path / "catalogue.sqlite"
    build.build(inputs, target)
    connection = sqlite3.connect(target)
    yield connection
    connection.close()


def test_build_reports_what_it_did(inputs: Inputs, tmp_path: Path) -> None:
    report = build.build(inputs, tmp_path / "catalogue.sqlite")
    assert report.stars == 15
    assert report.planets == 7
    # Kepler-22 and HD 133131 A are not in the fixture HYG.
    assert report.stars_from_archive == 2


def test_schema_version_is_recorded(catalogue: sqlite3.Connection) -> None:
    assert catalogue.execute("PRAGMA user_version").fetchone() == (SCHEMA_VERSION,)
    info = dict(catalogue.execute("SELECT key, value FROM build_info"))
    assert info["schema_version"] == str(SCHEMA_VERSION)
    assert info["reference_epoch"] == "J2000.0"


def test_every_source_says_where_it_came_from(catalogue: sqlite3.Connection) -> None:
    rows = catalogue.execute(
        "SELECT key, licence, citation, sha256 FROM source WHERE key != 'stellar-explorer-pipeline'"
    ).fetchall()
    assert {row[0] for row in rows} == {"hyg", "hipparcos-2007", "nasa-exoplanet-archive"}
    for key, licence, citation, sha256 in rows:
        assert licence, key
        assert citation, key
        assert sha256 is not None
        assert len(sha256) == 64, key


def test_foreign_keys_hold(catalogue: sqlite3.Connection) -> None:
    assert catalogue.execute("PRAGMA foreign_key_check").fetchall() == []


def test_star_view(catalogue: sqlite3.Connection) -> None:
    row = catalogue.execute(
        "SELECT ra, dec, distance, pmdec, spectral_type FROM star WHERE name = ?",
        ("Barnard's Star",),
    ).fetchone()
    ra, dec, distance, pmdec, spectral_type = row
    assert distance == pytest.approx(1000 / 548.31)
    assert pmdec == 10328.12
    assert spectral_type == "sdM4"
    assert ra == pytest.approx(269.45208, abs=1e-4)
    assert dec == pytest.approx(4.69336, abs=1e-4)


def test_planets_hang_from_their_hosts(catalogue: sqlite3.Connection) -> None:
    rows = dict(catalogue.execute("SELECT name, host_name FROM planet"))
    assert rows["Proxima Cen b"] == "Proxima Centauri"
    assert rows["eps Eri b"] == "Ran"
    assert rows["Kepler-22 b"] == "Kepler-22"


def test_companions_hang_from_their_primary(catalogue: sqlite3.Connection) -> None:
    (parent,) = catalogue.execute(
        "SELECT p.name FROM object AS o JOIN object AS p ON p.id = o.parent_id WHERE o.name = ?",
        ("Toliman",),
    ).fetchone()
    assert parent == "Rigil Kentaurus"


def test_names_resolve_regardless_of_case(catalogue: sqlite3.Connection) -> None:
    def resolve(catalogue_name: str, value: str) -> set[int]:
        return {
            row[0]
            for row in catalogue.execute(
                "SELECT object_id FROM identifier WHERE catalogue = ? AND value = ?",
                (catalogue_name, value),
            )
        }

    ran = resolve("name", "Ran")
    assert len(ran) == 1
    assert resolve("name", "RAN") == ran
    assert resolve("Bayer", "eps Eri") == ran
    assert resolve("HIP", "16537") == ran
    assert resolve("NASA Exoplanet Archive", "eps Eri") == ran


def test_failed_build_leaves_nothing_behind(inputs: Inputs, tmp_path: Path) -> None:
    inputs.exoplanets.write_text("not,the,archive\n")
    target = tmp_path / "catalogue.sqlite"
    with pytest.raises(Exception, match="not the expected CSV"):
        build.build(inputs, target)
    assert not target.exists()
    assert not list(tmp_path.glob("*.building"))
