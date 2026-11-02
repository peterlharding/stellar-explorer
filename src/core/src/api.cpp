// The C ABI (stellar_core.h) implemented over the internal C++ (astrometry.hpp).
// This translation unit is the only place the two meet; nothing C++ escapes it.

#include "stellar_core.h"

#include "astrometry.hpp"
#include "catalogue.hpp"
#include "voice.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

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
	const stellar::core::Voice* as_core(const StcVoice* v)
	{
		return reinterpret_cast<const stellar::core::Voice*>(v);
	}

	// Copy into a fixed field: truncated to fit, always NUL-terminated.
	template <std::size_t N>
	void copy_field(char (&dst)[N], const std::string& src)
	{
		const std::size_t n = std::min(src.size(), N - 1);
		std::memcpy(dst, src.data(), n);
		dst[n] = '\0';
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

	StcVoice* stc_voice_open(const StcCatalogue* catalogue)
	{
		if (catalogue == nullptr)
		{
			return nullptr;
		}
		std::vector<stellar::core::SpokenName> names = as_core(catalogue)->spoken_names();
		if (names.empty())
		{
			return nullptr;
		}
		return reinterpret_cast<StcVoice*>(new stellar::core::Voice(std::move(names)));
	}

	void stc_voice_close(StcVoice* voice)
	{
		delete reinterpret_cast<stellar::core::Voice*>(voice);
	}

	int32_t stc_voice_parse(const StcVoice* voice, const char* text, const char* wake_word, StcVoiceCommand* out)
	{
		if (voice == nullptr || text == nullptr || out == nullptr)
		{
			return -1;
		}
		const stellar::core::VoiceCommand command = as_core(voice)->parse(text, wake_word != nullptr ? wake_word : "");
		out->action = static_cast<int32_t>(command.action);
		out->woken = command.woken ? 1 : 0;
		out->star_id = command.star_id;
		copy_field(out->name, command.name);
		copy_field(out->target, command.target);
		return out->action;
	}

	int32_t stc_voice_phrase_count(const StcVoice* voice)
	{
		return voice == nullptr ? -1 : static_cast<int32_t>(as_core(voice)->phrases().size());
	}

	const char* stc_voice_phrase(const StcVoice* voice, int32_t index)
	{
		if (voice == nullptr || index < 0 || static_cast<std::size_t>(index) >= as_core(voice)->phrases().size())
		{
			return nullptr;
		}
		return as_core(voice)->phrases()[static_cast<std::size_t>(index)].c_str();
	}
}
