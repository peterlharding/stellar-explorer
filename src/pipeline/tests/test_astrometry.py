import math

import pytest

from stellar_pipeline import astrometry


def test_cartesian_round_trip() -> None:
    position = astrometry.to_cartesian(217.4289, -62.6795, 1.2959)
    ra, dec, distance = astrometry.to_spherical(position)
    assert (ra, dec, distance) == pytest.approx((217.4289, -62.6795, 1.2959))


def test_axes_follow_the_icrs_equatorial_frame() -> None:
    assert astrometry.to_cartesian(0, 0, 1) == pytest.approx((1, 0, 0))
    assert astrometry.to_cartesian(90, 0, 1) == pytest.approx((0, 1, 0), abs=1e-15)
    assert astrometry.to_cartesian(0, 90, 1) == pytest.approx((0, 0, 1), abs=1e-15)


def test_barnards_star_moves_from_the_hipparcos_epoch_to_j2000() -> None:
    # Hipparcos New Reduction at J1991.25; SIMBAD at J2000 (from Gaia DR3).
    ra, dec = astrometry.propagate(
        math.degrees(4.7028598776),
        math.degrees(0.0814769927),
        -798.58,
        10328.12,
        1991.25,
        2000.0,
        distance_pc=1000 / 548.31,
        rv_kms=-110.6,
    )
    assert astrometry.separation_arcsec(ra, dec, 269.4520769586, 4.6933649666) < 0.2


def test_propagation_near_the_pole_does_not_blow_up() -> None:
    # Polaris: proper motion in RA includes cos(dec), which is 0.013 here.
    ra, dec = astrometry.propagate(37.946, 89.2641, 44.48, -11.85, 1991.25, 2000.0)
    moved = astrometry.separation_arcsec(37.946, 89.2641, ra, dec)
    expected = math.hypot(44.48, -11.85) * 8.75 / 1000
    assert moved == pytest.approx(expected, rel=1e-6)


def test_radial_velocity_adds_perspective_acceleration() -> None:
    # Barnard's Star approaches fast enough that its proper motion grows
    # measurably: the extra shift is mu * (v_r / d) * dt^2.
    pmra, pmdec, distance, rv, dt = -798.58, 10328.12, 1.8238, -110.6, 8.75
    args = (269.45, 4.69, pmra, pmdec, 1991.25, 1991.25 + dt)
    without = astrometry.propagate(*args)
    with_rv = astrometry.propagate(*args, distance_pc=distance, rv_kms=rv)
    mu = math.hypot(pmra, pmdec) / 1000
    expected = mu * abs(rv) * astrometry.PC_PER_YEAR_PER_KMS / distance * dt**2
    assert astrometry.separation_arcsec(*without, *with_rv) == pytest.approx(expected, rel=0.02)


def test_space_velocity_of_barnards_star() -> None:
    velocity = astrometry.space_velocity(269.452, 4.6934, 1.8238, -798.58, 10328.12, -110.6)
    tangential = astrometry.KMS_PER_AU_PER_YEAR * math.hypot(-798.58, 10328.12) / 1000 * 1.8238
    speed = math.sqrt(sum(v * v for v in velocity))
    assert speed == pytest.approx(math.hypot(tangential, 110.6), rel=1e-9)
    assert speed == pytest.approx(142.3, abs=0.1)


def test_tangential_velocity_is_perpendicular_to_the_line_of_sight() -> None:
    velocity = astrometry.space_velocity(10, 20, 5, 100, -50, None)
    direction = astrometry.unit_vector(10, 20)
    assert sum(v * d for v, d in zip(velocity, direction, strict=True)) == pytest.approx(
        0, abs=1e-12
    )


def test_separation_handles_tiny_and_large_angles() -> None:
    assert astrometry.separation_arcsec(10, 10, 10, 10 + 1 / 3600) == pytest.approx(1, rel=1e-9)
    assert astrometry.separation_arcsec(0, 0, 180, 0) == pytest.approx(180 * 3600)


def test_absolute_magnitude_at_ten_parsecs_is_the_apparent_magnitude() -> None:
    assert astrometry.absolute_magnitude(4.2, 10) == pytest.approx(4.2)
    assert astrometry.absolute_magnitude(-1.44, 2.6371) == pytest.approx(1.454, abs=1e-3)


def test_temperature_from_colour() -> None:
    sun = astrometry.temperature_from_bv(0.656)
    assert sun == pytest.approx(5770, abs=50)
    assert astrometry.temperature_from_bv(2.5) is None
