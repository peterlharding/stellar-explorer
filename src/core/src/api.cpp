// The C ABI (stellar_core.h) implemented over the internal C++ (astrometry.hpp).
// This translation unit is the only place the two meet; nothing C++ escapes it.

#include "stellar_core.h"

#include "astrometry.hpp"
#include "catalogue.hpp"

namespace
{
	// StcCatalogue is opaque in the header; it is a stellar::core::Catalogue behind it.
	stellar::core::Catalogue* as_core(StcCatalogue* c)
	{
		return reinterpret_cast<stellar::core::Catalogue*>(c);
	}
	const stellar::core::Catalogue* as_core(const StcCatalogue* c)
	{
		return reinterpret_cast<const stellar::core::Catalogue*>(c);
	}
}

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

	StcCatalogue* stc_catalogue_open(const char* path)
	{
		return reinterpret_cast<StcCatalogue*>(stellar::core::Catalogue::open(path));
	}

	void stc_catalogue_close(StcCatalogue* catalogue)
	{
		delete as_core(catalogue); // delete nullptr is a no-op
	}

	int64_t stc_catalogue_star_count(const StcCatalogue* catalogue)
	{
		return catalogue == nullptr ? -1 : as_core(catalogue)->star_count();
	}

	int32_t stc_catalogue_load_stars(const StcCatalogue* catalogue, StcStar* out, int32_t capacity)
	{
		return catalogue == nullptr ? -1 : as_core(catalogue)->load_stars(out, capacity);
	}
}
