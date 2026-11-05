// Voice command tests, run without the engine by CTest and `make check`. The names
// stand in for the catalogue's: a few proper names and spoken designations, with two
// stars sharing "Alpha Centauri" to check the brighter wins.

#include "voice.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace
{
	int g_failures = 0;

	void check(bool ok, const std::string& what)
	{
		if (!ok)
		{
			std::printf("FAIL: %s\n", what.c_str());
			++g_failures;
		}
	}

	using stellar::core::VoiceAction;

	stellar::core::Voice make_voice()
	{
		return stellar::core::Voice({
			{"Sun", 1, -26.74, 0.0, true},
			{"Sirius", 32264, -1.44, 2.64, true},
			{"Vega", 91000, 0.03, 7.68, true},
			{"Toliman", 71453, 1.33, 1.34, true},
			{"Alpha Centauri", 71453, 1.33, 1.34, false},
			{"Rigil Kentaurus", 71456, -0.01, 1.34, true},
			{"Alpha Centauri", 71456, -0.01, 1.34, false},
			{"Delta Pavonis", 98924, 3.56, 6.1, false},
			{"Delta Leonis", 54712, 2.56, 17.9, false},
			{"61 Cygni", 103879, 5.2, 3.5, false},
			{"Barnard's Star", 87665, 9.5, 1.83, true},
			{"Zeta Velorum", 40000, 3.0, 300.0, false},
			{"Beid", 19539, 4.0, 63.0, true},
			{"Betelgeuse", 27920, 0.42, 152.0, true},
			{"Sadr", 100240, 2.23, 560.0, true},
			{"Paradys", 60000, 8.0, 200.0, true},
		});
	}

	void expect(const stellar::core::Voice& voice, const char* heard, VoiceAction action, int64_t star)
	{
		const stellar::core::VoiceCommand c = voice.parse(heard, "");
		check(c.action == action && c.star_id == star,
			std::string("\"") + heard + "\" -> action " + std::to_string(static_cast<int>(c.action)) + ", star " +
				std::to_string(c.star_id));
	}
}

int main()
{
	using stellar::core::normalise;
	check(normalise("Barnard's Star!") == "barnards star", "apostrophes dropped, punctuation removed");
	check(normalise("  Go to   DELTA-Pavonis. ") == "go to delta pavonis", "case, hyphens and spaces");
	check(normalise("Sixty-one Cygni") == "61 cygni", "a compound number as digits");
	check(normalise("seventy") == "70" && normalise("nine") == "9", "tens and units alone");
	check(normalise("Boötes") == "bo tes", "non-ASCII separates");

	const stellar::core::Voice voice = make_voice();

	expect(voice, "Show Sirius", VoiceAction::Show, 32264);
	expect(voice, "show me the sun", VoiceAction::Show, 1);
	expect(voice, "Sirius", VoiceAction::Show, 32264);
	expect(voice, "Where is Vega?", VoiceAction::Show, 91000);
	expect(voice, "Go to Delta Pavonis", VoiceAction::GoTo, 98924);
	expect(voice, "take me to sixty one Cygni please", VoiceAction::GoTo, 103879);
	expect(voice, "fly to Barnards star", VoiceAction::GoTo, 87665);
	expect(voice, "Return to the Sun", VoiceAction::ReturnToSun, 1);
	expect(voice, "go home", VoiceAction::ReturnToSun, 1);
	expect(voice, "Let go", VoiceAction::LetGo, -1);

	// A restart mid-sentence: the last command counts (heard in the headset).
	expect(voice, "Go to Siri go to Sirius", VoiceAction::GoTo, 32264);
	expect(voice, "show Vega no go to Sirius", VoiceAction::GoTo, 32264);

	// Two stars called Alpha Centauri: the brighter, Rigil Kentaurus.
	expect(voice, "show Alpha Centauri", VoiceAction::Show, 71456);

	// Misheard, and close enough: words split and letters wrong.
	expect(voice, "go to Delta Pavo Ness", VoiceAction::GoTo, 98924);
	expect(voice, "show Serious", VoiceAction::Show, 32264);
	// Heard in the Mac test: two edits from both Pavonis and Leonis; it sounds like Pavonis.
	expect(voice, "Take me to Delta Peronis", VoiceAction::GoTo, 98924);
	// Heard in the headset, and sounding alike: "r" silent, d for t, one consonant over.
	expect(voice, "Stella go to Satter", VoiceAction::GoTo, 100240);
	expect(voice, "go to Battle girls", VoiceAction::GoTo, 27920);

	// "Go there" names no star: the app goes to the one kept or pointed at.
	expect(voice, "Stella go there", VoiceAction::GoThere, -1);
	expect(voice, "take me there", VoiceAction::GoThere, -1);
	expect(voice, "Go to HIP go there", VoiceAction::GoThere, -1);

	// Nearer and further from the star at hand, without its name.
	expect(voice, "Stellar back off", VoiceAction::BackOff, -1);
	expect(voice, "move back please", VoiceAction::BackOff, -1);
	expect(voice, "closer", VoiceAction::Closer, -1);
	expect(voice, "Stella move in", VoiceAction::Closer, -1);

	// Which way is up on the screen.
	expect(voice, "Stellar align to galactic north", VoiceAction::AlignGalactic, -1);
	expect(voice, "galactic north", VoiceAction::AlignGalactic, -1);
	expect(voice, "celestial north", VoiceAction::AlignCelestial, -1);

	// The panel describing the star, hidden and shown; "show" alone still names a star.
	expect(voice, "Stellar hide info", VoiceAction::HideInfo, -1);
	expect(voice, "hide the panel", VoiceAction::HideInfo, -1);
	expect(voice, "Stellar show info", VoiceAction::ShowInfo, -1);
	expect(voice, "show details please", VoiceAction::ShowInfo, -1);
	expect(voice, "show Sirius", VoiceAction::Show, 32264);

	// How far out stars are shown; "show" alone still names a star.
	expect(voice, "Stellar show more", VoiceAction::ShowMore, -1);
	expect(voice, "show more stars please", VoiceAction::ShowMore, -1);
	expect(voice, "further out", VoiceAction::ShowMore, -1);
	expect(voice, "show less", VoiceAction::ShowLess, -1);
	expect(voice, "show fewer stars", VoiceAction::ShowLess, -1);
	expect(voice, "show everything", VoiceAction::ShowEverything, -1);
	expect(voice, "show all the stars", VoiceAction::ShowEverything, -1);
	expect(voice, "show Vega", VoiceAction::Show, 91000);
	{
		const auto within = [&voice](const char* heard, double parsecs) {
			const auto c = voice.parse(heard, "Stellar");
			check(c.action == VoiceAction::ShowWithin && std::fabs(c.radius_parsecs - parsecs) < 1e-3,
				std::string("\"") + heard + "\" -> action " + std::to_string(static_cast<int>(c.action)) + ", " + std::to_string(c.radius_parsecs) + " pc");
		};
		within("Stellar show within 20 light years", 20.0 / 3.26156);
		within("Stellar, show within twenty light-years", 20.0 / 3.26156);
		within("Stellar show stars within 6.5 parsecs", 6.5);
		within("Stellar only within six point five parsecs", 6.5);
		within("Stellar show within 1,000 light years", 1000.0 / 3.26156);
		within("Stellar show within two hundred light years", 200.0 / 3.26156);
		within("Stellar show within a hundred parsecs", 100.0);
		within("Stellar show within 50", 50.0 / 3.26156); // no unit heard: light years
		within("Stellar show within 10 pc", 10.0);
		within("Stellar show within", 0.0); // no distance heard
		within("Stella show 20 light years", 20.0 / 3.26156); // heard in the headset, "within" lost
		within("Stellar show 5 parsecs", 5.0);
		// "Parsecs" as Apple's recogniser wrote it in the headset.
		within("Stella Show within six part six", 6.0);
		within("Stella Show within six Park", 6.0);
		within("Stella show within 6:05", 6.0);
		within("Stella within 6:06", 6.0);
		within("Stellar Show within to light years", 2.0 / 3.26156);
		within("No Stella show within five 6:00", 5.0);
		within("Stellar Show within 100 Perec", 100.0);
		within("Stella show 50 Pacific", 50.0);
		within("Stellar show within 6 point 5 parsecs", 6.5); // "point" is not parsecs
		within("Stellar show within 6.5 light years", 6.5 / 3.26156); // a decimal is still one
		const auto unit = [&voice](const char* heard) { return voice.parse(heard, "Stellar").radius_unit; };
		check(unit("Stellar show within 5") == 0 && unit("Stellar show within 5 light years") == 1 && unit("Stellar show within 5 parsecs") == 2,
			"the unit heard, or none");
	}
	// Quitting, heard in the headset as both.
	expect(voice, "Stellar quit", VoiceAction::Quit, -1);
	expect(voice, "Stellar exit", VoiceAction::Quit, -1);
	expect(voice, "close the app please", VoiceAction::Quit, -1);

	// A number alone is still no star's name, nor a distance without its unit.
	expect(voice, "show 5", VoiceAction::Show, -1);

	// Not close enough to anything.
	expect(voice, "go to the Pleiades", VoiceAction::GoTo, -1); // not Paradys, which sounds a consonant away
	expect(voice, "what a lovely evening", VoiceAction::None, -1);
	expect(voice, "Serious", VoiceAction::None, -1); // a sound-alike needs a command before it
	expect(voice, "Sirius", VoiceAction::Show, 32264);
	expect(voice, "show Vela", VoiceAction::Show, -1); // one letter from Vega, but too short to guess
	expect(voice, "go to bed", VoiceAction::GoTo, -1);  // three letters: exact or nothing
	expect(voice, "", VoiceAction::None, -1);

	// The command follows the wake word's last use.
	{
		const auto c = voice.parse("I said Stellar show Vega no Stellar go to Sirius", "Stellar");
		check(c.woken && c.action == VoiceAction::GoTo && c.star_id == 32264, "the last wake word counts");
		const auto none = voice.parse("show Vega", "Stellar");
		check(!none.woken && none.star_id == 91000, "parsed, but not woken, without the wake word");
		const auto heard = voice.parse("Stellar, go to the Pleiades", "Stellar");
		check(heard.woken && heard.target == "pleiades" && heard.name.empty(), "an unmatched name is reported as heard");
		const auto stella = voice.parse("Stella, show Vega", "Stellar");
		check(stella.woken && stella.star_id == 91000, "the wake word heard a letter short");
		const auto stellar = voice.parse("Stellars go to Sirius", "Stellar");
		check(stellar.woken && stellar.star_id == 32264, "the wake word heard a letter long");
		const auto still_a = voice.parse("Still a go there", "Stellar");
		check(still_a.woken && still_a.action == VoiceAction::GoThere, "a two-word sound-alike before a command");
		const auto still_i = voice.parse("Still I back off", "Stellar");
		check(still_i.woken && still_i.action == VoiceAction::BackOff, "\"still I\" before a command");
		const auto chatter = voice.parse("I am still a bit tired", "Stellar");
		check(!chatter.woken, "a sound-alike with no command after it does not wake");
		const auto bella = voice.parse("Bella show Vega", "Stellar");
		check(!bella.woken, "not woken by a word two letters away");
		const auto name = voice.parse("stellar go to delta pavo ness", "Stellar");
		check(name.name == "Delta Pavonis", "the matched name is the catalogue's");
	}

	// The brightest and the nearest stars in turn, each by its proper and plain Bayer names.
	{
		const auto& phrases = voice.phrases();
		const auto at = [&](const char* name) {
			std::size_t i = 0;
			while (i < phrases.size() && phrases[i] != name)
			{
				++i;
			}
			return i;
		};
		check(phrases.size() == 15 && phrases[0] == "Sun" && phrases[1] == "Sirius", "each name once, the Sun and Sirius first");
		check(at("Barnard's Star") < at("Zeta Velorum"), "a faint near star before a brighter far one");
		check(at("Alpha Centauri") == at("Rigil Kentaurus") + 1, "a star's names together");
	}
	{
		// A designation with a number is a hint only for a star with no other name.
		const stellar::core::Voice numbered({
			{"Sirius", 1, -1.44, 2.6, true},
			{"9 Canis Majoris", 1, -1.44, 2.6, false},
			{"61 Cygni", 2, 5.2, 3.5, false},
			{"Wolf 359", 3, 13.5, 2.4, true},
		});
		const auto& phrases = numbered.phrases();
		check(phrases.size() == 3 && phrases[0] == "Sirius" && phrases[1] == "Wolf 359" && phrases[2] == "61 Cygni",
			"no \"9 Canis Majoris\"; numbered names kept where they are all there is");
	}

	std::printf(g_failures ? "%d check(s) failed\n" : "all checks passed\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
