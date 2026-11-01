// The C ABI (stellar_core.h) implemented over the internal C++ (astrometry.hpp).
// This translation unit is the only place the two meet; nothing C++ escapes it.

#include "stellar_core.h"

#include "astrometry.hpp"

extern "C"
{
	uint32_t stc_abi_version(void)
	{
		return STC_ABI_VERSION;
	}

	double stc_distance_parsecs(double parallax_mas)
	{
		return stellar::core::distance_parsecs(parallax_mas);
	}

	StcVec3 stc_icrs_position(double ra_deg, double dec_deg, double distance_pc)
	{
		const stellar::core::Vec3 v = stellar::core::icrs_position(ra_deg, dec_deg, distance_pc);
		return StcVec3{v.x, v.y, v.z};
	}
}
