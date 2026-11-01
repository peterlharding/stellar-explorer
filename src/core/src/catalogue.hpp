#pragma once

#include "stellar_core.h" // StcStar

#include <cstdint>

struct sqlite3; // the vendored SQLite, forward-declared to keep it out of this header

namespace stellar::core
{
	// The star catalogue (one SQLite file, built offline by the pipeline), opened
	// read-only. RAII: the destructor closes it. ADR 0008 keeps this in the core, not
	// the engine; the C ABI in stellar_core.h wraps it behind an opaque handle.
	class Catalogue
	{
	public:
		// Open read-only; returns nullptr if the file cannot be opened.
		static Catalogue* open(const char* path);
		~Catalogue();

		Catalogue(const Catalogue&) = delete;
		Catalogue& operator=(const Catalogue&) = delete;

		// Stars with both a position and an absolute magnitude; negative on error.
		int64_t star_count() const;
		// Fill up to `capacity` rows into `out`; returns the number written, negative on error.
		int32_t load_stars(StcStar* out, int32_t capacity) const;

	private:
		explicit Catalogue(sqlite3* db) : db_(db) {}
		sqlite3* db_;
	};
}
