"""Turns catalogue rows into objects: one star from all its sources, one planet per row.

Each source's claims are kept as they are. Then, for every quantity, one value
is marked preferred, and the pipeline derives what the app needs at the
reference epoch J2000.0: position, distance, Cartesian coordinates, velocity,
absolute magnitude and temperature.

Which value is preferred, best first:

1. A value the pipeline derived from preferred inputs (for example the J2000
   position propagated from Hipparcos). The temperature from colour is the
   exception: it is derived only where no temperature was measured.
2. Hipparcos New Reduction: positions, parallaxes and proper motions with
   their uncertainties.
3. HYG: names, magnitudes, colours, spectral types, radial velocities, and the
   astrometry of stars Hipparcos did not observe.
4. NASA Exoplanet Archive: host stars not found in HYG.

One exception: where HYG's distance departs from the Hipparcos parallax, HYG
has replaced a known-bad solution (alpha Centauri B is one), so HYG's distance
is preferred and no distance is derived from the parallax.
"""

import math
from collections.abc import Iterable

from stellar_pipeline import astrometry, identifiers
from stellar_pipeline.exoplanets import ArchivePlanet, Value
from stellar_pipeline.hipparcos import EPOCH as HIPPARCOS_EPOCH
from stellar_pipeline.hipparcos import HipparcosStar
from stellar_pipeline.hyg import EPOCH as HYG_EPOCH
from stellar_pipeline.hyg import OVERFLOWED_PROPER_MOTION, HygStar
from stellar_pipeline.records import Identifier, Measurement, ObjectRecord, Property
from stellar_pipeline.sources import EXOPLANETS, HIPPARCOS, HYG, PIPELINE_KEY

PRECEDENCE = (PIPELINE_KEY, HIPPARCOS.key, HYG.key, EXOPLANETS.key)

# HYG and Hipparcos are taken to agree on distance within this fraction; HYG
# rounds distances to four decimals.
DISTANCE_AGREEMENT = 1e-3
# Parallaxes at or below this give no distance, as in HYG (1000 pc).
MIN_PARALLAX_MAS = 1.0

# Derivations, with the version of each formula. Bump a version when its
# formula changes, so values from an older build can be told apart.
DERIVATIONS = {
    "epoch-propagation": (
        1,
        "Straight-line motion from the measurement epoch to J2000.0, using the preferred "
        "proper motion, and distance and radial velocity where both are known.",
    ),
    "distance-from-parallax": (
        1,
        "Distance = 1000 / parallax (mas), for parallaxes above 1 mas. The uncertainties are "
        "the distances at parallax -/+ one standard error; the upper one is missing when the "
        "parallax is within one error of zero.",
    ),
    "cartesian-position": (
        1,
        "x, y, z in parsecs, ICRS equatorial, from the preferred J2000 position and distance.",
    ),
    "space-velocity": (
        1,
        "vx, vy, vz in km/s from proper motion, distance and radial velocity.",
    ),
    "tangential-velocity": (
        1,
        "vx, vy, vz in km/s from proper motion and distance only: the radial velocity is "
        "unknown, so the star moves across the sky but not towards or away from the Sun.",
    ),
    "absolute-magnitude": (
        1,
        "M = m - 5 log10(d) + 5, from the preferred apparent V magnitude and distance.",
    ),
    "temperature-from-colour": (
        1,
        "Effective temperature from B-V treating the star as a blackbody, after Ballesteros "
        "(2012, EPL 97, 34008); only for -0.4 <= B-V <= 2.0, and only where no measured "
        "temperature is known. A rough estimate, poor for M dwarfs.",
    ),
}

_rank = {key: index for index, key in enumerate(PRECEDENCE)}


def choose_preferred(record: ObjectRecord) -> None:
    """Mark the best valid value of each quantity and property as preferred."""
    best: dict[str, Measurement] = {}
    for measurement in record.measurements:
        measurement.preferred = False
        if measurement.invalid:
            continue
        current = best.get(measurement.quantity)
        if current is None or _rank[measurement.source] < _rank[current.source]:
            best[measurement.quantity] = measurement
    for measurement in best.values():
        measurement.preferred = True

    best_property: dict[str, Property] = {}
    for prop in record.properties:
        prop.preferred = False
        current_property = best_property.get(prop.name)
        if current_property is None or _rank[prop.source] < _rank[current_property.source]:
            best_property[prop.name] = prop
    for prop in best_property.values():
        prop.preferred = True


def _derived(quantity: str, value: float, derivation: str, **kwargs: float | None) -> Measurement:
    return Measurement(
        quantity=quantity,
        value=value,
        source=PIPELINE_KEY,
        derivation=derivation,
        err_lower=kwargs.get("err_lower"),
        err_upper=kwargs.get("err_upper"),
        epoch=kwargs.get("epoch"),
    )


# Identifiers and names


def _hyg_identifiers(star: HygStar) -> list[Identifier]:
    found = [Identifier(identifiers.HYG, str(star.id), HYG.key)]
    if star.hip is not None:
        found.append(Identifier(identifiers.HIP, str(star.hip), HYG.key))
    if star.hd is not None:
        found.append(Identifier(identifiers.HD, str(star.hd), HYG.key))
    if star.hr is not None:
        found.append(Identifier(identifiers.HR, str(star.hr), HYG.key))
    if star.gliese is not None:
        found.append(Identifier(identifiers.GLIESE, identifiers.gliese(star.gliese), HYG.key))
    if star.constellation is not None:
        if star.bayer is not None:
            value = identifiers.bayer(star.bayer, star.constellation)
            found.append(Identifier(identifiers.BAYER, value, HYG.key))
        if star.flamsteed is not None:
            value = identifiers.flamsteed(star.flamsteed, star.constellation)
            found.append(Identifier(identifiers.FLAMSTEED, value, HYG.key))
        if star.variable is not None:
            value = f"{star.variable} {star.constellation}"
            found.append(Identifier(identifiers.VARIABLE, value, HYG.key))
    if star.is_sun:
        found.append(Identifier(identifiers.NAME, "Sun", HYG.key))
    if star.proper is not None:
        found.append(Identifier(identifiers.NAME, star.proper, HYG.key))
    return found


_NAME_ORDER = (
    identifiers.NAME,
    identifiers.BAYER,
    identifiers.FLAMSTEED,
    identifiers.GLIESE,
    identifiers.HD,
    identifiers.HIP,
    identifiers.HR,
    identifiers.EXOPLANET_ARCHIVE,
    identifiers.HYG,
)
_NAME_PREFIX = {
    identifiers.GLIESE: "GJ ",
    identifiers.HD: "HD ",
    identifiers.HIP: "HIP ",
    identifiers.HR: "HR ",
    identifiers.HYG: "HYG ",
}


def display_name(found: Iterable[Identifier]) -> str:
    """The name the app shows: a proper name, else Bayer, Flamsteed, Gliese, HD, HIP, ..."""
    by_catalogue: dict[str, str] = {}
    for identifier in found:
        by_catalogue.setdefault(identifier.catalogue, identifier.value)
    for catalogue in _NAME_ORDER:
        if catalogue in by_catalogue:
            return _NAME_PREFIX.get(catalogue, "") + by_catalogue[catalogue]
    message = "object has no identifier to name it by"
    raise ValueError(message)


# Stars


def _hyg_measurements(star: HygStar) -> list[Measurement]:
    source = HYG.key
    found = [
        Measurement("ra", star.ra_deg, source, epoch=HYG_EPOCH),
        Measurement("dec", star.dec_deg, source, epoch=HYG_EPOCH),
        Measurement("vmag", star.mag, source),
    ]
    for quantity, value in (("pmra", star.pmra), ("pmdec", star.pmdec)):
        overflowed = abs(value) == OVERFLOWED_PROPER_MOTION
        found.append(
            Measurement(
                quantity,
                value,
                source,
                invalid=overflowed,
                note="The field overflowed in HYG: the real value does not fit."
                if overflowed
                else None,
            )
        )
    if star.distance_pc is not None:
        found.append(Measurement("distance", star.distance_pc, source))
    if star.rv is not None:
        found.append(Measurement("rv", star.rv, source))
    if star.absmag is not None:
        found.append(Measurement("absmag", star.absmag, source))
    if star.luminosity is not None:
        found.append(Measurement("luminosity", star.luminosity, source))
    if star.colour_index is not None:
        found.append(Measurement("b_v", star.colour_index, source))
    return found


def _hipparcos_measurements(entry: HipparcosStar) -> list[Measurement]:
    source = HIPPARCOS.key
    ra = math.degrees(entry.ra_rad)
    dec = math.degrees(entry.dec_rad)
    # The RA error is along a great circle; in degrees of RA it grows towards the poles.
    ra_error = entry.e_ra / 3.6e6 / max(math.cos(entry.dec_rad), 1e-9)
    dec_error = entry.e_dec / 3.6e6
    found = [
        Measurement("ra", ra, source, ra_error, ra_error, epoch=HIPPARCOS_EPOCH),
        Measurement("dec", dec, source, dec_error, dec_error, epoch=HIPPARCOS_EPOCH),
        Measurement("parallax", entry.parallax, source, entry.e_parallax, entry.e_parallax),
        Measurement("pmra", entry.pmra, source, entry.e_pmra, entry.e_pmra),
        Measurement("pmdec", entry.pmdec, source, entry.e_pmdec, entry.e_pmdec),
        Measurement("hpmag", entry.hpmag, source, entry.e_hpmag, entry.e_hpmag),
    ]
    if entry.b_v is not None:
        found.append(Measurement("b_v", entry.b_v, source, entry.e_b_v, entry.e_b_v))
    return found


def _archive_value(quantity: str, value: Value, *, epoch: float | None = None) -> Measurement:
    reference = value.reference
    return Measurement(
        quantity,
        value.value,
        EXOPLANETS.key,
        err_lower=value.err_lower,
        err_upper=value.err_upper,
        limit=value.limit,
        epoch=epoch,
        reference_label=None if reference is None else reference.label,
        reference_url=None if reference is None else reference.url,
    )


def _host_measurements(host: ArchivePlanet) -> tuple[list[Measurement], list[Property]]:
    """What the exoplanet archive says about a host star."""
    source = EXOPLANETS.key
    reference = host.position_reference
    label = None if reference is None else reference.label
    url = None if reference is None else reference.url
    note = (
        "Epoch taken as J2015.5 (Gaia DR2 through TIC v8). TIC v8 took some positions "
        "from other catalogues, at other epochs."
        if host.position_epoch is not None
        else "The source does not state the epoch."
    )
    measurements = [
        Measurement("ra", host.ra_deg, source, epoch=host.position_epoch, note=note,
                    reference_label=label, reference_url=url),
        Measurement("dec", host.dec_deg, source, epoch=host.position_epoch, note=note,
                    reference_label=label, reference_url=url),
    ]  # fmt: skip
    if host.pmra is not None and host.pmdec is not None:
        measurements.append(Measurement("pmra", host.pmra, source))
        measurements.append(Measurement("pmdec", host.pmdec, source))
    if host.distance is not None:
        measurements.append(_archive_value("distance", host.distance))
    if host.vmag is not None:
        measurements.append(_archive_value("vmag", host.vmag))
    if host.teff is not None:
        measurements.append(_archive_value("teff", host.teff))
    properties = []
    if host.spectral_type is not None:
        properties.append(Property("spectral_type", host.spectral_type, source))
    return measurements, properties


def _host_identifiers(host: ArchivePlanet) -> list[Identifier]:
    source = EXOPLANETS.key
    found = [Identifier(identifiers.EXOPLANET_ARCHIVE, host.hostname, source)]
    for designation in (host.hip, host.hd):
        parsed = None if designation is None else identifiers.parse_prefixed(designation)
        if parsed is not None:
            found.append(Identifier(parsed[0], parsed[1], source))
    if host.gaia_dr3 is not None:
        found.append(Identifier(identifiers.GAIA_DR3, host.gaia_dr3, source))
    return found


def _from_source(record: ObjectRecord, quantity: str, source: str) -> Measurement | None:
    for measurement in record.measurements:
        if measurement.quantity == quantity and measurement.source == source:
            return measurement
    return None


def _derive_distance(record: ObjectRecord) -> None:
    """Distance from the Hipparcos parallax, unless HYG deliberately departs from it."""
    parallax = _from_source(record, "parallax", HIPPARCOS.key)
    if parallax is None:
        return
    hyg_distance = _from_source(record, "distance", HYG.key)
    p = parallax.value
    if hyg_distance is not None:
        from_parallax = 1000.0 / p if p > 0 else math.inf
        if abs(from_parallax - hyg_distance.value) > DISTANCE_AGREEMENT * hyg_distance.value:
            hyg_distance.note = (
                f"HYG departs from the Hipparcos parallax of {p} mas here, and its value is "
                "preferred."
            )
            return
    if p <= MIN_PARALLAX_MAS:
        return
    sigma = parallax.err_upper or 0.0
    distance = 1000.0 / p
    record.measurements.append(
        _derived(
            "distance",
            distance,
            "distance-from-parallax",
            err_lower=distance - 1000.0 / (p + sigma),
            err_upper=1000.0 / (p - sigma) - distance if p > sigma else None,
        )
    )


def _derive_position_and_motion(record: ObjectRecord) -> None:
    """J2000 position, Cartesian coordinates, velocity, absolute magnitude and temperature."""
    choose_preferred(record)
    ra = record.preferred("ra")
    dec = record.preferred("dec")
    pmra = record.preferred_value("pmra")
    pmdec = record.preferred_value("pmdec")
    distance = record.preferred_value("distance")
    rv = record.preferred_value("rv")
    if ra is None or dec is None:
        return

    ra_value, dec_value, epoch = ra.value, dec.value, ra.epoch
    if (
        epoch is not None
        and epoch != astrometry.REFERENCE_EPOCH
        and pmra is not None
        and pmdec is not None
    ):
        ra_value, dec_value = astrometry.propagate(
            ra.value,
            dec.value,
            pmra,
            pmdec,
            epoch,
            astrometry.REFERENCE_EPOCH,
            distance_pc=distance,
            rv_kms=rv,
        )
        epoch = astrometry.REFERENCE_EPOCH
        record.measurements.append(_derived("ra", ra_value, "epoch-propagation", epoch=epoch))
        record.measurements.append(_derived("dec", dec_value, "epoch-propagation", epoch=epoch))

    if distance is not None:
        position = astrometry.to_cartesian(ra_value, dec_value, distance)
        for axis, value in zip(("x", "y", "z"), position, strict=True):
            record.measurements.append(_derived(axis, value, "cartesian-position", epoch=epoch))
        if pmra is not None and pmdec is not None:
            velocity = astrometry.space_velocity(ra_value, dec_value, distance, pmra, pmdec, rv)
            derivation = "space-velocity" if rv is not None else "tangential-velocity"
            for axis, value in zip(("vx", "vy", "vz"), velocity, strict=True):
                record.measurements.append(_derived(axis, value, derivation))
        vmag = record.preferred_value("vmag")
        if vmag is not None:
            absmag = astrometry.absolute_magnitude(vmag, distance)
            record.measurements.append(_derived("absmag", absmag, "absolute-magnitude"))

    # The colour formula is a rough estimate: it is only a fallback for a measured temperature.
    b_v = record.preferred_value("b_v")
    if b_v is not None and record.preferred("teff") is None:
        teff = astrometry.temperature_from_bv(b_v)
        if teff is not None:
            record.measurements.append(_derived("teff", teff, "temperature-from-colour"))


def _sun(star: HygStar) -> ObjectRecord:
    """The Sun is the origin of the frame: it has no astrometry, only a place and a magnitude."""
    found = _hyg_identifiers(star)
    record = ObjectRecord(kind="star", name="Sun", source=HYG.key, identifiers=found)
    record.measurements.append(Measurement("vmag", star.mag, HYG.key))
    if star.absmag is not None:
        record.measurements.append(Measurement("absmag", star.absmag, HYG.key))
    if star.colour_index is not None:
        record.measurements.append(Measurement("b_v", star.colour_index, HYG.key))
    if star.luminosity is not None:
        record.measurements.append(Measurement("luminosity", star.luminosity, HYG.key))
    for axis in ("x", "y", "z", "vx", "vy", "vz"):
        derivation = "cartesian-position" if axis in ("x", "y", "z") else "space-velocity"
        measurement = _derived(axis, 0.0, derivation, epoch=astrometry.REFERENCE_EPOCH)
        measurement.note = "The Sun is the origin of the frame."
        record.measurements.append(measurement)
    if star.spectral_type is not None:
        record.properties.append(Property("spectral_type", star.spectral_type, HYG.key))
    b_v = star.colour_index
    teff = None if b_v is None else astrometry.temperature_from_bv(b_v)
    if teff is not None:
        record.measurements.append(_derived("teff", teff, "temperature-from-colour"))
    choose_preferred(record)
    return record


def star(
    hyg_star: HygStar,
    hipparcos_star: HipparcosStar | None = None,
    host: ArchivePlanet | None = None,
) -> ObjectRecord:
    """One HYG star, with its Hipparcos astrometry and, if it hosts planets, the archive's view."""
    if hyg_star.is_sun:
        return _sun(hyg_star)
    found = _hyg_identifiers(hyg_star)
    record = ObjectRecord(kind="star", name=display_name(found), source=HYG.key)
    record.identifiers = found
    record.measurements = _hyg_measurements(hyg_star)
    if hyg_star.spectral_type is not None:
        record.properties.append(Property("spectral_type", hyg_star.spectral_type, HYG.key))
    if hyg_star.constellation is not None:
        record.properties.append(Property("constellation", hyg_star.constellation, HYG.key))
    if hipparcos_star is not None:
        record.measurements += _hipparcos_measurements(hipparcos_star)
    if host is not None:
        record.identifiers += _host_identifiers(host)
        measurements, properties = _host_measurements(host)
        record.measurements += measurements
        record.properties += properties
    _derive_distance(record)
    _derive_position_and_motion(record)
    choose_preferred(record)
    return record


def host_star(host: ArchivePlanet) -> ObjectRecord:
    """A planet host that is not in HYG, from what the exoplanet archive says about it."""
    found = _host_identifiers(host)
    record = ObjectRecord(kind="star", name=host.hostname, source=EXOPLANETS.key)
    record.identifiers = found
    record.measurements, record.properties = _host_measurements(host)
    _derive_position_and_motion(record)
    choose_preferred(record)
    return record


# Planets

_PLANET_QUANTITIES = {
    "pl_orbper": "orbital_period",
    "pl_orbsmax": "semi_major_axis",
    "pl_orbeccen": "eccentricity",
    "pl_orbincl": "inclination",
    "pl_orblper": "periastron_argument",
    "pl_orbtper": "periastron_time",
    "pl_rade": "radius",
    "pl_bmasse": "mass",
    "pl_eqt": "equilibrium_temperature",
}


def planet(row: ArchivePlanet) -> ObjectRecord:
    source = EXOPLANETS.key
    record = ObjectRecord(kind="planet", name=row.name, source=source)
    record.identifiers.append(Identifier(identifiers.EXOPLANET_ARCHIVE, row.name, source))
    for parameter, value in row.parameters.items():
        record.measurements.append(_archive_value(_PLANET_QUANTITIES[parameter], value))
    if row.discovery_year is not None:
        record.measurements.append(Measurement("discovery_year", row.discovery_year, source))
    for name, text in (
        ("discovery_method", row.discovery_method),
        ("mass_provenance", row.mass_provenance),
        ("letter", row.letter),
    ):
        if text is not None:
            record.properties.append(Property(name, text, source))
    choose_preferred(record)
    return record
