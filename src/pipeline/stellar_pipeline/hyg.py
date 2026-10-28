"""Reads the HYG database (CSV, version 4.x).

HYG's placeholders are turned into "unknown" here, so nothing downstream sees them:

- A distance of 100000 pc means the parallax was too small or negative; HYG
  treats parallaxes of 1 mas or less this way.
- A radial velocity of exactly 0 means none is known, for all stars but the Sun.
- Absolute magnitude and luminosity are computed by HYG from the distance, so
  they are meaningless where the distance is a placeholder.
- A proper motion of +/-9999.99 mas/yr is an overflowed field: the real value
  did not fit. It is kept and flagged so the record shows what HYG says.
"""

import csv
import gzip
from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path

PLACEHOLDER_DISTANCE_PC = 100_000.0
OVERFLOWED_PROPER_MOTION = 9999.99
EPOCH = 2000.0


@dataclass(frozen=True, slots=True)
class HygStar:
    id: int
    hip: int | None
    hd: int | None
    hr: int | None
    gliese: str | None
    proper: str | None
    bayer: str | None
    flamsteed: str | None
    constellation: str | None
    variable: str | None
    ra_deg: float
    dec_deg: float
    distance_pc: float | None
    pmra: float
    pmdec: float
    rv: float | None
    mag: float
    absmag: float | None
    spectral_type: str | None
    colour_index: float | None
    luminosity: float | None
    # HYG id of the primary when this star is a companion in a multiple system.
    primary_id: int | None

    @property
    def is_sun(self) -> bool:
        return self.id == 0


def _text(value: str) -> str | None:
    value = value.strip()
    return value or None


def _int(value: str) -> int | None:
    value = value.strip()
    return int(value) if value else None


def _float(value: str) -> float | None:
    value = value.strip()
    return float(value) if value else None


def parse_row(row: dict[str, str]) -> HygStar:
    star_id = int(row["id"])
    distance = float(row["dist"])
    known_distance = 0 < distance < PLACEHOLDER_DISTANCE_PC
    rv = float(row["rv"])
    primary = int(row["comp_primary"])
    return HygStar(
        id=star_id,
        hip=_int(row["hip"]),
        hd=_int(row["hd"]),
        hr=_int(row["hr"]),
        gliese=_text(row["gl"]),
        proper=_text(row["proper"]),
        bayer=_text(row["bayer"]),
        flamsteed=_text(row["flam"]),
        constellation=_text(row["con"]),
        variable=_text(row["var"]),
        ra_deg=float(row["ra"]) * 15.0,
        dec_deg=float(row["dec"]),
        distance_pc=distance if known_distance else None,
        pmra=float(row["pmra"]),
        pmdec=float(row["pmdec"]),
        rv=rv if rv != 0.0 or star_id == 0 else None,
        mag=float(row["mag"]),
        absmag=float(row["absmag"]) if known_distance or star_id == 0 else None,
        spectral_type=_text(row["spect"]),
        colour_index=_float(row["ci"]),
        luminosity=float(row["lum"]) if known_distance or star_id == 0 else None,
        primary_id=primary if primary != star_id else None,
    )


def read(path: Path) -> Iterator[HygStar]:
    with gzip.open(path, "rt", encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream):
            yield parse_row(row)
