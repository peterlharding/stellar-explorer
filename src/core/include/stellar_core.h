/*
 * Stellar Explorer core — public C ABI.
 *
 * The engine-independent core (ADR 0008) is a static library with a C ABI, which
 * the Unreal front end links through a thin C++ wrapper (ADR 0014). This header is
 * the whole boundary: only C — plain functions, POD structs by value — crosses it.
 * No C++, STL, or exceptions. Memory the core allocates, the core frees.
 *
 * Positions are in parsecs, in the right-handed ICRS equatorial frame (x toward
 * RA 0h / Dec 0, z toward the north celestial pole). Converting to a left-handed
 * engine frame (the y flip) and rebasing camera-relative is the caller's job.
 */
#ifndef STELLAR_CORE_H
#define STELLAR_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ABI version of this library; a caller checks it matches this header. */
#define STC_ABI_VERSION 1u
uint32_t stc_abi_version(void);

/* A position in parsecs, right-handed ICRS equatorial Cartesian. POD, by value. */
typedef struct StcVec3
{
	double x;
	double y;
	double z;
} StcVec3;

/* Distance in parsecs from a parallax in milliarcseconds; NaN if parallax <= 0. */
double stc_distance_parsecs(double parallax_mas);

/* ICRS Cartesian position (parsecs) from right ascension and declination
 * (degrees) and distance (parsecs). */
StcVec3 stc_icrs_position(double ra_deg, double dec_deg, double distance_pc);

/* ---- Catalogue ------------------------------------------------------------
 *
 * The star catalogue (one SQLite file, built offline by the pipeline), opened
 * read-only. The handle is opaque: only pointers to it cross the boundary, and
 * the core owns its lifetime (open / close). Star rows come back as POD, by
 * value in a caller-provided buffer; the core allocates nothing the caller frees.
 */

/* An opaque handle to an open catalogue. */
typedef struct StcCatalogue StcCatalogue;

/* One star: identity, position (parsecs, right-handed ICRS), and photometry.
 * Names and spectral types (strings) are not yet carried across the boundary. */
typedef struct StcStar
{
	int64_t id;
	double x;
	double y;
	double z;
	double absmag; /* absolute visual magnitude */
	double teff;   /* effective temperature, kelvin (5800 when the catalogue has none) */
} StcStar;

/* Open the catalogue at `path` read-only; returns NULL if it cannot be opened. */
StcCatalogue* stc_catalogue_open(const char* path);

/* Close a catalogue opened by stc_catalogue_open. NULL is ignored. */
void stc_catalogue_close(StcCatalogue* catalogue);

/* How many stars have both a position and an absolute magnitude (the ones
 * stc_catalogue_load_stars returns); negative on error. */
int64_t stc_catalogue_star_count(const StcCatalogue* catalogue);

/* Fill up to `capacity` stars into `out`, returning the number written, or a
 * negative value on error. Call stc_catalogue_star_count first to size `out`. */
int32_t stc_catalogue_load_stars(const StcCatalogue* catalogue, StcStar* out, int32_t capacity);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STELLAR_CORE_H */
