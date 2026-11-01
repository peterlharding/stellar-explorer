#include "catalogue.hpp"

#include "sqlite3.h"

namespace stellar::core
{
	namespace
	{
		// Stars usable by the renderer: a position and an absolute magnitude (the Sun is
		// at the origin). Matches the query the spike's StarField runs against the `star`
		// view; teff defaults to 5800 K and spectral_type to empty when absent.
		constexpr const char* kSelectStars =
			"SELECT id, x, y, z, absmag, coalesce(teff, 5800), name, coalesce(spectral_type, '') "
			"FROM star WHERE x IS NOT NULL AND absmag IS NOT NULL";
		constexpr const char* kCountStars =
			"SELECT count(*) FROM star WHERE x IS NOT NULL AND absmag IS NOT NULL";

		// Copy a SQLite text column into a fixed field: truncated to fit, always
		// NUL-terminated, empty when the column is NULL.
		void copy_text(char* dst, int capacity, const unsigned char* src)
		{
			if (capacity <= 0)
			{
				return;
			}
			int i = 0;
			if (src != nullptr)
			{
				for (; i < capacity - 1 && src[i] != '\0'; ++i)
				{
					dst[i] = static_cast<char>(src[i]);
				}
			}
			dst[i] = '\0';
		}
	}

	Catalogue* Catalogue::open(const char* path)
	{
		if (path == nullptr)
		{
			return nullptr;
		}
		sqlite3* db = nullptr;
		if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
		{
			sqlite3_close(db); // safe on the handle open_v2 returns even on failure
			return nullptr;
		}
		// The `star` view's GROUP BY needs a temporary table; keep it in memory, since a
		// packaged app (notably on visionOS) has nowhere to write one by default.
		sqlite3_exec(db, "PRAGMA temp_store = MEMORY", nullptr, nullptr, nullptr);
		return new Catalogue(db);
	}

	Catalogue::~Catalogue()
	{
		sqlite3_close(db_);
	}

	int64_t Catalogue::star_count() const
	{
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db_, kCountStars, -1, &stmt, nullptr) != SQLITE_OK)
		{
			return -1;
		}
		int64_t count = -1;
		if (sqlite3_step(stmt) == SQLITE_ROW)
		{
			count = sqlite3_column_int64(stmt, 0);
		}
		sqlite3_finalize(stmt);
		return count;
	}

	int32_t Catalogue::load_stars(StcStar* out, int32_t capacity) const
	{
		if (out == nullptr || capacity < 0)
		{
			return -1;
		}
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db_, kSelectStars, -1, &stmt, nullptr) != SQLITE_OK)
		{
			return -1;
		}
		int32_t written = 0;
		while (written < capacity && sqlite3_step(stmt) == SQLITE_ROW)
		{
			StcStar& s = out[written];
			s.id = sqlite3_column_int64(stmt, 0);
			s.x = sqlite3_column_double(stmt, 1);
			s.y = sqlite3_column_double(stmt, 2);
			s.z = sqlite3_column_double(stmt, 3);
			s.absmag = sqlite3_column_double(stmt, 4);
			s.teff = sqlite3_column_double(stmt, 5);
			copy_text(s.name, STC_NAME_CAP, sqlite3_column_text(stmt, 6));
			copy_text(s.spectral_type, STC_SPECTRAL_CAP, sqlite3_column_text(stmt, 7));
			++written;
		}
		sqlite3_finalize(stmt);
		return written;
	}
}
