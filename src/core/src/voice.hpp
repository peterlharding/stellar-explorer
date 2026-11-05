#pragma once

#include "catalogue.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace stellar::core
{
	// What a spoken command asks for. The numbers are StcVoiceAction's in stellar_core.h.
	enum class VoiceAction : int32_t
	{
		None = 0,
		Show = 1,
		GoTo = 2,
		ReturnToSun = 3,
		LetGo = 4,
		GoThere = 5,
		BackOff = 6,
		Closer = 7,
		AlignGalactic = 8,
		AlignCelestial = 9,
		HideInfo = 10,
		ShowInfo = 11,
	};

	struct VoiceCommand
	{
		VoiceAction action = VoiceAction::None;
		// Whether the text held the wake word; the command is what follows its last use.
		bool woken = false;
		// The star named, or -1 when none was named or none matched.
		int64_t star_id = -1;
		// The matched name as the catalogue writes it ("Delta Pavonis"), or empty.
		std::string name;
		// What was heard where a star's name should be, normalised; empty if nothing.
		std::string target;
	};

	// Lower case, letters and digits only, words separated by single spaces, numbers
	// spoken as words written as digits: "Barnard's Star" -> "barnards star",
	// "Sixty-one Cygni" -> "61 cygni".
	std::string normalise(std::string_view text);

	// Turns what a recogniser heard into a command and a star: the fixed command
	// phrases ("show", "go to", "return to the Sun", "let go") and the catalogue's star
	// names, matched with some tolerance for mishearing ("Delta Pavo Ness").
	class Voice
	{
	public:
		explicit Voice(std::vector<SpokenName> names);

		// Parse `text`. With a non-empty `wake_word`, the command is what follows the
		// wake word's last use, and `woken` says whether it was there at all.
		VoiceCommand parse(std::string_view text, std::string_view wake_word) const;

		// Every name, once each, the brightest and the nearest stars taken in turn: vocabulary
		// for a recogniser, which is best told only the first hundred or two.
		const std::vector<std::string>& phrases() const { return phrases_; }

	private:
		struct Entry
		{
			std::string name;
			int64_t star_id;
			double apparent_magnitude;
		};

		// The star a heard name means, or nullptr: exactly, else the closest name within
		// a few edits; of names equally close, the one that sounds closest, then the
		// brighter star.
		// Without bSoundsAlike, only exact and near matches: for a name said with no command
		// before it, where a sound-alike turned "hello there" into Alioth.
		const Entry* match(const std::string& heard, bool bSoundsAlike) const;

		// By the name's normalised form with its spaces taken out, since a recogniser
		// splits and joins words freely ("Pavo Ness" for "Pavonis"); the brightest star
		// of each name only.
		std::unordered_map<std::string, Entry> by_key_;
		std::vector<std::string> phrases_;
		int64_t sun_id_ = -1;
	};
}
