"""Finds the HYG star behind each exoplanet host.

The archive names hosts its own way ("eps Eri", "Kepler-22") but often gives
HIP and HD numbers. Hosts are matched by HIP, then HD, then a Gliese number
in the host name, then by position. A host with no match becomes a star of
its own, described by the archive.
"""

import math
from collections import Counter, defaultdict
from collections.abc import Iterable
from dataclasses import dataclass, field

from stellar_pipeline import astrometry, identifiers
from stellar_pipeline.exoplanets import ArchivePlanet
from stellar_pipeline.hipparcos import EPOCH as HIPPARCOS_EPOCH
from stellar_pipeline.hipparcos import HipparcosStar
from stellar_pipeline.hyg import HygStar

# Positions must agree within this for stars with Hipparcos astrometry...
MATCH_RADIUS_ARCSEC = 2.0
# ...and within this for the others, whose HYG positions come from the Gliese
# catalogue and can be tens of arcseconds out (Wolf 359 is 6.6" from SIMBAD).
MATCH_RADIUS_COARSE_ARCSEC = 30.0
# And the V magnitudes, where both are known, within this.
MATCH_MAGNITUDE = 1.0

_CELL_DEG = 1.0


@dataclass(frozen=True, slots=True)
class _Entry:
    hyg_id: int
    ra_deg: float
    dec_deg: float
    mag: float
    radius_arcsec: float


@dataclass
class HygIndex:
    """What matching needs to know about HYG, gathered in one pass over the catalogue."""

    by_hip: dict[int, int] = field(default_factory=dict)
    by_hd: dict[int, list[int]] = field(default_factory=lambda: defaultdict(list))
    by_gliese: dict[str, int] = field(default_factory=dict)
    primaries: set[int] = field(default_factory=set)
    cells: dict[tuple[int, int], list[_Entry]] = field(default_factory=lambda: defaultdict(list))

    def add(self, star: HygStar, hipparcos_star: HipparcosStar | None = None) -> None:
        """Index a star; with its Hipparcos entry, at the position propagated from that.

        HYG's own J2000 right ascensions are out by up to 18" for fast stars far
        from the equator, because HYG left cos(dec) out when propagating them.
        """
        if star.is_sun:
            return
        if star.hip is not None:
            self.by_hip[star.hip] = star.id
        if star.hd is not None:
            self.by_hd[star.hd].append(star.id)
        if star.gliese is not None:
            self.by_gliese[identifiers.gliese(star.gliese)] = star.id
        if star.primary_id is None:
            self.primaries.add(star.id)
        if hipparcos_star is None:
            ra, dec = star.ra_deg, star.dec_deg
            radius = MATCH_RADIUS_COARSE_ARCSEC
        else:
            ra, dec = astrometry.propagate(
                math.degrees(hipparcos_star.ra_rad),
                math.degrees(hipparcos_star.dec_rad),
                hipparcos_star.pmra,
                hipparcos_star.pmdec,
                HIPPARCOS_EPOCH,
                astrometry.REFERENCE_EPOCH,
            )
            radius = MATCH_RADIUS_ARCSEC
        self.cells[_cell(ra, dec)].append(_Entry(star.id, ra, dec, star.mag, radius))

    def nearest(self, positions: list[tuple[float, float]], mag: float | None) -> int | None:
        """The closest star to any of `positions` within its match radius, with agreeing magnitude.

        Several positions allow for not knowing the epoch of the one given.
        """
        best: tuple[float, int] | None = None
        ra_deg, dec_deg = positions[0]
        for entry in self._neighbours(ra_deg, dec_deg):
            separation = min(
                astrometry.separation_arcsec(ra, dec, entry.ra_deg, entry.dec_deg)
                for ra, dec in positions
            )
            if separation > entry.radius_arcsec:
                continue
            if mag is not None and abs(mag - entry.mag) > MATCH_MAGNITUDE:
                continue
            if best is None or separation < best[0]:
                best = (separation, entry.hyg_id)
        return None if best is None else best[1]

    def _neighbours(self, ra_deg: float, dec_deg: float) -> Iterable[_Entry]:
        ra_cell, dec_cell = _cell(ra_deg, dec_deg)
        cells_in_ra = round(360 / _CELL_DEG)
        # Near the poles a cell spans little sky in RA, so search the whole ring.
        near_pole = abs(dec_deg) > 80
        ra_cells = range(cells_in_ra) if near_pole else (ra_cell - 1, ra_cell, ra_cell + 1)
        for dec_index in (dec_cell - 1, dec_cell, dec_cell + 1):
            for ra_index in ra_cells:
                yield from self.cells.get((ra_index % cells_in_ra, dec_index), ())


def _cell(ra_deg: float, dec_deg: float) -> tuple[int, int]:
    return int(ra_deg // _CELL_DEG) % round(360 / _CELL_DEG), math.floor(dec_deg / _CELL_DEG)


@dataclass
class HostMatches:
    # Archive host name -> HYG id, for hosts found in HYG.
    to_hyg: dict[str, int] = field(default_factory=dict)
    # How each host was matched, for the build report.
    methods: Counter[str] = field(default_factory=Counter)


def _by_identifier(host: ArchivePlanet, index: HygIndex) -> tuple[int, str] | None:
    for designation in (host.hip, host.hd, host.hostname):
        parsed = None if designation is None else identifiers.parse_prefixed(designation)
        if parsed is None:
            continue
        catalogue, value = parsed
        if catalogue == identifiers.HIP and value.isdigit() and int(value) in index.by_hip:
            return index.by_hip[int(value)], "HIP"
        if catalogue == identifiers.HD and value.isdigit():
            candidates = index.by_hd.get(int(value), [])
            primaries = [c for c in candidates if c in index.primaries]
            if len(candidates) == 1:
                return candidates[0], "HD"
            if len(primaries) == 1:
                return primaries[0], "HD"
        if catalogue == identifiers.GLIESE and value in index.by_gliese:
            return index.by_gliese[value], "GJ"
    return None


def _by_position(host: ArchivePlanet, index: HygIndex) -> int | None:
    # The archive's position is usually at J2015.5 but sometimes already at
    # about J2000 (see exoplanets.py), so try it both moved and as given.
    positions = [(host.ra_deg, host.dec_deg)]
    if host.position_epoch is not None and host.pmra is not None and host.pmdec is not None:
        positions.insert(
            0,
            astrometry.propagate(
                host.ra_deg,
                host.dec_deg,
                host.pmra,
                host.pmdec,
                host.position_epoch,
                astrometry.REFERENCE_EPOCH,
            ),
        )
    mag = None if host.vmag is None else host.vmag.value
    return index.nearest(positions, mag)


def match_hosts(hosts: Iterable[ArchivePlanet], index: HygIndex) -> HostMatches:
    matches = HostMatches()
    for host in hosts:
        found = _by_identifier(host, index)
        if found is None:
            hyg_id = _by_position(host, index)
            found = None if hyg_id is None else (hyg_id, "position")
        if found is None:
            matches.methods["unmatched"] += 1
            continue
        matches.to_hyg[host.hostname] = found[0]
        matches.methods[found[1]] += 1
    return matches
