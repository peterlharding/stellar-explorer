"""Reads the Hipparcos New Reduction astrometric catalogue (van Leeuwen 2007, CDS I/311).

The file is fixed-width; the byte ranges below are from the catalogue's ReadMe.
Positions are at epoch J1991.25. The right ascension error is on a great
circle (mas), like the proper motion in right ascension.
"""

import gzip
from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path

EPOCH = 1991.25


@dataclass(frozen=True, slots=True)
class HipparcosStar:
    hip: int
    ra_rad: float
    dec_rad: float
    parallax: float
    pmra: float
    pmdec: float
    e_ra: float
    e_dec: float
    e_parallax: float
    e_pmra: float
    e_pmdec: float
    hpmag: float
    e_hpmag: float
    b_v: float | None
    e_b_v: float | None


def _field(line: str, first: int, last: int) -> float:
    """Bytes `first` to `last` inclusive, numbered from 1 as in the ReadMe."""
    return float(line[first - 1 : last])


def parse_line(line: str) -> HipparcosStar:
    b_v, e_b_v = _field(line, 153, 158), _field(line, 160, 164)
    # A colour index of 0.000 with an error of 0.000 means none was measured.
    missing_colour = b_v == 0.0 and e_b_v == 0.0
    return HipparcosStar(
        hip=int(line[0:6]),
        ra_rad=_field(line, 16, 28),
        dec_rad=_field(line, 30, 42),
        parallax=_field(line, 44, 50),
        pmra=_field(line, 52, 59),
        pmdec=_field(line, 61, 68),
        e_ra=_field(line, 70, 75),
        e_dec=_field(line, 77, 82),
        e_parallax=_field(line, 84, 89),
        e_pmra=_field(line, 91, 96),
        e_pmdec=_field(line, 98, 103),
        hpmag=_field(line, 130, 136),
        e_hpmag=_field(line, 138, 143),
        b_v=None if missing_colour else b_v,
        e_b_v=None if missing_colour or e_b_v == 0.0 else e_b_v,
    )


def read(path: Path) -> Iterator[HipparcosStar]:
    with gzip.open(path, "rt", encoding="ascii") as stream:
        for line in stream:
            if line.strip():
                yield parse_line(line)
