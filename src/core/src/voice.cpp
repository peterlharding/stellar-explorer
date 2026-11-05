#include "voice.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <utility>

namespace stellar::core
{
	namespace
	{
		struct Phrase
		{
			const char* words; // normalised
			VoiceAction action;
		};

		// The command phrases, normalised, longest first where one begins another. A
		// command naming no star stands alone; the others are followed by a star's name.
		constexpr std::array kPhrases{
			Phrase{"return to the sun", VoiceAction::ReturnToSun},
			Phrase{"go back to the sun", VoiceAction::ReturnToSun},
			Phrase{"back to the sun", VoiceAction::ReturnToSun},
			Phrase{"return home", VoiceAction::ReturnToSun},
			Phrase{"take me home", VoiceAction::ReturnToSun},
			Phrase{"go home", VoiceAction::ReturnToSun},
			Phrase{"let it go", VoiceAction::LetGo},
			Phrase{"let go", VoiceAction::LetGo},
			Phrase{"release", VoiceAction::LetGo},
			Phrase{"align to galactic north", VoiceAction::AlignGalactic},
			Phrase{"galactic north", VoiceAction::AlignGalactic},
			Phrase{"align to celestial north", VoiceAction::AlignCelestial},
			Phrase{"celestial north", VoiceAction::AlignCelestial},
			Phrase{"hide the info", VoiceAction::HideInfo},
			Phrase{"hide info", VoiceAction::HideInfo},
			Phrase{"hide the panel", VoiceAction::HideInfo},
			Phrase{"hide panel", VoiceAction::HideInfo},
			Phrase{"hide details", VoiceAction::HideInfo},
			Phrase{"info off", VoiceAction::HideInfo},
			Phrase{"show the info", VoiceAction::ShowInfo},
			Phrase{"show info", VoiceAction::ShowInfo},
			Phrase{"show the panel", VoiceAction::ShowInfo},
			Phrase{"show panel", VoiceAction::ShowInfo},
			Phrase{"show details", VoiceAction::ShowInfo},
			Phrase{"info on", VoiceAction::ShowInfo},
			// How far out stars are shown: before "show", which they begin.
			Phrase{"show more stars", VoiceAction::ShowMore},
			Phrase{"show more", VoiceAction::ShowMore},
			Phrase{"show further out", VoiceAction::ShowMore},
			Phrase{"further out", VoiceAction::ShowMore},
			Phrase{"see more", VoiceAction::ShowMore},
			Phrase{"show fewer stars", VoiceAction::ShowLess},
			Phrase{"show less stars", VoiceAction::ShowLess},
			Phrase{"show fewer", VoiceAction::ShowLess},
			Phrase{"show less", VoiceAction::ShowLess},
			Phrase{"see less", VoiceAction::ShowLess},
			Phrase{"show all the stars", VoiceAction::ShowEverything},
			Phrase{"show all stars", VoiceAction::ShowEverything},
			Phrase{"show every star", VoiceAction::ShowEverything},
			Phrase{"show everything", VoiceAction::ShowEverything},
			Phrase{"show all", VoiceAction::ShowEverything},
			Phrase{"no limit", VoiceAction::ShowEverything},
			Phrase{"show only stars within", VoiceAction::ShowWithin},
			Phrase{"show stars within", VoiceAction::ShowWithin},
			Phrase{"show only within", VoiceAction::ShowWithin},
			Phrase{"show within", VoiceAction::ShowWithin},
			Phrase{"only within", VoiceAction::ShowWithin},
			Phrase{"within", VoiceAction::ShowWithin},
			Phrase{"limit to", VoiceAction::ShowWithin},
			// After "back to the sun", which "go back to the sun" ends with.
			Phrase{"backtrack", VoiceAction::Backtrack},
			Phrase{"back track", VoiceAction::Backtrack},
			Phrase{"take me back", VoiceAction::Backtrack},
			Phrase{"go back", VoiceAction::Backtrack},
			Phrase{"undo", VoiceAction::Backtrack},
			Phrase{"quit the app", VoiceAction::Quit},
			Phrase{"exit the app", VoiceAction::Quit},
			Phrase{"close the app", VoiceAction::Quit},
			Phrase{"quit", VoiceAction::Quit},
			Phrase{"exit", VoiceAction::Quit},
			Phrase{"back off", VoiceAction::BackOff},
			Phrase{"back up", VoiceAction::BackOff},
			Phrase{"move back", VoiceAction::BackOff},
			Phrase{"step back", VoiceAction::BackOff},
			Phrase{"move away", VoiceAction::BackOff},
			Phrase{"move closer", VoiceAction::Closer},
			Phrase{"come closer", VoiceAction::Closer},
			Phrase{"go closer", VoiceAction::Closer},
			Phrase{"closer", VoiceAction::Closer},
			Phrase{"move in", VoiceAction::Closer},
			Phrase{"take me there", VoiceAction::GoThere},
			Phrase{"go there", VoiceAction::GoThere},
			Phrase{"fly there", VoiceAction::GoThere},
			Phrase{"go to it", VoiceAction::GoThere},
			Phrase{"take me to", VoiceAction::GoTo},
			Phrase{"go to", VoiceAction::GoTo},
			Phrase{"fly to", VoiceAction::GoTo},
			Phrase{"travel to", VoiceAction::GoTo},
			Phrase{"show me", VoiceAction::Show},
			Phrase{"show", VoiceAction::Show},
			Phrase{"find", VoiceAction::Show},
			Phrase{"where is", VoiceAction::Show},
			Phrase{"wheres", VoiceAction::Show},
			Phrase{"point to", VoiceAction::Show},
			Phrase{"point at", VoiceAction::Show},
			Phrase{"look at", VoiceAction::Show},
			Phrase{"select", VoiceAction::Show},
		};

		constexpr double kLightYearsPerParsec = 3.26156;

		bool all_digits(const std::string& word)
		{
			return !word.empty() && std::all_of(word.begin(), word.end(), [](char c) { return c >= '0' && c <= '9'; });
		}

		// A distance from words[at] on, as normalised: "20 light years", "6 5 parsecs" (6.5, its
		// point a space once normalised), "6 point 5 parsecs", "1 000 light years" (1,000), "2
		// hundred light years", in parsecs; light years if no unit was heard. 0 if no number.
		// Unit: 0 none heard, 1 light years, 2 parsecs.
		//
		// Apple's recogniser often mishears "parsecs": "6 part 6", "6 park", "50 Pacific", "100
		// Perec", and as a time, "6:06", "6:05", "5 6:00" (all in the headset). So after the number
		// a word beginning with "p" ("point" and "please" aside) is parsecs, and so is a second
		// number of two digits beginning with 0, as no one says "six point oh six"; a time anywhere
		// in the command is dealt with before (has_time).
		double parse_distance(const std::vector<std::string>& words, std::size_t at, int& unit)
		{
			unit = 0;
			bool as_time = false;
			double value = 0.0;
			bool any = false;
			std::size_t i = at;
			if (i < words.size() && (words[i] == "a" || words[i] == "about"))
			{
				++i;
			}
			for (; i < words.size(); ++i)
			{
				// "Two" written as it sounds, as Apple's recogniser did in the headset ("within to light years").
				const std::string& word = !any && (words[i] == "to" || words[i] == "too") ? std::string("2") : words[i];
				if (all_digits(word))
				{
					if (any && word.size() == 3)
					{
						value = value * 1000.0 + std::stod(word); // a thousands separator
					}
					else if (any && word.size() == 2 && word[0] == '0')
					{
						as_time = true; // "6:06": "parsecs" heard as a time
						++i;
						break;
					}
					else if (any)
					{
						value += std::stod(word) / std::pow(10.0, static_cast<double>(word.size())); // a decimal point
					}
					else
					{
						value = std::stod(word);
					}
					any = true;
				}
				else if (word == "point" && i + 1 < words.size() && all_digits(words[i + 1]))
				{
					value += std::stod(words[i + 1]) / std::pow(10.0, static_cast<double>(words[i + 1].size()));
					any = true;
					++i;
				}
				else if (word == "hundred" || word == "thousand")
				{
					value = (any ? value : 1.0) * (word == "hundred" ? 100.0 : 1000.0);
					any = true;
				}
				else
				{
					break;
				}
			}
			if (!any)
			{
				return 0.0;
			}
			const bool parsecs = as_time || (i < words.size() && words[i][0] == 'p' && words[i] != "point" && words[i] != "please");
			const bool light_years = i < words.size() &&
				(words[i] == "ly" || words[i] == "lightyear" || words[i] == "lightyears" ||
					(words[i] == "light" && i + 1 < words.size() && (words[i + 1] == "year" || words[i + 1] == "years")));
			unit = parsecs ? 2 : light_years ? 1 : 0;
			return parsecs ? value : value / kLightYearsPerParsec;
		}

		// Whether the text, as heard, holds a time ("6:00"): the recogniser's way of writing "parsecs".
		bool has_time(std::string_view text)
		{
			for (std::size_t i = 1; i + 1 < text.size(); ++i)
			{
				if (text[i] == ':' && std::isdigit(static_cast<unsigned char>(text[i - 1])) && std::isdigit(static_cast<unsigned char>(text[i + 1])))
				{
					return true;
				}
			}
			return false;
		}

		constexpr std::array<const char*, 20> kUnits{
			"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
			"eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
		constexpr std::array<const char*, 8> kTens{
			"twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};

		template <std::size_t N>
		int find_word(const std::array<const char*, N>& words, const std::string& word)
		{
			for (std::size_t i = 0; i < words.size(); ++i)
			{
				if (word == words[i])
				{
					return static_cast<int>(i);
				}
			}
			return -1;
		}

		std::vector<std::string> split(const std::string& text)
		{
			std::vector<std::string> words;
			std::size_t start = 0;
			while (start < text.size())
			{
				std::size_t end = text.find(' ', start);
				if (end == std::string::npos)
				{
					end = text.size();
				}
				if (end > start)
				{
					words.push_back(text.substr(start, end - start));
				}
				start = end + 1;
			}
			return words;
		}

		std::string join(const std::vector<std::string>& words, std::size_t from)
		{
			std::string text;
			for (std::size_t i = from; i < words.size(); ++i)
			{
				if (!text.empty())
				{
					text += ' ';
				}
				text += words[i];
			}
			return text;
		}

		// Whether `words` begins with the words of `phrase` at `at`; how many if so.
		std::size_t starts_with(const std::vector<std::string>& words, std::size_t at, const std::string& phrase)
		{
			const std::vector<std::string> wanted = split(phrase);
			if (wanted.empty() || at + wanted.size() > words.size())
			{
				return 0;
			}
			for (std::size_t i = 0; i < wanted.size(); ++i)
			{
				if (words[at + i] != wanted[i])
				{
					return 0;
				}
			}
			return wanted.size();
		}

		std::string without_spaces(const std::string& text)
		{
			std::string key;
			for (const char c : text)
			{
				if (c != ' ')
				{
					key += c;
				}
			}
			return key;
		}

		// The word's consonants, roughly as they sound: vowels, h, w and y dropped, c and q
		// as k, z as s, v as f, doubles collapsed. Loose, more goes: r (an Australian "Sadr"
		// is "satter") and the difference between voiced and unvoiced (d t, b p, g k).
		// Strictly "peronis" is p-r-n-s, nearer "pavonis" (p-f-n-s) than "leonis" (l-n-s);
		// loosely "satter" and "sadr" are both s-t.
		std::string sound(const std::string& key, bool loose)
		{
			std::string out;
			for (const char c : key)
			{
				char k = c;
				switch (c)
				{
				case 'a': case 'e': case 'i': case 'o': case 'u': case 'h': case 'w': case 'y':
					continue;
				case 'c': case 'q':
					k = 'k';
					break;
				case 'z':
					k = 's';
					break;
				case 'v':
					k = 'f';
					break;
				default:
					break;
				}
				if (loose)
				{
					switch (k)
					{
					case 'r':
						continue;
					case 'd':
						k = 't';
						break;
					case 'b':
						k = 'p';
						break;
					case 'g':
						k = 'k';
						break;
					default:
						break;
					}
				}
				if (out.empty() || out.back() != k)
				{
					out += k;
				}
			}
			return out;
		}

		// Edits (insertions, deletions, substitutions) to turn a into b, or limit + 1 if
		// more than limit.
		std::size_t edit_distance(const std::string& a, const std::string& b, std::size_t limit)
		{
			const std::size_t gap = a.size() > b.size() ? a.size() - b.size() : b.size() - a.size();
			if (gap > limit)
			{
				return limit + 1;
			}
			std::vector<std::size_t> row(b.size() + 1);
			for (std::size_t j = 0; j <= b.size(); ++j)
			{
				row[j] = j;
			}
			for (std::size_t i = 1; i <= a.size(); ++i)
			{
				std::size_t diagonal = row[0];
				row[0] = i;
				std::size_t best = row[0];
				for (std::size_t j = 1; j <= b.size(); ++j)
				{
					const std::size_t above = row[j];
					row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
					diagonal = above;
					best = std::min(best, row[j]);
				}
				if (best > limit)
				{
					return limit + 1;
				}
			}
			return row[b.size()];
		}
	}

	std::string normalise(std::string_view text)
	{
		// Letters and digits kept, lower-cased; apostrophes dropped ("Barnard's" ->
		// "barnards"); anything else, including any byte outside ASCII, separates words.
		std::string cleaned;
		for (const char c : text)
		{
			const auto u = static_cast<unsigned char>(c);
			if (u < 128 && std::isalnum(u))
			{
				cleaned += static_cast<char>(std::tolower(u));
			}
			else if (c != '\'')
			{
				cleaned += ' ';
			}
		}

		// Numbers said as words, written as digits: "sixty one" -> "61", "nine" -> "9".
		const std::vector<std::string> words = split(cleaned);
		std::vector<std::string> out;
		for (std::size_t i = 0; i < words.size(); ++i)
		{
			const int ten = find_word(kTens, words[i]);
			if (ten >= 0)
			{
				const int unit = i + 1 < words.size() ? find_word(kUnits, words[i + 1]) : -1;
				if (unit >= 1 && unit <= 9)
				{
					out.push_back(std::to_string((ten + 2) * 10 + unit));
					++i;
				}
				else
				{
					out.push_back(std::to_string((ten + 2) * 10));
				}
				continue;
			}
			const int unit = find_word(kUnits, words[i]);
			out.push_back(unit >= 0 ? std::to_string(unit) : words[i]);
		}
		return join(out, 0);
	}

	Voice::Voice(std::vector<SpokenName> names)
	{
		std::sort(names.begin(), names.end(), [](const SpokenName& a, const SpokenName& b) {
			return a.apparent_magnitude < b.apparent_magnitude;
		});
		for (const SpokenName& n : names)
		{
			const std::string key = without_spaces(normalise(n.name));
			if (key.empty())
			{
				continue;
			}
			// Brightest first, so the first star seen with a name keeps it.
			by_key_.emplace(key, Entry{n.name, n.star_id, n.apparent_magnitude});
			if (key == "sun" && sun_id_ < 0)
			{
				sun_id_ = n.star_id;
			}
		}

		// The phrases: the stars brightest and nearest in turn, each by the names people use
		// for it, proper names and plain Bayer names ("Sirius", "Alpha Canis Majoris"), not
		// "9 Canis Majoris" or "Alpha 1 Centauri" unless it has nothing else. A recogniser is
		// best told only a hundred or two, and Delta Pavonis (6 pc away, magnitude 3.6) is
		// among the nearest named stars but far from the brightest: not told of it, Apple's
		// recogniser heard "Peronis".
		const auto has_digit = [](const std::string& name) {
			return std::any_of(name.begin(), name.end(), [](char c) { return c >= '0' && c <= '9'; });
		};
		std::unordered_map<int64_t, std::vector<std::string>> by_star;
		std::unordered_map<int64_t, std::vector<std::string>> numbered;
		std::vector<int64_t> brightest_stars;
		for (const SpokenName& n : names) // brightest first, from the sort above
		{
			const auto owner = by_key_.find(without_spaces(normalise(n.name)));
			if (owner == by_key_.end() || owner->second.star_id != n.star_id)
			{
				continue; // "Alpha Centauri" is listed for the brighter of the two
			}
			if (by_star.count(n.star_id) == 0 && numbered.count(n.star_id) == 0)
			{
				brightest_stars.push_back(n.star_id);
			}
			// Proper names are kept even with digits ("Wolf 359"); designations only without.
			(n.proper || !has_digit(n.name) ? by_star : numbered)[n.star_id].push_back(n.name);
		}
		for (const int64_t id : brightest_stars)
		{
			if (by_star[id].empty())
			{
				by_star[id].push_back(numbered[id].front());
			}
		}
		std::vector<int64_t> nearest_stars = brightest_stars;
		std::unordered_map<int64_t, double> distance;
		for (const SpokenName& n : names)
		{
			distance[n.star_id] = n.distance_parsecs;
		}
		std::stable_sort(nearest_stars.begin(), nearest_stars.end(), [&](int64_t a, int64_t b) { return distance[a] < distance[b]; });
		std::unordered_map<int64_t, bool> listed;
		for (std::size_t i = 0; i < brightest_stars.size(); ++i)
		{
			for (const int64_t id : {brightest_stars[i], nearest_stars[i]})
			{
				if (listed.emplace(id, true).second)
				{
					const std::vector<std::string>& star_names = by_star[id];
					phrases_.insert(phrases_.end(), star_names.begin(), star_names.end());
				}
			}
		}
	}

	const Voice::Entry* Voice::match(const std::string& heard, bool bSoundsAlike) const
	{
		const std::string key = without_spaces(heard);
		if (key.empty())
		{
			return nullptr;
		}
		if (const auto exact = by_key_.find(key); exact != by_key_.end())
		{
			return &exact->second;
		}
		// One edit for every four letters after the first: enough for "pavoness" to be
		// "pavonis", none for a word of four letters or fewer ("vela" is not "Vega", "bed"
		// is not "Beid"). Failing that, from five letters, a name that sounds alike: the
		// same consonants, or one apart once there are five ("battle girls", "ptlkls", for
		// "Betelgeuse", "ptlks"), within two edits in three of the letters.
		const std::size_t near = (key.size() - 1) / 4;
		const std::size_t loose = bSoundsAlike && key.size() >= 5 ? key.size() * 2 / 3 : near;
		const std::string heard_sound = sound(key, true);
		const std::string heard_strict = sound(key, false);
		// One consonant's slack from five up: at four, "pleiades" (p-l-t-s) became Paradys (p-t-s).
		const std::size_t sound_slack = heard_sound.size() >= 5 ? 1 : 0;
		const Entry* best = nullptr;
		std::size_t best_rank = 0;
		std::size_t best_sound = 0;
		for (const auto& [name_key, entry] : by_key_)
		{
			const std::size_t distance = edit_distance(key, name_key, loose);
			if (distance > loose)
			{
				continue;
			}
			// Near matches rank before sound-alikes, then fewer edits first.
			std::size_t rank = distance;
			if (distance > near)
			{
				const std::string name_sound = sound(name_key, true);
				if (edit_distance(heard_sound, name_sound, sound_slack) > sound_slack)
				{
					continue;
				}
				rank += loose + 1;
			}
			const std::string name_strict = sound(name_key, false);
			const std::size_t sound_distance = edit_distance(heard_strict, name_strict, heard_strict.size() + name_strict.size());
			// Of names equally many edits away, the one whose consonants are closest: "peronis"
			// (p-n-s) is "pavonis" (p-f-n-s), not "leonis" (l-n-s), though Leonis is brighter.
			if (best == nullptr || rank < best_rank || (rank == best_rank && sound_distance < best_sound) ||
				(rank == best_rank && sound_distance == best_sound && entry.apparent_magnitude < best->apparent_magnitude))
			{
				best = &entry;
				best_rank = rank;
				best_sound = sound_distance;
			}
		}
		return best;
	}

	VoiceCommand Voice::parse(std::string_view text, std::string_view wake_word) const
	{
		VoiceCommand command;
		std::vector<std::string> words = split(normalise(text));

		// The command follows the wake word's last use, so that in a running transcript
		// only the latest command counts.
		const std::string wake = normalise(wake_word);
		if (!wake.empty())
		{
			// A one-word wake word is heard with a letter's slack: Apple's recogniser writes
			// "Stellar" as "Stella" as often as not.
			const bool one_word = wake.find(' ') == std::string::npos;
			for (std::size_t i = words.size(); i-- > 0;)
			{
				std::size_t n = starts_with(words, i, wake);
				if (n == 0 && one_word && wake.size() >= 5 && edit_distance(words[i], wake, 1) <= 1)
				{
					n = 1;
				}
				// Further off, one or two words that sound like it ("still a", "still I" for
				// "Stellar", all heard in the headset) count only with a command straight after.
				for (std::size_t span = 1; n == 0 && one_word && span <= 2 && i + span < words.size(); ++span)
				{
					std::string merged = words[i];
					if (span == 2)
					{
						merged += words[i + 1];
					}
					const bool sounds_like = merged.size() >= 4 && sound(merged, true) == sound(wake, true);
					const bool command_next = std::any_of(kPhrases.begin(), kPhrases.end(), [&](const Phrase& phrase) {
						return starts_with(words, i + span, phrase.words) > 0;
					});
					if (sounds_like && command_next)
					{
						n = span;
					}
				}
				if (n > 0)
				{
					command.woken = true;
					words.erase(words.begin(), words.begin() + static_cast<std::ptrdiff_t>(i + n));
					break;
				}
			}
		}

		std::size_t at = 0;
		if (starts_with(words, at, "please") > 0)
		{
			++at;
		}
		// The last command said counts: people restart mid-sentence ("go to Siri go to
		// Sirius", heard in the headset), and only what follows the last start is meant.
		command.action = VoiceAction::Show; // a star's name alone shows it
		bool commanded = false;
		for (std::size_t i = words.size(); i-- > at && !commanded;)
		{
			for (const Phrase& phrase : kPhrases)
			{
				if (const std::size_t n = starts_with(words, i, phrase.words); n > 0)
				{
					command.action = phrase.action;
					commanded = true;
					at = i + n;
					break;
				}
			}
		}
		if (command.action == VoiceAction::LetGo || command.action == VoiceAction::GoThere || command.action == VoiceAction::BackOff ||
			command.action == VoiceAction::Closer || command.action == VoiceAction::AlignGalactic || command.action == VoiceAction::AlignCelestial ||
			command.action == VoiceAction::HideInfo || command.action == VoiceAction::ShowInfo || command.action == VoiceAction::ShowMore ||
			command.action == VoiceAction::ShowLess || command.action == VoiceAction::ShowEverything || command.action == VoiceAction::Quit ||
			command.action == VoiceAction::Backtrack)
		{
			return command;
		}
		if (command.action == VoiceAction::ShowWithin)
		{
			command.radius_parsecs = parse_distance(words, at, command.radius_unit);
			// A time heard is "parsecs" misheard, and the number is the first said: "five 6:00" is 5 pc.
			if (has_time(text))
			{
				const auto first = std::find_if(words.begin() + static_cast<std::ptrdiff_t>(std::min(at, words.size())), words.end(), all_digits);
				if (first != words.end())
				{
					command.radius_parsecs = std::stod(*first);
					command.radius_unit = 2;
				}
			}
			return command;
		}
		if (command.action == VoiceAction::ReturnToSun)
		{
			command.star_id = sun_id_;
			command.name = "Sun";
			return command;
		}

		if (starts_with(words, at, "the") > 0)
		{
			++at;
		}
		if (!words.empty() && words.back() == "please")
		{
			words.pop_back();
		}
		// "Show 20 light years", heard without "within": a distance, with its unit, is no star's name.
		if (command.action == VoiceAction::Show)
		{
			int unit = 0;
			const double radius = parse_distance(words, at, unit);
			if (radius > 0.0 && unit != 0)
			{
				command.action = VoiceAction::ShowWithin;
				command.radius_parsecs = radius;
				command.radius_unit = unit;
				return command;
			}
		}
		command.target = join(words, at);
		if (const Entry* entry = match(command.target, commanded))
		{
			command.star_id = entry->star_id;
			command.name = entry->name;
		}
		else if (!commanded)
		{
			// Words that are neither a command nor a star's name: not a command at all.
			command.action = VoiceAction::None;
		}
		return command;
	}
}
