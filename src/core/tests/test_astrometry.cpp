// Core reference tests, run without the engine (ADR 0008) by CTest and `make check`.
// They exercise both the internal C++ and the C ABI. Zero dependencies: a failure
// prints and the process exits non-zero.

#include "stellar_core.h"

#include "astrometry.hpp"

#include <cmath>
#include <cstdio>

namespace
{
	int g_failures = 0;

	void check(bool ok, const char* what)
	{
		if (!ok)
		{
			std::printf("FAIL: %s\n", what);
			++g_failures;
		}
	}

	bool close(double a, double b, double tol = 1e-9)
	{
		return std::fabs(a - b) <= tol;
	}
}

int main()
{
	// Distance from parallax: exact relationships.
	check(close(stc_distance_parsecs(100.0), 10.0), "100 mas -> 10 pc");
	check(close(stc_distance_parsecs(1000.0), 1.0), "1000 mas -> 1 pc");
	check(std::isnan(stc_distance_parsecs(0.0)), "0 mas -> NaN");
	check(std::isnan(stc_distance_parsecs(-5.0)), "negative parallax -> NaN");

	// A known star, to a tolerance: Proxima Centauri, parallax ~768.07 mas -> ~1.302 pc.
	check(close(stc_distance_parsecs(768.07), 1.3020, 1e-3), "Proxima ~1.302 pc");

	// ICRS position: axis-aligned cases, in parsecs.
	StcVec3 p = stc_icrs_position(0.0, 0.0, 10.0);
	check(close(p.x, 10.0) && close(p.y, 0.0) && close(p.z, 0.0), "RA 0, Dec 0 -> +x");
	p = stc_icrs_position(90.0, 0.0, 10.0);
	check(close(p.x, 0.0) && close(p.y, 10.0) && close(p.z, 0.0), "RA 90, Dec 0 -> +y");
	p = stc_icrs_position(0.0, 90.0, 5.0);
	check(close(p.x, 0.0) && close(p.y, 0.0) && close(p.z, 5.0), "Dec 90 -> +z");

	// The transform preserves distance.
	p = stc_icrs_position(123.4, -56.7, 42.0);
	check(close(std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z), 42.0), "norm == distance");

	// ABI version matches the header.
	check(stc_abi_version() == STC_ABI_VERSION, "ABI version matches header");

	std::printf(g_failures ? "%d check(s) failed\n" : "all checks passed\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
