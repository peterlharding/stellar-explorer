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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STELLAR_CORE_H */
