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
# A designation as people say it ("Delta Pavonis" for "del Pav"), for voice and search.
SPOKEN = "spoken"

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

# SIMBAD's Greek letters, written out.
_GREEK_SPOKEN = {
    "alf": "Alpha",
    "bet": "Beta",
    "gam": "Gamma",
    "del": "Delta",
    "eps": "Epsilon",
    "zet": "Zeta",
    "eta": "Eta",
    "tet": "Theta",
    "iot": "Iota",
    "kap": "Kappa",
    "lam": "Lambda",
    "mu": "Mu",
    "nu": "Nu",
    "ksi": "Xi",
    "omi": "Omicron",
    "pi": "Pi",
    "rho": "Rho",
    "sig": "Sigma",
    "tau": "Tau",
    "ups": "Upsilon",
    "phi": "Phi",
    "chi": "Chi",
    "psi": "Psi",
    "ome": "Omega",
}

# The genitive of each IAU constellation, by its abbreviation, as a designation says it
# ("Pav" -> "Pavonis"); Bootis without its diaeresis, as a recogniser writes it.
GENITIVES = {
    "And": "Andromedae",
    "Ant": "Antliae",
    "Aps": "Apodis",
    "Aqr": "Aquarii",
    "Aql": "Aquilae",
    "Ara": "Arae",
    "Ari": "Arietis",
    "Aur": "Aurigae",
    "Boo": "Bootis",
    "Cae": "Caeli",
    "Cam": "Camelopardalis",
    "Cnc": "Cancri",
    "CVn": "Canum Venaticorum",
    "CMa": "Canis Majoris",
    "CMi": "Canis Minoris",
    "Cap": "Capricorni",
    "Car": "Carinae",
    "Cas": "Cassiopeiae",
    "Cen": "Centauri",
    "Cep": "Cephei",
    "Cet": "Ceti",
    "Cha": "Chamaeleontis",
    "Cir": "Circini",
    "Col": "Columbae",
    "Com": "Comae Berenices",
    "CrA": "Coronae Australis",
    "CrB": "Coronae Borealis",
    "Crv": "Corvi",
    "Crt": "Crateris",
    "Cru": "Crucis",
    "Cyg": "Cygni",
    "Del": "Delphini",
    "Dor": "Doradus",
    "Dra": "Draconis",
    "Equ": "Equulei",
    "Eri": "Eridani",
    "For": "Fornacis",
    "Gem": "Geminorum",
    "Gru": "Gruis",
    "Her": "Herculis",
    "Hor": "Horologii",
    "Hya": "Hydrae",
    "Hyi": "Hydri",
    "Ind": "Indi",
    "Lac": "Lacertae",
    "Leo": "Leonis",
    "LMi": "Leonis Minoris",
    "Lep": "Leporis",
    "Lib": "Librae",
    "Lup": "Lupi",
    "Lyn": "Lyncis",
    "Lyr": "Lyrae",
    "Men": "Mensae",
    "Mic": "Microscopii",
    "Mon": "Monocerotis",
    "Mus": "Muscae",
    "Nor": "Normae",
    "Oct": "Octantis",
    "Oph": "Ophiuchi",
    "Ori": "Orionis",
    "Pav": "Pavonis",
    "Peg": "Pegasi",
    "Per": "Persei",
    "Phe": "Phoenicis",
    "Pic": "Pictoris",
    "Psc": "Piscium",
    "PsA": "Piscis Austrini",
    "Pup": "Puppis",
    "Pyx": "Pyxidis",
    "Ret": "Reticuli",
    "Sge": "Sagittae",
    "Sgr": "Sagittarii",
    "Sco": "Scorpii",
    "Scl": "Sculptoris",
    "Sct": "Scuti",
    "Ser": "Serpentis",
    "Sex": "Sextantis",
    "Tau": "Tauri",
    "Tel": "Telescopii",
    "Tri": "Trianguli",
    "TrA": "Trianguli Australis",
    "Tuc": "Tucanae",
    "UMa": "Ursae Majoris",
    "UMi": "Ursae Minoris",
    "Vel": "Velorum",
    "Vir": "Virginis",
    "Vol": "Volantis",
    "Vul": "Vulpeculae",
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


def spoken(designation: str) -> list[str]:
    """A Bayer or Flamsteed designation as people say it, as SIMBAD writes it on input.

    'del Pav' -> ['Delta Pavonis']; '61 Cyg' -> ['61 Cygni']. A numbered Bayer letter gives
    the form with its number and the one without, since people say "Alpha Centauri" for
    either of 'alf1 Cen' and 'alf2 Cen'. Latin Bayer letters are kept: 'p Eri' -> ['p Eridani'].
    """
    letter, constellation = designation.split(" ")
    genitive = GENITIVES[constellation]
    if letter.isdigit():
        return [f"{letter} {genitive}"]
    match = re.fullmatch(r"(?P<letter>[a-z]+)(?P<index>\d*)", letter)
    if match is None or match["letter"] not in _GREEK_SPOKEN:
        return [f"{letter} {genitive}"]
    greek = _GREEK_SPOKEN[match["letter"]]
    if match["index"]:
        return [f"{greek} {match['index']} {genitive}", f"{greek} {genitive}"]
    return [f"{greek} {genitive}"]


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
