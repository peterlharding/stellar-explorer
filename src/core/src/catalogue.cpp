#include "catalogue.hpp"

#include "sqlite3.h"

#include <cmath>

namespace stellar::core
{
	namespace
	{
		// Stars usable by the renderer: a position and an absolute magnitude (the Sun is
		// at the origin), in id order; teff defaults to 5800 K and spectral_type to empty
		// when absent. These read the tables directly rather than the pipeline's `star`
		// view: the view pivots every measurement of every star through a GROUP BY, which
		// took about 2.4 s for the 113,000 stars on a Mac (twice as long on a Vision Pro),
		// against about 0.6 s here. The pipeline guarantees exactly one preferred value per
		// quantity of each object (DATA-MODEL.md, "Preferred values"), so a join on the
		// preferred row gives what the view's max() does. The two queries must agree on
		// which stars count: an inner join on x and absmag, the rest left joins.
		constexpr const char* kSelectStars =
			"SELECT o.id, x.value, y.value, z.value, a.value, coalesce(t.value, 5800), o.name, coalesce(p.value, '') "
			"FROM object AS o "
			"JOIN measurement AS x ON x.object_id = o.id AND x.quantity = 'x' AND x.preferred "
			"JOIN measurement AS a ON a.object_id = o.id AND a.quantity = 'absmag' AND a.preferred "
			"LEFT JOIN measurement AS y ON y.object_id = o.id AND y.quantity = 'y' AND y.preferred "
			"LEFT JOIN measurement AS z ON z.object_id = o.id AND z.quantity = 'z' AND z.preferred "
			"LEFT JOIN measurement AS t ON t.object_id = o.id AND t.quantity = 'teff' AND t.preferred "
			"LEFT JOIN property AS p ON p.object_id = o.id AND p.name = 'spectral_type' AND p.preferred "
			"WHERE o.kind = 'star' "
			"ORDER BY o.id";
		constexpr const char* kCountStars =
			"SELECT count(*) "
			"FROM object AS o "
			"JOIN measurement AS x ON x.object_id = o.id AND x.quantity = 'x' AND x.preferred "
			"JOIN measurement AS a ON a.object_id = o.id AND a.quantity = 'absmag' AND a.preferred "
			"WHERE o.kind = 'star'";

		// Proper names and spoken forms (the pipeline's `name` and `spoken` identifiers) of
		// the stars kSelectStars returns, with what the magnitude needs. Joined the same way.
		constexpr const char* kSelectSpokenNames =
			"SELECT i.value, o.id, x.value, y.value, z.value, a.value, i.catalogue = 'name' "
			"FROM identifier AS i "
			"JOIN object AS o ON o.id = i.object_id AND o.kind = 'star' "
			"JOIN measurement AS x ON x.object_id = o.id AND x.quantity = 'x' AND x.preferred "
			"JOIN measurement AS a ON a.object_id = o.id AND a.quantity = 'absmag' AND a.preferred "
			"LEFT JOIN measurement AS y ON y.object_id = o.id AND y.quantity = 'y' AND y.preferred "
			"LEFT JOIN measurement AS z ON z.object_id = o.id AND z.quantity = 'z' AND z.preferred "
			"WHERE i.catalogue IN ('name', 'spoken')";

		// The Sun's apparent magnitude, for the star at the origin.
		constexpr double kSunApparentMagnitude = -26.74;

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

	namespace
	{
		sqlite3* open_read_only(const char* path)
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
			return db;
		}

		// PRAGMA user_version, where the pipeline writes its SCHEMA_VERSION; negative when
		// the file is not a database (SQLite reads nothing until the first statement).
		int32_t user_version(sqlite3* db)
		{
			sqlite3_stmt* stmt = nullptr;
			int32_t version = -1;
			if (sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &stmt, nullptr) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW)
			{
				version = sqlite3_column_int(stmt, 0);
			}
			sqlite3_finalize(stmt);
			return version;
		}
	}

	int32_t Catalogue::schema(const char* path)
	{
		sqlite3* db = open_read_only(path);
		if (db == nullptr)
		{
			return -1;
		}
		const int32_t version = user_version(db);
		sqlite3_close(db);
		return version;
	}

	Catalogue* Catalogue::open(const char* path)
	{
		sqlite3* db = open_read_only(path);
		if (db == nullptr)
		{
			return nullptr;
		}
		// A catalogue from another pipeline's schema would be misread, not just fail.
		if (user_version(db) != STC_SCHEMA_VERSION)
		{
			sqlite3_close(db);
			return nullptr;
		}
		// Keep any temporary table a query needs (a sort, a GROUP BY) in memory, since a
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

	std::vector<SpokenName> Catalogue::spoken_names() const
	{
		std::vector<SpokenName> names;
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db_, kSelectSpokenNames, -1, &stmt, nullptr) != SQLITE_OK)
		{
			return names;
		}
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			const unsigned char* text = sqlite3_column_text(stmt, 0);
			const double x = sqlite3_column_double(stmt, 2);
			const double y = sqlite3_column_double(stmt, 3);
			const double z = sqlite3_column_double(stmt, 4);
			const double absmag = sqlite3_column_double(stmt, 5);
			const double distance = std::sqrt(x * x + y * y + z * z);
			names.push_back(SpokenName{
				text != nullptr ? reinterpret_cast<const char*>(text) : "",
				sqlite3_column_int64(stmt, 1),
				distance > 0.0 ? absmag + 5.0 * std::log10(distance) - 5.0 : kSunApparentMagnitude,
				distance,
				sqlite3_column_int(stmt, 6) != 0});
		}
		sqlite3_finalize(stmt);
		return names;
	}
}
