#include "StarVoice.h"

#include "Async/Async.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "StarField.h"
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
	/** "Closer" stops this far from a star: nearer than the camera's near plane it is not drawn. */
	constexpr double NearestParsecs = 0.2;
	/** How long the wake word heard alone waits for the command that follows it. */
	constexpr double WakeWaitSeconds = 3.0;

	/** Spoken without a star's name, so worth biasing the recogniser towards too. */
	const TCHAR* const CommandPhrases[] = {TEXT("Show"), TEXT("Go to"), TEXT("Go there"), TEXT("Back off"), TEXT("Closer"), TEXT("Return to the Sun"), TEXT("Let go")};
}

AStarVoice::AStarVoice()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AStarVoice::Setup(AStarField* InStarField, AStarPointer* InPointer)
{
	StarField = InStarField;
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
	if (Loading.IsValid())
	{
		Voice = Loading.Get();
	}
	stc_voice_close(Voice);
	Voice = nullptr;
	Super::EndPlay(Reason);
}

void AStarVoice::StartLoading()
{
	bLoadingStarted = true;
	LoadStart = FPlatformTime::Seconds();
	// The names take a few tenths of a second to read and index: not on the game thread.
	Loading = Async(EAsyncExecution::Thread, [Path = StarField->GetCataloguePath()]() -> StcVoice* {
		StcCatalogue* Catalogue = stc_catalogue_open(TCHAR_TO_UTF8(*Path));
		StcVoice* Names = stc_voice_open(Catalogue);
		stc_catalogue_close(Catalogue);
		return Names;
	});
}

void AStarVoice::StartListening()
{
	Voice = Loading.Get();
	Loading.Reset();
	if (Voice == nullptr)
	{
		UE_LOG(LogStarVoice, Error, TEXT("Voice: cannot read the star names from %s"), *StarField->GetCataloguePath());
		Recognizer.Reset();
		return;
	}
	TArray<FString> Hints;
	for (const TCHAR* Phrase : CommandPhrases)
	{
		Hints.Add(Phrase);
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
	UE_LOG(LogStarVoice, Display, TEXT("Voice: %d star names read in %.2f s; the recogniser is told of %d phrases"), Names, FPlatformTime::Seconds() - LoadStart,
		Hints.Num());
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
	if (!Recognizer || StarField == nullptr || Pointer == nullptr)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (!bHomeSet)
	{
		if (const APlayerController* Player = GetWorld()->GetFirstPlayerController(); Player != nullptr && Player->GetPawn() != nullptr)
		{
			Home = Player->GetPawn()->GetActorLocation();
			bHomeSet = true;
		}
	}
	if (!bLoadingStarted)
	{
		if (StarField->GetCataloguePath().IsEmpty())
		{
			return;
		}
		StartLoading();
	}
	if (Loading.IsValid())
	{
		if (!Loading.IsReady())
		{
			return;
		}
		StartListening();
		if (!Recognizer)
		{
			return;
		}
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
			// Show the words as they come only once the wake word is among them.
			StcVoiceCommand Command;
			if (Voice != nullptr && stc_voice_parse(Voice, TCHAR_TO_UTF8(*LastText), TCHAR_TO_UTF8(*WakeWord), &Command) >= 0 && Command.woken != 0)
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
	if (bWakeWord && !bTalking && !bAwaitingFinal && !bActed && !LastText.IsEmpty() && Now - LastChange > WakePauseSeconds)
	{
		StcVoiceCommand Command;
		const bool bParsed = stc_voice_parse(Voice, TCHAR_TO_UTF8(*LastText), TCHAR_TO_UTF8(*WakeWord), &Command) >= 0 && Command.woken != 0;
		const bool bWokenOnly = bParsed && Command.action == STC_VOICE_NONE && Command.target[0] == '\0';
		const bool bNoStarYet = bParsed && (Command.action == STC_VOICE_SHOW || Command.action == STC_VOICE_GO_TO) && Command.star_id < 0;
		if ((bWokenOnly || bNoStarYet) && Now - LastChange < WakeWaitSeconds)
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
	StcVoiceCommand Command;
	if (stc_voice_parse(Voice, TCHAR_TO_UTF8(*Heard), bNeedWake ? TCHAR_TO_UTF8(*WakeWord) : "", &Command) < 0)
	{
		return false;
	}
	if (bNeedWake && Command.woken == 0)
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
	Act(Command, Heard);
	return true;
}

void AStarVoice::Act(const StcVoiceCommand& Command, const FString& Heard)
{
	FString Name = UTF8_TO_TCHAR(Command.name);
	int32 Star = Command.star_id >= 0 ? StarField->FindById(Command.star_id) : INDEX_NONE;
	switch (Command.action)
	{
	case STC_VOICE_GO_THERE:
		// No name to mishear: the star kept, or the one the ray points at.
		Star = Pointer->GetTarget();
		if (Star == INDEX_NONE)
		{
			ShowLine(TEXT("Point at a star first"), false, ResultSeconds);
			return;
		}
		Name = StarField->GetName(Star);
		GoNear(Star);
		Pointer->Keep(Star);
		ShowLine(FString::Printf(TEXT("Going to %s"), *Name), false, ResultSeconds);
		return;
	case STC_VOICE_BACK_OFF:
	case STC_VOICE_CLOSER:
	{
		// Along the line from the star kept or pointed at, so it stays where it is in view.
		const bool bBack = Command.action == STC_VOICE_BACK_OFF;
		const APlayerController* Player = GetWorld()->GetFirstPlayerController();
		const FVector Head = Player->PlayerCameraManager->GetCameraLocation();
		Star = Pointer->GetTarget();
		if (Star == INDEX_NONE)
		{
			if (!bBack)
			{
				ShowLine(TEXT("Point at a star first"), false, ResultSeconds);
				return;
			}
			MoveHeadTo(Head - Player->PlayerCameraManager->GetCameraRotation().Vector() * AStarField::UnitsPerParsec);
			ShowLine(TEXT("Back 1 pc"), false, ResultSeconds);
			return;
		}
		const FVector Position = StarField->GetPosition(Star);
		const FVector Away = Head - Position;
		const double Parsecs = FMath::Max(Away.Size() / AStarField::UnitsPerParsec * (bBack ? DistanceFactor : 1.0 / DistanceFactor), NearestParsecs);
		MoveHeadTo(Position + Away.GetSafeNormal() * Parsecs * AStarField::UnitsPerParsec);
		const FString Distance = Parsecs < 10.0 ? FString::Printf(TEXT("%.1f"), Parsecs) : FString::Printf(TEXT("%.0f"), Parsecs);
		ShowLine(FString::Printf(TEXT("%s pc from %s"), *Distance, *StarField->GetName(Star)), false, ResultSeconds);
		return;
	}
	case STC_VOICE_LET_GO:
		Pointer->Keep(INDEX_NONE);
		ShowLine(TEXT("Let go"), false, ResultSeconds);
		return;
	case STC_VOICE_RETURN_TO_SUN:
		MoveHeadTo(Home);
		Pointer->Keep(Star);
		ShowLine(TEXT("Back to the Sun"), false, ResultSeconds);
		return;
	case STC_VOICE_SHOW:
	case STC_VOICE_GO_TO:
		if (Star == INDEX_NONE)
		{
			const FString Target = UTF8_TO_TCHAR(Command.target);
			ShowLine(Target.IsEmpty() ? FString::Printf(TEXT("\"%s\": which star?"), *Heard) : FString::Printf(TEXT("No star called \"%s\""), *Target), false,
				ResultSeconds);
			return;
		}
		if (Command.action == STC_VOICE_GO_TO)
		{
			GoNear(Star);
		}
		Pointer->Keep(Star);
		ShowLine(FString::Printf(TEXT("%s %s"), Command.action == STC_VOICE_GO_TO ? TEXT("Going to") : TEXT("Showing"), *Name), false, ResultSeconds);
		return;
	default:
		ShowLine(FString::Printf(TEXT("\"%s\": not a command"), *Heard), false, ResultSeconds);
		return;
	}
}

void AStarVoice::GoNear(int32 Star)
{
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	const FVector Head = Player->PlayerCameraManager->GetCameraLocation();
	const FVector Position = StarField->GetPosition(Star);
	MoveHeadTo(Position - (Position - Head).GetSafeNormal() * ArriveParsecs * AStarField::UnitsPerParsec);
}

void AStarVoice::MoveHeadTo(const FVector& Target)
{
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = Player != nullptr ? Player->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		return;
	}
	// In a headset the head is the pawn plus the tracked offset; move the pawn by the
	// difference so the head, not the floor under it, arrives.
	const FVector Head = Player->PlayerCameraManager->GetCameraLocation();
	Pawn->SetActorLocation(Pawn->GetActorLocation() + (Target - Head));
	if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
	{
		Movement->StopMovementImmediately();
	}
}

void AStarVoice::NextUtterance()
{
	LastText.Reset();
	bActed = false;
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
