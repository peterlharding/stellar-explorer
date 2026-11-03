#pragma once

// Astrometry: the science the front end must not own (ADR 0008). Plain C++ in the
// stellar::core namespace; the C ABI in stellar_core.h wraps it. 64-bit throughout.

namespace stellar::core
{
	// A position in parsecs, right-handed ICRS equatorial Cartesian.
	struct Vec3
	{
		double x;
		double y;
		double z;
	};

	// Distance in parsecs from a parallax in milliarcseconds; NaN if parallax <= 0.
	double distance_parsecs(double parallax_mas);

	// ICRS Cartesian position (parsecs) from right ascension and declination
	// (degrees) and distance (parsecs).
	Vec3 icrs_position(double ra_deg, double dec_deg, double distance_pc);
}
