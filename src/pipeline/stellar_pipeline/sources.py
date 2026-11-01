"""The catalogues the pipeline reads, and where each one comes from.

Each source becomes a row in the catalogue's `source` table, so the app can show
where every value came from and under what licence.
"""

from dataclasses import dataclass
from urllib.parse import urlencode


@dataclass(frozen=True)
class SourceSpec:
    key: str
    name: str
    version: str
    licence: str
    url: str
    citation: str
    download_url: str
    filename: str
    # Set when the file is fixed: the download must match it exactly. A source
    # that changes over time (the exoplanet archive) has none, and its hash is
    # recorded when it is fetched instead.
    sha256: str | None
    # Mirrors tried, in order, when download_url cannot be reached. For a fixed
    # source each must serve the identical file, so the sha256 above still holds.
    mirror_urls: tuple[str, ...] = ()


HYG = SourceSpec(
    key="hyg",
    name="HYG Database",
    version="4.4",
    licence="CC BY-SA 4.0",
    url="https://codeberg.org/astronexus/hyg",
    citation="Nash, D., The HYG Database, version 4.4, Astronexus, https://astronexus.com/projects/hyg",
    download_url=(
        "https://codeberg.org/astronexus/hyg/media/commit/"
        "53e3df311869e813ace5f1ad2ec4ce909f13256c/data/hyg/CURRENT/hyg_v44.csv.gz"
    ),
    filename="hyg_v44.csv.gz",
    sha256="00b349893b9a53106dd488d8371e8d2fa586043e500bb3cdb8bff3931682197d",
)

HIPPARCOS = SourceSpec(
    key="hipparcos-2007",
    name="Hipparcos, the New Reduction",
    version="2007 (CDS I/311, files of 15 September 2008)",
    licence="Free for scientific use with citation (VizieR rules of usage)",
    url="https://cdsarc.cds.unistra.fr/viz-bin/cat/I/311",
    citation="van Leeuwen, F. 2007, A&A 474, 653, 2007A&A...474..653V",
    download_url="https://cdsarc.cds.unistra.fr/ftp/I/311/hip2.dat.gz",
    filename="hip2.dat.gz",
    sha256="8e624f843d4254a9b7c2e8dda8e3158dbe825bf98f6bf3dbbae7f0d0b73d6858",
    # CDS can be unreachable from some networks; the Harvard VizieR mirror serves the
    # byte-identical file (same sha256), so the cited source stays CDS I/311.
    mirror_urls=("https://vizier.cfa.harvard.edu/ftp/cats/I/311/hip2.dat.gz",),
)

# Planet parameters that carry an upper and lower uncertainty, a limit flag and
# a reference. Each becomes a measurement of the same name without the `pl_`.
EXOPLANET_PARAMETERS = (
    "pl_orbper",
    "pl_orbsmax",
    "pl_orbeccen",
    "pl_orbincl",
    "pl_orblper",
    "pl_orbtper",
    "pl_rade",
    "pl_bmasse",
    "pl_eqt",
)

EXOPLANET_COLUMNS = (
    "pl_name",
    "pl_letter",
    "hostname",
    "hip_name",
    "hd_name",
    "gaia_dr3_id",
    "ra",
    "dec",
    "ra_reflink",
    "sy_pmra",
    "sy_pmdec",
    "sy_dist",
    "sy_disterr1",
    "sy_disterr2",
    "sy_dist_reflink",
    "sy_vmag",
    "sy_vmagerr1",
    "sy_vmagerr2",
    "sy_vmag_reflink",
    "st_spectype",
    "st_teff",
    "st_tefferr1",
    "st_tefferr2",
    "st_tefflim",
    "st_teff_reflink",
    "discoverymethod",
    "disc_year",
    "pl_bmassprov",
    *(
        column
        for parameter in EXOPLANET_PARAMETERS
        for column in (
            parameter,
            f"{parameter}err1",
            f"{parameter}err2",
            f"{parameter}lim",
            f"{parameter}_reflink",
        )
    ),
)

_EXOPLANET_QUERY = f"select {','.join(EXOPLANET_COLUMNS)} from pscomppars order by pl_name"

EXOPLANETS = SourceSpec(
    key="nasa-exoplanet-archive",
    name="NASA Exoplanet Archive, Planetary Systems Composite Parameters",
    # Replaced by the retrieval date when the file is fetched: the table changes weekly.
    version="",
    licence="Free to use with acknowledgement (NASA Exoplanet Archive)",
    url="https://exoplanetarchive.ipac.caltech.edu/",
    citation=(
        "NASA Exoplanet Archive, Planetary Systems Composite Table, doi:10.26133/NEA13. "
        "This research has made use of the NASA Exoplanet Archive, which is operated by the "
        "California Institute of Technology, under contract with the National Aeronautics and "
        "Space Administration under the Exoplanet Exploration Program."
    ),
    download_url="https://exoplanetarchive.ipac.caltech.edu/TAP/sync?"
    + urlencode({"query": _EXOPLANET_QUERY, "format": "csv"}),
    filename="pscomppars.csv",
    sha256=None,
)

ALL = (HYG, HIPPARCOS, EXOPLANETS)

PIPELINE_KEY = "stellar-explorer-pipeline"
PIPELINE_URL = "https://github.com/peterlharding/stellar-explorer"
