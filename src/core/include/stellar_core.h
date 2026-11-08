/*
 * Stellar Explorer core - public C ABI.
 *
 * The engine-independent core (ADR 0008) is a static library with a C ABI, which
 * the Unreal front end links through a thin C++ wrapper (ADR 0014). This header is
 * the whole boundary: only C - plain functions, POD structs by value - crosses it.
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
#define STC_ABI_VERSION 7u
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

/* Capacities (including the NUL terminator) of the fixed string fields in StcStar.
 * Values longer than this from the catalogue are truncated. Names fit comfortably;
 * spectral types are short (e.g. "M5.5Ve"). Fixed arrays keep StcStar plain data in
 * the caller's buffer - no per-string allocation or lifetime to manage. */
#define STC_NAME_CAP 64
#define STC_SPECTRAL_CAP 24

/* One star: identity, position (parsecs, right-handed ICRS), photometry, and the
 * display strings. All by value in the caller's buffer. */
typedef struct StcStar
{
	int64_t id;
	double x;
	double y;
	double z;
	double absmag;                        /* absolute visual magnitude */
	double teff;                          /* effective temperature, kelvin (5800 when the catalogue has none) */
	char name[STC_NAME_CAP];              /* NUL-terminated; truncated if longer */
	char spectral_type[STC_SPECTRAL_CAP]; /* NUL-terminated; empty when the catalogue has none */
} StcStar;

/* The catalogue schema this core reads: the pipeline's SCHEMA_VERSION, which it stores in
 * each catalogue as SQLite's PRAGMA user_version. Changes only when the tables do. */
#define STC_SCHEMA_VERSION 1

/* Open the catalogue at `path` read-only; returns NULL if it cannot be opened, or if its
 * schema is not STC_SCHEMA_VERSION (a catalogue built by an older or newer pipeline). */
StcCatalogue* stc_catalogue_open(const char* path);

/* The schema version of the catalogue at `path`, to say why stc_catalogue_open refused it;
 * negative if the file cannot be read as a catalogue at all. */
int32_t stc_catalogue_schema(const char* path);

/* Close a catalogue opened by stc_catalogue_open. NULL is ignored. */
void stc_catalogue_close(StcCatalogue* catalogue);

/* How many stars have both a position and an absolute magnitude (the ones
 * stc_catalogue_load_stars returns); negative on error. */
int64_t stc_catalogue_star_count(const StcCatalogue* catalogue);

/* Fill up to `capacity` stars into `out`, returning the number written, or a
 * negative value on error. Call stc_catalogue_star_count first to size `out`. */
int32_t stc_catalogue_load_stars(const StcCatalogue* catalogue, StcStar* out, int32_t capacity);

/* ---- Voice ----------------------------------------------------------------
 *
 * Turns what a speech recogniser heard into a command and a star. The recogniser
 * itself is the platform's (the app's job); the words and the names are matched
 * here, the same on every platform. Names are the catalogue's proper names and
 * spoken forms of its designations ("Delta Pavonis" for "del Pav"), matched with
 * some tolerance for mishearing.
 */

/* An opaque handle to the names of an open catalogue, ready for matching. */
typedef struct StcVoice StcVoice;

/* What a command asks for. */
typedef enum StcVoiceAction
{
	STC_VOICE_NONE = 0,          /* not a command */
	STC_VOICE_SHOW = 1,          /* "show Sirius", or a star's name alone */
	STC_VOICE_GO_TO = 2,         /* "go to Delta Pavonis" */
	STC_VOICE_RETURN_TO_SUN = 3, /* "return to the Sun" */
	STC_VOICE_LET_GO = 4,        /* "let go" of the star kept */
	STC_VOICE_GO_THERE = 5,      /* "go there": to the star kept or pointed at, no name needed */
	STC_VOICE_BACK_OFF = 6,      /* "back off": further from the star kept or pointed at */
	STC_VOICE_CLOSER = 7,        /* "closer": nearer the star kept or pointed at */
	STC_VOICE_ALIGN_GALACTIC = 8, /* "galactic north": the galactic north pole up on the screen */
	STC_VOICE_ALIGN_CELESTIAL = 9, /* "celestial north": the celestial north pole up, as at start */
	STC_VOICE_HIDE_INFO = 10,     /* "hide info": the panel describing the star, hidden until shown */
	STC_VOICE_SHOW_INFO = 11,     /* "show info": the panel shown again */
	STC_VOICE_SHOW_MORE = 12,     /* "show more": stars twice as far out shown */
	STC_VOICE_SHOW_LESS = 13,     /* "show less": only stars half as far out shown */
	STC_VOICE_SHOW_WITHIN = 14,   /* "show within 20 light years" (or parsecs): radius_parsecs set, 0 if no distance was heard */
	STC_VOICE_SHOW_EVERYTHING = 15, /* "show everything": no limit to how far out stars are shown */
	STC_VOICE_QUIT = 16,           /* "quit" or "exit": the app, which asks for it twice */
	STC_VOICE_BACKTRACK = 17       /* "backtrack" or "go back": to where the last journey began */
} StcVoiceAction;

/* Capacity (including the NUL) of StcVoiceCommand's target: what was heard. */
#define STC_VOICE_TARGET_CAP 96

typedef struct StcVoiceCommand
{
	int32_t action;                     /* an StcVoiceAction */
	int32_t woken;                      /* 1 if the text held the wake word; the command follows its last use */
	int64_t star_id;                    /* the star named, as StcStar.id; -1 if none was named or none matched */
	char name[STC_NAME_CAP];            /* the name matched, as the catalogue writes it; empty if none */
	char target[STC_VOICE_TARGET_CAP];  /* the words heard where a star's name belongs, normalised; may be empty */
	double radius_parsecs;              /* for STC_VOICE_SHOW_WITHIN, the distance heard, in parsecs; 0 if none */
	int32_t radius_unit;                /* its unit as heard: 0 none (light years taken), 1 light years, 2 parsecs */
} StcVoiceCommand;

/* Read the names from an open catalogue; NULL on error. The catalogue may be closed after. */
StcVoice* stc_voice_open(const StcCatalogue* catalogue);

/* Free a StcVoice. NULL is ignored. */
void stc_voice_close(StcVoice* voice);

/* Parse `text` (UTF-8) into `out`, returning its action, or a negative value on error.
 * With a non-empty `wake_word` ("Stellar"), the command is what follows the wake
 * word's last use, and `out->woken` says whether it was heard. */
int32_t stc_voice_parse(const StcVoice* voice, const char* text, const char* wake_word, StcVoiceCommand* out);

/* The names, once each, brightest star first: vocabulary a recogniser can be given.
 * The strings belong to `voice` and last as long as it does. */
int32_t stc_voice_phrase_count(const StcVoice* voice);
const char* stc_voice_phrase(const StcVoice* voice, int32_t index);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STELLAR_CORE_H */
