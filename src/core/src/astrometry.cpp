#include "astrometry.hpp"

#include <cmath>
#include <limits>

namespace stellar::core
{
	namespace
	{
		// Not M_PI: that needs _USE_MATH_DEFINES on MSVC, and the core builds with its
		// own toolchain on each platform (ADR 0014).
		constexpr double kPi = 3.14159265358979323846;
		constexpr double kDegToRad = kPi / 180.0;
	}

	double distance_parsecs(double parallax_mas)
	{
		// A parallax in arcseconds is 1/distance in parsecs, by definition; mas is 1/1000.
		if (parallax_mas > 0.0)
		{
			return 1000.0 / parallax_mas;
		}
		return std::numeric_limits<double>::quiet_NaN();
	}

	Vec3 icrs_position(double ra_deg, double dec_deg, double distance_pc)
	{
		const double ra = ra_deg * kDegToRad;
		const double dec = dec_deg * kDegToRad;
		const double cos_dec = std::cos(dec);
		return Vec3{
			distance_pc * cos_dec * std::cos(ra),
			distance_pc * cos_dec * std::sin(ra),
			distance_pc * std::sin(dec)};
	}
}
