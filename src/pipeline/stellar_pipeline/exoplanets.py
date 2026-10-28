"""Reads the NASA Exoplanet Archive's Planetary Systems Composite Parameters table.

Each row is one planet, with the parameters of its host system repeated on
every row. Every parameter has its own reference, given as an HTML link.

Positions come mostly from TIC v8, which takes them from Gaia DR2 at epoch
J2015.5 where it can. Its positions are taken to be at J2015.5; the gate tests
confirm that for the hosts also in Hipparcos (median difference 0.01").
A few fast-moving nearby stars are exceptions: TIC v8 took their positions from
2MASS, at about J2000. For rows whose position comes from elsewhere the epoch
is not stated.
"""

import csv
import html
import re
from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path

from stellar_pipeline.sources import EXOPLANET_PARAMETERS

TIC_V8_EPOCH = 2015.5
_TIC_V8_REFSTR = "STASSUN_ET_AL__2019"

_LINK = re.compile(r"href=(?P<url>\S+?)(?:\s|>).*?>(?P<label>.*?)</a>", re.DOTALL)
_TAGS = re.compile(r"<[^>]+>")


class ArchiveFormatError(Exception):
    pass


@dataclass(frozen=True, slots=True)
class Reference:
    label: str
    url: str | None


@dataclass(frozen=True, slots=True)
class Value:
    value: float
    err_lower: float | None
    err_upper: float | None
    limit: int
    reference: Reference | None


@dataclass(frozen=True, slots=True)
class ArchivePlanet:
    name: str
    letter: str | None
    hostname: str
    hip: str | None
    hd: str | None
    gaia_dr3: str | None
    ra_deg: float
    dec_deg: float
    position_epoch: float | None
    position_reference: Reference | None
    pmra: float | None
    pmdec: float | None
    distance: Value | None
    vmag: Value | None
    teff: Value | None
    spectral_type: str | None
    discovery_method: str | None
    discovery_year: int | None
    mass_provenance: str | None
    parameters: dict[str, Value]


def parse_reference(link: str) -> Reference | None:
    """'<a refstr=X href=https://... target=ref>Feng et al. 2017</a>' -> label and URL."""
    if not link.strip():
        return None
    match = _LINK.search(link)
    if match is None:
        return Reference(label=html.unescape(_TAGS.sub("", link)).strip(), url=None)
    return Reference(label=html.unescape(match["label"]).strip(), url=html.unescape(match["url"]))


def _float(value: str) -> float | None:
    value = value.strip()
    return float(value) if value else None


def _text(value: str) -> str | None:
    value = value.strip()
    return value or None


def _value(row: dict[str, str], column: str, *, has_limit: bool = True) -> Value | None:
    value = _float(row[column])
    if value is None:
        return None
    upper = _float(row[f"{column}err1"])
    lower = _float(row[f"{column}err2"])
    limit = int(row[f"{column}lim"] or 0) if has_limit else 0
    return Value(
        value=value,
        err_lower=None if lower is None else abs(lower),
        err_upper=upper,
        limit=limit,
        reference=parse_reference(row[f"{column}_reflink"]),
    )


def _gaia_dr3(designation: str) -> str | None:
    prefix = "Gaia DR3 "
    designation = designation.strip()
    return designation.removeprefix(prefix) if designation.startswith(prefix) else None


def parse_row(row: dict[str, str]) -> ArchivePlanet:
    year = _text(row["disc_year"])
    position_reference = parse_reference(row["ra_reflink"])
    from_tic = _TIC_V8_REFSTR in row["ra_reflink"]
    return ArchivePlanet(
        name=row["pl_name"].strip(),
        letter=_text(row["pl_letter"]),
        hostname=row["hostname"].strip(),
        hip=_text(row["hip_name"]),
        hd=_text(row["hd_name"]),
        gaia_dr3=_gaia_dr3(row["gaia_dr3_id"]),
        ra_deg=float(row["ra"]),
        dec_deg=float(row["dec"]),
        position_epoch=TIC_V8_EPOCH if from_tic else None,
        position_reference=position_reference,
        pmra=_float(row["sy_pmra"]),
        pmdec=_float(row["sy_pmdec"]),
        distance=_value(row, "sy_dist", has_limit=False),
        vmag=_value(row, "sy_vmag", has_limit=False),
        teff=_value(row, "st_teff"),
        spectral_type=_text(row["st_spectype"]),
        discovery_method=_text(row["discoverymethod"]),
        discovery_year=int(year) if year else None,
        mass_provenance=_text(row["pl_bmassprov"]),
        parameters={
            parameter: value
            for parameter in EXOPLANET_PARAMETERS
            if (value := _value(row, parameter)) is not None
        },
    )


def read(path: Path) -> Iterator[ArchivePlanet]:
    with path.open(encoding="utf-8", newline="") as stream:
        first = stream.readline()
        if not first.startswith("pl_name,"):
            # The archive answers a failed query with an XML error and HTTP 200.
            message = f"{path} is not the expected CSV: it starts {first[:80]!r}"
            raise ArchiveFormatError(message)
        stream.seek(0)
        for row in csv.DictReader(stream):
            yield parse_row(row)
