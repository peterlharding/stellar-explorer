"""Catalogue designations, written the way SIMBAD writes them.

HYG spells Bayer letters "Alp", "The-1"; SIMBAD and the exoplanet archive
write "alf", "tet1". Using SIMBAD's form means a name typed by the user, a
host name from the archive and a HYG designation all find the same star.
"""

import re

HYG = "HYG"
HIP = "HIP"
HD = "HD"
HR = "HR"
GLIESE = "GJ"
BAYER = "Bayer"
FLAMSTEED = "Flamsteed"
VARIABLE = "GCVS"
NAME = "name"
GAIA_DR3 = "Gaia DR3"
EXOPLANET_ARCHIVE = "NASA Exoplanet Archive"

_GREEK = {
    "Alp": "alf",
    "Bet": "bet",
    "Gam": "gam",
    "Del": "del",
    "Eps": "eps",
    "Zet": "zet",
    "Eta": "eta",
    "The": "tet",
    "Iot": "iot",
    "Kap": "kap",
    "Lam": "lam",
    "Mu": "mu",
    "Nu": "nu",
    "Xi": "ksi",
    "Omi": "omi",
    "Pi": "pi",
    "Rho": "rho",
    "Sig": "sig",
    "Tau": "tau",
    "Ups": "ups",
    "Phi": "phi",
    "Chi": "chi",
    "Psi": "psi",
    "Ome": "ome",
}

_BAYER = re.compile(r"^(?P<letter>[A-Za-z]+)(?:-(?P<index>\d+))?$")
_GLIESE = re.compile(r"^(?:Gl|GJ)\s+(?P<number>\S+)$")
_PREFIXED = re.compile(
    r"^(?P<catalogue>HIP|HD|GJ|Gl|Gliese)\s+(?P<number>\d+(?:\.\d+)?)\s*(?P<component>[A-Z]?)$"
)


def bayer(letter: str, constellation: str) -> str:
    """HYG's Bayer letter and constellation as SIMBAD writes them: "The-1", "Ori" -> "tet1 Ori"."""
    match = _BAYER.match(letter)
    if match is None:
        message = f"unrecognised Bayer designation {letter!r}"
        raise ValueError(message)
    name = match["letter"]
    # Latin Bayer letters ("p Eri", "A Car") are kept as they are.
    name = _GREEK.get(name, name)
    return f"{name}{match['index'] or ''} {constellation}"


def flamsteed(number: str, constellation: str) -> str:
    return f"{int(number)} {constellation}"


def gliese(designation: str) -> str:
    """'Gl 244A' or 'GJ 1293' -> '244A' or '1293'. Gl and GJ share one numbering."""
    match = _GLIESE.match(designation.strip())
    if match is None:
        message = f"unrecognised Gliese designation {designation!r}"
        raise ValueError(message)
    return match["number"]


def parse_prefixed(designation: str) -> tuple[str, str] | None:
    """A HIP, HD or Gliese designation as (catalogue, value); anything else gives None.

    'HIP 70890' -> ('HIP', '70890'); 'Gliese 12' -> ('GJ', '12'); 'GJ 667 C' -> ('GJ', '667C').
    HIP and HD number whole systems, so a component letter after them is dropped:
    'HD 133131 A' -> ('HD', '133131').
    """
    match = _PREFIXED.match(designation.strip())
    if match is None:
        return None
    if match["catalogue"] in ("Gl", "GJ", "Gliese"):
        return GLIESE, match["number"] + match["component"]
    if "." in match["number"]:
        return None
    return match["catalogue"], match["number"]
