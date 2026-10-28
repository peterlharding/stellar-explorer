"""Positions and motions of stars.

Conventions, used throughout the catalogue:

- Right ascension and declination in degrees, ICRS.
- Proper motion in mas/yr; the right ascension component already includes cos(dec).
- Parallax in mas, distance in parsecs, radial velocity in km/s.
- Cartesian positions in parsecs and velocities in km/s, in the ICRS equatorial
  frame centred on the Sun: +x towards RA 0h Dec 0, +z towards the north
  celestial pole. This is the frame HYG uses.
- Epochs are Julian years (J2000.0 is 2000.0).

Stars move in straight lines at constant velocity, which is the model the app
uses as well (ARCHITECTURE.md, "Time and motion").
"""

import math

type Vec3 = tuple[float, float, float]

AU_KM = 149_597_870.7
PARSEC_KM = AU_KM * 648_000 / math.pi
JULIAN_YEAR_S = 365.25 * 86_400
# km/s for a proper motion of 1 arcsec/yr at 1 pc, i.e. 1 AU/yr.
KMS_PER_AU_PER_YEAR = AU_KM / JULIAN_YEAR_S
PC_PER_YEAR_PER_KMS = JULIAN_YEAR_S / PARSEC_KM
MAS_TO_RAD = math.pi / (180 * 3600 * 1000)

REFERENCE_EPOCH = 2000.0


def _add(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def _scale(a: Vec3, k: float) -> Vec3:
    return (a[0] * k, a[1] * k, a[2] * k)


def _norm(a: Vec3) -> float:
    return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])


def unit_vector(ra_deg: float, dec_deg: float) -> Vec3:
    ra, dec = math.radians(ra_deg), math.radians(dec_deg)
    return (math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec))


def _local_axes(ra_deg: float, dec_deg: float) -> tuple[Vec3, Vec3]:
    """Unit vectors towards increasing right ascension (east) and declination (north)."""
    ra, dec = math.radians(ra_deg), math.radians(dec_deg)
    east = (-math.sin(ra), math.cos(ra), 0.0)
    north = (-math.sin(dec) * math.cos(ra), -math.sin(dec) * math.sin(ra), math.cos(dec))
    return east, north


def to_cartesian(ra_deg: float, dec_deg: float, distance_pc: float) -> Vec3:
    return _scale(unit_vector(ra_deg, dec_deg), distance_pc)


def to_spherical(position: Vec3) -> tuple[float, float, float]:
    """Return right ascension and declination in degrees, and the length of `position`."""
    x, y, z = position
    length = _norm(position)
    ra = math.degrees(math.atan2(y, x)) % 360.0
    dec = math.degrees(math.asin(max(-1.0, min(1.0, z / length))))
    return ra, dec, length


def space_velocity(
    ra_deg: float,
    dec_deg: float,
    distance_pc: float,
    pmra_mas_yr: float,
    pmdec_mas_yr: float,
    rv_kms: float | None,
) -> Vec3:
    """Velocity in km/s. With no radial velocity the star moves across the sky only."""
    east, north = _local_axes(ra_deg, dec_deg)
    k = KMS_PER_AU_PER_YEAR * distance_pc / 1000.0
    tangential = _add(_scale(east, pmra_mas_yr * k), _scale(north, pmdec_mas_yr * k))
    if rv_kms is None:
        return tangential
    return _add(tangential, _scale(unit_vector(ra_deg, dec_deg), rv_kms))


def propagate(
    ra_deg: float,
    dec_deg: float,
    pmra_mas_yr: float,
    pmdec_mas_yr: float,
    from_epoch: float,
    to_epoch: float,
    *,
    distance_pc: float | None = None,
    rv_kms: float | None = None,
) -> tuple[float, float]:
    """Move a position from one epoch to another along a straight line in space.

    Without a distance only the direction can be moved, and the radial velocity
    is ignored. With a distance and a radial velocity the change of distance is
    included, which matters only for the nearest, fastest stars.
    """
    dt = to_epoch - from_epoch
    if distance_pc is None or rv_kms is None:
        east, north = _local_axes(ra_deg, dec_deg)
        drift = _add(
            _scale(east, pmra_mas_yr * MAS_TO_RAD * dt),
            _scale(north, pmdec_mas_yr * MAS_TO_RAD * dt),
        )
        ra, dec, _ = to_spherical(_add(unit_vector(ra_deg, dec_deg), drift))
        return ra, dec
    start = to_cartesian(ra_deg, dec_deg, distance_pc)
    velocity = space_velocity(ra_deg, dec_deg, distance_pc, pmra_mas_yr, pmdec_mas_yr, rv_kms)
    ra, dec, _ = to_spherical(_add(start, _scale(velocity, PC_PER_YEAR_PER_KMS * dt)))
    return ra, dec


def separation_arcsec(ra1_deg: float, dec1_deg: float, ra2_deg: float, dec2_deg: float) -> float:
    """Angle between two directions, accurate for small and large separations."""
    a, b = unit_vector(ra1_deg, dec1_deg), unit_vector(ra2_deg, dec2_deg)
    cross = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
    return math.degrees(math.atan2(_norm(cross), dot)) * 3600


def absolute_magnitude(apparent: float, distance_pc: float) -> float:
    return apparent - 5 * math.log10(distance_pc) + 5


def temperature_from_bv(b_minus_v: float) -> float | None:
    """Effective temperature in kelvin from B-V, after Ballesteros (2012, EPL 97, 34008).

    The formula treats the star as a blackbody and holds for roughly
    -0.4 < B-V < 2.0; outside that range there is no answer.
    """
    if not -0.4 <= b_minus_v <= 2.0:
        return None
    return 4600 * (1 / (0.92 * b_minus_v + 1.7) + 1 / (0.92 * b_minus_v + 0.62))
