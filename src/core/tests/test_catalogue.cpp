// Catalogue tests, run without the engine by CTest and `make check`. A tiny fixture
// database stands in for the real catalogue: the pipeline's `object`, `measurement` and
// `property` tables, cut down to the columns the core queries, with rows the queries
// must skip (values not preferred, an object that is not a star).

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
			"CREATE TABLE object (id INTEGER PRIMARY KEY, kind TEXT, name TEXT);"
			"CREATE TABLE measurement (object_id INTEGER, quantity TEXT, value REAL, preferred INTEGER);"
			"CREATE TABLE property (object_id INTEGER, name TEXT, value TEXT, preferred INTEGER);"
			// Names out of id order, so a scan by name would give a different order.
			"INSERT INTO object VALUES (1, 'star', 'Sol'), (2, 'star', 'B'), (3, 'star', 'C'), (4, 'star', '" +
			long_name +
			"'), (5, 'planet', 'P');"
			"INSERT INTO measurement VALUES"
			" (1, 'x', 1.0, 1), (1, 'y', 2.0, 1), (1, 'z', 2.0, 1), (1, 'absmag', 5.0, 1), (1, 'teff', 5800, 1),"
			" (1, 'x', 99.0, 0), (1, 'absmag', 99.0, 0),"                                      // not preferred -> ignored
			" (2, 'absmag', 3.0, 1),"                                                          // no position -> filtered out
			" (3, 'x', 10.0, 1), (3, 'y', 0.0, 1), (3, 'z', 0.0, 1), (3, 'absmag', 4.2, 1)," // no teff -> coalesced to 5800
			" (4, 'x', 0.0, 1), (4, 'y', 0.0, 1), (4, 'z', 1.0, 1), (4, 'absmag', 6.0, 1), (4, 'teff', 4000, 1),"
			" (5, 'x', 0.0, 1), (5, 'y', 0.0, 1), (5, 'z', 0.0, 1), (5, 'absmag', 9.0, 1);" // not a star -> filtered out
			"INSERT INTO property VALUES"
			" (1, 'spectral_type', 'G2V', 1), (1, 'spectral_type', 'K0', 0), (3, 'spectral_type', 'M5V', 1);";
			// 4: long name truncated, no spectral type
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
		check(stc_catalogue_star_count(cat) == 3, "three usable stars (the position-less one and the planet are filtered)");

		StcStar stars[8];
		const int32_t n = stc_catalogue_load_stars(cat, stars, 8);
		check(n == 3, "loaded three stars");

		// In id order: 1, 3, 4.
		check(stars[0].id == 1, "first row id");
		check(close(stars[0].x, 1.0) && close(stars[0].y, 2.0) && close(stars[0].z, 2.0), "first position");
		check(close(stars[0].absmag, 5.0), "first absmag");
		check(close(stars[0].teff, 5800.0), "first teff");
		check(std::strcmp(stars[0].name, "Sol") == 0, "first name");
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
