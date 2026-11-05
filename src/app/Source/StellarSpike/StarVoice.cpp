#include "StarVoice.h"

#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "StarActions.h"
#include "StarPointer.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarVoice, Log, All);

namespace
{
	/** How long a result stays above the panel. */
	constexpr double ResultSeconds = 6.0;
	/** How long a problem (no permission, no recogniser) stays above the panel. */
	constexpr double ProblemSeconds = 15.0;
	/**
	 * Push to talk ends only once the hold has been let go this long: hand tracking loses a
	 * held pinch for a frame or two while the wearer talks, and in the first headset test
	 * each such drop ended the utterance before a word was heard.
	 */
	constexpr double TalkReleaseSeconds = 0.3;
	/** How long the wake word heard alone waits for the command that follows it. */
	constexpr double WakeWaitSeconds = 3.0;
	/**
	 * "Show within 5", its unit not yet heard, waits this long for one: acted on at once, in
	 * the headset "five" became light years as "parsecs" was still being said.
	 */
	constexpr double UnitWaitSeconds = 1.5;

	/** Spoken without a star's name, so worth biasing the recogniser towards too. */
	const TCHAR* const CommandPhrases[] = {TEXT("Show"), TEXT("Go to"), TEXT("Go there"), TEXT("Back off"), TEXT("Closer"), TEXT("Return to the Sun"), TEXT("Let go"),
		TEXT("Hide info"), TEXT("Show info"), TEXT("Show more"), TEXT("Show less"), TEXT("Show everything"), TEXT("Show within"), TEXT("light years"),
		TEXT("parsecs"), TEXT("Quit"), TEXT("Backtrack")};
}

AStarVoice::AStarVoice()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AStarVoice::Setup(AStarActions* InActions, AStarPointer* InPointer)
{
	Actions = InActions;
	Pointer = InPointer;
}

void AStarVoice::BeginPlay()
{
	Super::BeginPlay();
	FParse::Value(FCommandLine::Get(), TEXT("Voice="), Listening);
	FParse::Value(FCommandLine::Get(), TEXT("WakeWord="), WakeWord);
	FParse::Value(FCommandLine::Get(), TEXT("VoiceHints="), HintCount);
	bPushToTalk = Listening.Equals(TEXT("PushToTalk"), ESearchCase::IgnoreCase) || Listening.Equals(TEXT("Both"), ESearchCase::IgnoreCase);
	bWakeWord = Listening.Equals(TEXT("WakeWord"), ESearchCase::IgnoreCase) || Listening.Equals(TEXT("Both"), ESearchCase::IgnoreCase);
	TArray<FString> KeyNames;
	TalkKeys.ParseIntoArray(KeyNames, TEXT(","));
	for (const FString& Name : KeyNames)
	{
		Keys.Add(FKey(*Name.TrimStartAndEnd()));
	}
	if (!bPushToTalk && !bWakeWord)
	{
		UE_LOG(LogStarVoice, Display, TEXT("Voice off (Listening=%s)"), *Listening);
		return;
	}
	Recognizer = FVoiceRecognizer::Create();
	if (!Recognizer)
	{
		UE_LOG(LogStarVoice, Display, TEXT("Voice: no speech recogniser on this platform yet"));
		return;
	}
	UE_LOG(LogStarVoice, Display, TEXT("Voice: %s%s%s"), bPushToTalk ? TEXT("push to talk") : TEXT(""), bPushToTalk && bWakeWord ? TEXT(" and ") : TEXT(""),
		bWakeWord ? *FString::Printf(TEXT("the wake word \"%s\""), *WakeWord) : TEXT(""));
}

void AStarVoice::EndPlay(const EEndPlayReason::Type Reason)
{
	Recognizer.Reset();
	Super::EndPlay(Reason);
}

void AStarVoice::StartListening()
{
	Voice = Actions->GetNames();
	TArray<FString> Hints;
	for (const TCHAR* Phrase : CommandPhrases)
	{
		Hints.Add(Phrase);
	}
	// Distances as said after "show within": "parsecs" alone was not enough, and in the headset
	// "within 6 parsecs" was written "within 6:06", read as a time.
	for (const int32 Count : {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 50, 100})
	{
		Hints.Add(FString::Printf(TEXT("within %d parsecs"), Count));
		Hints.Add(FString::Printf(TEXT("within %d light years"), Count));
	}
	if (bWakeWord)
	{
		Hints.Add(WakeWord);
	}
	const int32 Names = stc_voice_phrase_count(Voice);
	for (int32 Index = 0; Index < FMath::Min(Names, HintCount); ++Index)
	{
		Hints.Add(UTF8_TO_TCHAR(stc_voice_phrase(Voice, Index)));
	}
	UE_LOG(LogStarVoice, Display, TEXT("Voice: the recogniser is told of %d phrases, %d of them star names"), Hints.Num(), FMath::Min(Names, HintCount));
	Recognizer->Start(Hints);
}

bool AStarVoice::IsTalkHeld() const
{
	if (Pointer != nullptr && Pointer->IsOtherHandPinched())
	{
		return true;
	}
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (Player == nullptr)
	{
		return false;
	}
	for (const FKey& Key : Keys)
	{
		if (Key.IsValid() && Player->IsInputKeyDown(Key))
		{
			return true;
		}
	}
	return false;
}

void AStarVoice::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Recognizer || Actions == nullptr || Pointer == nullptr)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	// Listening starts once the star names are read, to give the recogniser as hints.
	if (Voice == nullptr)
	{
		if (Actions->GetNames() == nullptr)
		{
			return;
		}
		StartListening();
	}

	// The recogniser's state, as it changes: logged, and shown when it cannot listen.
	const FString Status = Recognizer->GetStatus();
	if (Status != LoggedStatus)
	{
		UE_LOG(LogStarVoice, Display, TEXT("Voice: %s"), *Status);
		LoggedStatus = Status;
		if (!Status.StartsWith(TEXT("Listening")) && !Status.StartsWith(TEXT("Asking")))
		{
			ShowLine(Status, false, ProblemSeconds);
		}
	}

	// Push to talk: a new utterance when the hold starts; its end when the hold ends.
	if (bPushToTalk)
	{
		if (IsTalkHeld())
		{
			LastHeld = Now;
		}
		const bool bHeld = Now - LastHeld < TalkReleaseSeconds;
		if (bHeld && !bTalking)
		{
			UE_LOG(LogStarVoice, Display, TEXT("Voice: talk held"));
			TalkStart = Now;
			bTalking = true;
			bAwaitingFinal = false;
			Recognizer->Restart();
			LastText.Reset();
			ShowLine(TEXT("Listening..."), true, 0.0);
		}
		else if (!bHeld && bTalking)
		{
			bTalking = false;
			bAwaitingFinal = true;
			// Timed from the moment the hold really ended, not from the end of the grace period.
			ReleasedAt = LastHeld;
			UE_LOG(LogStarVoice, Display, TEXT("Voice: talk let go after %.2f s held"), ReleasedAt - TalkStart);
			Recognizer->Finish();
		}
	}

	FVoiceRecognizer::FHeard Heard;
	while (Recognizer->Poll(Heard))
	{
		if (!Heard.Error.IsEmpty() && !Heard.Text.IsEmpty())
		{
			UE_LOG(LogStarVoice, Display, TEXT("Voice: utterance ended with \"%s\" (%s)"), *Heard.Text, *Heard.Error);
		}
		if (bTalking || bAwaitingFinal)
		{
			if (!Heard.Text.IsEmpty())
			{
				ShowLine(FString::Printf(TEXT("\"%s\""), *Heard.Text), true, 0.0);
			}
			if (Heard.bFinal)
			{
				// Ended by letting go, or by the recogniser on a long silence while still held.
				const bool bReleased = bAwaitingFinal;
				bAwaitingFinal = false;
				if (bReleased)
				{
					UE_LOG(LogStarVoice, Display, TEXT("Voice: final words %.2f s after letting go"), Now - ReleasedAt);
				}
				if (Heard.Text.IsEmpty())
				{
					UE_LOG(LogStarVoice, Display, TEXT("Voice: heard nothing%s%s"), Heard.Error.IsEmpty() ? TEXT("") : TEXT(": "), *Heard.Error);
					ShowLine(TEXT("Heard nothing"), false, ResultSeconds);
				}
				else
				{
					Handle(Heard.Text, false, bReleased ? ReleasedAt : Now);
				}
				if (bTalking)
				{
					Recognizer->Restart();
				}
				else
				{
					NextUtterance();
				}
			}
			continue;
		}
		if (!bWakeWord)
		{
			continue;
		}
		if (Heard.Text != LastText)
		{
			LastText = Heard.Text;
			LastChange = Now;
			bActed = false;
			// Show the words as they come only once the wake word is among them, and note when
			// the command in them last changed.
			StcVoiceCommand Command{};
			const bool bWoken = Voice != nullptr && stc_voice_parse(Voice, TCHAR_TO_UTF8(*LastText), TCHAR_TO_UTF8(*WakeWord), &Command) >= 0 && Command.woken != 0;
			const int32 Action = bWoken ? Command.action : STC_VOICE_NONE;
			const int64 Star = bWoken ? Command.star_id : -1;
			const double Radius = bWoken ? Command.radius_parsecs : 0.0;
			const int32 Unit = bWoken ? Command.radius_unit : 0;
			if (Action != LastAction || Star != LastStar || Radius != LastRadius || Unit != LastUnit)
			{
				LastAction = Action;
				LastStar = Star;
				LastRadius = Radius;
				LastUnit = Unit;
				CommandSince = Now;
			}
			if (bWoken)
			{
				ShowLine(FString::Printf(TEXT("\"%s\""), *LastText), true, 0.0);
			}
		}
		if (Heard.bFinal)
		{
			if (!bActed && !LastText.IsEmpty())
			{
				Handle(LastText, true, LastChange);
			}
			NextUtterance();
		}
	}

	// The wake word: act once the words have stopped changing. A command that is not yet
	// complete waits longer for the recogniser to finish: the wake word alone (in the headset
	// "Stellar" was often followed by a pause), or a star's name that matches nothing yet
	// (Apple's recogniser writes "Siri" first and corrects it to "Sirius" a moment later).
	// Whatever is acted on or ignored, the next utterance starts afresh.
	// But a complete command is acted on once it has stood a moment, though words go on: in a
	// room with other people talking the words may not stop for a long time.
	// "Show within" is complete once it has its distance; without a unit yet, it waits a while for one.
	const bool bNamed = LastAction == STC_VOICE_SHOW || LastAction == STC_VOICE_GO_TO;
	const bool bWithin = LastAction == STC_VOICE_SHOW_WITHIN;
	const bool bComplete = LastAction != STC_VOICE_NONE && (!bNamed || LastStar >= 0) && (!bWithin || LastRadius > 0.0);
	const double Pause = bNamed ? WakePauseSeconds : bWithin && LastUnit == 0 ? UnitWaitSeconds : CommandPauseSeconds;
	if (bWakeWord && !bTalking && !bAwaitingFinal && !bActed && bComplete && Now - CommandSince > Pause)
	{
		bActed = true;
		Handle(LastText, true, CommandSince);
		NextUtterance();
	}
	if (bWakeWord && !bTalking && !bAwaitingFinal && !bActed && !LastText.IsEmpty() && Now - LastChange > WakePauseSeconds)
	{
		StcVoiceCommand Command{};
		const bool bParsed = stc_voice_parse(Voice, TCHAR_TO_UTF8(*LastText), TCHAR_TO_UTF8(*WakeWord), &Command) >= 0 && Command.woken != 0;
		const bool bWokenOnly = bParsed && Command.action == STC_VOICE_NONE && Command.target[0] == '\0';
		const bool bNoStarYet = bParsed && (Command.action == STC_VOICE_SHOW || Command.action == STC_VOICE_GO_TO) && Command.star_id < 0;
		const bool bNoDistanceYet = bParsed && Command.action == STC_VOICE_SHOW_WITHIN && Command.radius_parsecs <= 0.0;
		const bool bNoUnitYet = bParsed && Command.action == STC_VOICE_SHOW_WITHIN && Command.radius_unit == 0;
		if (((bWokenOnly || bNoStarYet || bNoDistanceYet) && Now - LastChange < WakeWaitSeconds) || (bNoUnitYet && Now - LastChange < UnitWaitSeconds))
		{
			ShowLine(TEXT("Listening..."), true, 0.0);
		}
		else
		{
			bActed = true;
			Handle(LastText, true, LastChange);
			NextUtterance();
		}
	}

	if (LineUntil > 0.0 && Now > LineUntil)
	{
		Pointer->SetVoiceLine(FString(), false);
		LineUntil = 0.0;
	}
}

bool AStarVoice::Handle(const FString& Heard, bool bNeedWake, double SpokenEnd)
{
	StcVoiceCommand Command{};
	if (stc_voice_parse(Voice, TCHAR_TO_UTF8(*Heard), bNeedWake ? TCHAR_TO_UTF8(*WakeWord) : "", &Command) < 0)
	{
		return false;
	}
	// Asked to say "quit" again, the wearer says just that: no wake word needed.
	const bool bConfirmingQuit = Command.action == STC_VOICE_QUIT && Actions->IsQuitAsked();
	if (bNeedWake && Command.woken == 0 && !bConfirmingQuit)
	{
		UE_LOG(LogStarVoice, Display, TEXT("Voice: heard \"%s\" without the wake word: ignored"), *Heard);
		// Said like a command, it was likely meant as one: say why nothing happened.
		if (Command.action != STC_VOICE_NONE)
		{
			// Kept short enough to sit above the panel.
			const FString Shown = Heard.Len() > 24 ? Heard.Left(23) + TEXT("...") : Heard;
			ShowLine(FString::Printf(TEXT("\"%s\": say \"%s\" first"), *Shown, *WakeWord), false, ResultSeconds);
		}
		return false;
	}
	UE_LOG(LogStarVoice, Display, TEXT("Voice: heard \"%s\" -> action %d, star %lld \"%s\", target \"%s\"; acting %.2f s after the words ended"), *Heard,
		Command.action, Command.star_id, UTF8_TO_TCHAR(Command.name), UTF8_TO_TCHAR(Command.target), FPlatformTime::Seconds() - SpokenEnd);
	ShowLine(Actions->Perform(Command, Heard), false, ResultSeconds);
	return true;
}

void AStarVoice::NextUtterance()
{
	LastText.Reset();
	bActed = false;
	LastAction = STC_VOICE_NONE;
	LastRadius = 0.0;
	LastUnit = 0;
	LastStar = -1;
	if (bWakeWord)
	{
		Recognizer->Restart();
	}
}

void AStarVoice::ShowLine(const FString& Text, bool bActive, double Seconds)
{
	Pointer->SetVoiceLine(Text, bActive);
	LineUntil = Seconds > 0.0 ? FPlatformTime::Seconds() + Seconds : 0.0;
}
