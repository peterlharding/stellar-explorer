import csv
from dataclasses import replace

import pytest

from stellar_pipeline import exoplanets, hipparcos, hyg
from stellar_pipeline.crossmatch import HygIndex, match_hosts

from .conftest import FIXTURES


@pytest.fixture(scope="module")
def index() -> HygIndex:
    with (FIXTURES / "hip2.dat").open() as stream:
        by_hip = {entry.hip: entry for entry in map(hipparcos.parse_line, stream)}
    built = HygIndex()
    with (FIXTURES / "hyg.csv").open(newline="") as stream:
        for star in map(hyg.parse_row, csv.DictReader(stream)):
            built.add(star, None if star.hip is None else by_hip.get(star.hip))
    return built


@pytest.fixture(scope="module")
def hosts() -> dict[str, exoplanets.ArchivePlanet]:
    return {p.hostname: p for p in exoplanets.read(FIXTURES / "pscomppars.csv")}


def test_hosts_match_by_hip_hd_and_gliese(
    index: HygIndex,
    hosts: dict[str, exoplanets.ArchivePlanet],
) -> None:
    matches = match_hosts(hosts.values(), index)
    assert matches.to_hyg["Proxima Cen"] == 70666
    assert matches.to_hyg["eps Eri"] == 16496
    assert matches.to_hyg["tau Cet"] == 8087
    assert matches.to_hyg["GJ 667 C"] == 119144
    assert "Kepler-22" not in matches.to_hyg
    assert matches.methods["GJ"] == 1


def test_shared_hd_number_goes_to_the_primary(
    index: HygIndex,
    hosts: dict[str, exoplanets.ArchivePlanet],
) -> None:
    # HD 38 is both Gl 4A (HYG 472, the primary) and Gl 4B.
    host = replace(hosts["Kepler-22"], hostname="HD 38", hip=None, hd="HD 38")
    assert match_hosts([host], index).to_hyg["HD 38"] == 472


def test_host_matches_by_position_when_no_identifier_does(
    index: HygIndex,
    hosts: dict[str, exoplanets.ArchivePlanet],
) -> None:
    proxima = hosts["Proxima Cen"]
    anonymous = replace(proxima, hostname="Anonymous", hip=None, hd=None)
    matches = match_hosts([anonymous], index)
    assert matches.to_hyg["Anonymous"] == 70666
    assert matches.methods["position"] == 1


def test_position_match_needs_the_magnitude_to_agree(
    index: HygIndex,
    hosts: dict[str, exoplanets.ArchivePlanet],
) -> None:
    proxima = hosts["Proxima Cen"]
    assert proxima.vmag is not None
    brighter = replace(proxima.vmag, value=proxima.vmag.value - 3)
    impostor = replace(proxima, hostname="Impostor", hip=None, hd=None, vmag=brighter)
    assert "Impostor" not in match_hosts([impostor], index).to_hyg
