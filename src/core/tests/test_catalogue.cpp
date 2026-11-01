// Catalogue tests, run without the engine by CTest and `make check`. A tiny fixture
// database stands in for the real catalogue: a `star` table exposing the columns the
// core queries (the pipeline's `star` is a view with the same columns, so the core's
// SQL is identical against both).

#include "stellar_core.h"

#include "sqlite3.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

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

	bool write_fixture(const char* path, const std::string& long_name)
	{
		sqlite3* db = nullptr;
		if (sqlite3_open(path, &db) != SQLITE_OK)
		{
			sqlite3_close(db);
			return false;
		}
		const std::string ddl =
			"CREATE TABLE star (id INTEGER, name TEXT, x REAL, y REAL, z REAL, absmag REAL, teff REAL, spectral_type TEXT);"
			"INSERT INTO star VALUES (1,'A', 1.0, 2.0, 2.0, 5.0, 5800, 'G2V');"   // kept
			"INSERT INTO star VALUES (2,'B', NULL, NULL, NULL, 3.0, NULL, NULL);" // no position -> filtered out
			"INSERT INTO star VALUES (3,'C', 10.0, 0.0, 0.0, 4.2, NULL, 'M5V');"  // no teff -> coalesced to 5800
			"INSERT INTO star VALUES (4,'" +
			long_name + "', 0.0, 0.0, 1.0, 6.0, 4000, NULL);"; // long name truncated, no spectral type
		const int rc = sqlite3_exec(db, ddl.c_str(), nullptr, nullptr, nullptr);
		sqlite3_close(db);
		return rc == SQLITE_OK;
	}
}

int main()
{
	const char* path = "catalogue_fixture.sqlite";
	const std::string long_name(80, 'a'); // longer than STC_NAME_CAP, to test truncation
	std::remove(path);
	check(write_fixture(path, long_name), "write fixture");

	StcCatalogue* cat = stc_catalogue_open(path);
	check(cat != nullptr, "open fixture read-only");
	if (cat != nullptr)
	{
		check(stc_catalogue_star_count(cat) == 3, "three usable stars (the position-less one is filtered)");

		StcStar stars[8];
		const int32_t n = stc_catalogue_load_stars(cat, stars, 8);
		check(n == 3, "loaded three stars");

		// Table-scan order is by rowid: ids 1, 3, 4.
		check(stars[0].id == 1, "first row id");
		check(close(stars[0].x, 1.0) && close(stars[0].y, 2.0) && close(stars[0].z, 2.0), "first position");
		check(close(stars[0].absmag, 5.0), "first absmag");
		check(close(stars[0].teff, 5800.0), "first teff");
		check(std::strcmp(stars[0].name, "A") == 0, "first name");
		check(std::strcmp(stars[0].spectral_type, "G2V") == 0, "first spectral type");

		check(stars[1].id == 3, "second row id");
		check(close(stars[1].teff, 5800.0), "missing teff coalesced to 5800");
		check(std::strcmp(stars[1].spectral_type, "M5V") == 0, "second spectral type");

		// Long name truncated to the field capacity and NUL-terminated; no spectral type.
		check(stars[2].id == 4, "third row id");
		check(std::strlen(stars[2].name) == STC_NAME_CAP - 1, "long name truncated to capacity");
		check(stars[2].name[0] == 'a' && stars[2].name[STC_NAME_CAP - 2] == 'a', "truncated name content");
		check(stars[2].spectral_type[0] == '\0', "missing spectral type is empty");

		StcStar one[1];
		check(stc_catalogue_load_stars(cat, one, 1) == 1, "capacity is respected");
	}
	stc_catalogue_close(cat);
	stc_catalogue_close(nullptr); // a null handle is ignored

	check(stc_catalogue_open("does_not_exist_12345.sqlite") == nullptr, "missing file -> null");

	std::remove(path);
	std::printf(g_failures ? "%d check(s) failed\n" : "all checks passed\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
