#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "VoiceRecognizer.h"
#include "stellar_core.h"
#include "StarVoice.generated.h"

class AStarField;
class AStarPointer;

/**
 * Spoken commands: "Show Vega", "Go to Delta Pavonis", "Go there" (to the star kept or
 * pointed at), "Back off" and "Closer" (from and to it), "Return to the Sun", "Let go".
 *
 * The platform's recogniser (FVoiceRecognizer) hears the words; the core (stc_voice_*)
 * turns them into a command and a star, from the catalogue's names and their spoken
 * forms. Two ways to be heard, either or both (Listening):
 *  - Push to talk: hold a pinch on the hand that does not point (Vision Pro), a grip
 *    button (Vive) or V (desktop), say the command, and let go.
 *  - Wake word: say "Stellar, go to Sirius" at any time; the command is acted on once
 *    the words stop for WakePauseSeconds.
 * What was heard shows just above the panel, and the time from the end of speaking to
 * acting is logged, for the voice spike.
 */
UCLASS(Config = Game)
class AStarVoice : public AActor
{
	GENERATED_BODY()

public:
	AStarVoice();

	void Setup(AStarField* InStarField, AStarPointer* InPointer);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Read the names in the background, then start the recogniser with the brightest as hints. */
	void StartLoading();
	void StartListening();
	bool IsTalkHeld() const;
	/** Parse what was heard and do it. With bNeedWake, only if the wake word was in it. Returns whether it was a command. */
	bool Handle(const FString& Heard, bool bNeedWake, double SpokenEnd);
	void Act(const StcVoiceCommand& Command, const FString& Heard);
	/** Move the viewer to ArriveParsecs short of a star, on the line from where the head is now. */
	void GoNear(int32 Star);
	/** Move the viewer so the head ends at Target, keeping where the head is within the tracked space. */
	void MoveHeadTo(const FVector& Target);
	/** The next utterance: in wake-word mode, start listening afresh. */
	void NextUtterance();
	void ShowLine(const FString& Text, bool bActive, double Seconds);

	UPROPERTY()
	TObjectPtr<AStarField> StarField;

	UPROPERTY()
	TObjectPtr<AStarPointer> Pointer;

	/** "PushToTalk", "WakeWord", "Both" or "Off". -Voice= on the command line overrides it. */
	UPROPERTY(Config)
	FString Listening = TEXT("Both");

	/** Said before a command when not pushing to talk. -WakeWord= overrides it. */
	UPROPERTY(Config)
	FString WakeWord = TEXT("Stellar");

	/** Keys held to talk, comma-separated; the first found pressed counts. */
	UPROPERTY(Config)
	FString TalkKeys = TEXT("V,Vive_Left_Grip_Click,Vive_Right_Grip_Click");

	/** How many star names, the brightest and nearest in turn, the recogniser is told to expect. -VoiceHints= overrides it. */
	UPROPERTY(Config)
	int32 HintCount = 200;

	/** With the wake word, a command is acted on once the words have not changed for this long. */
	UPROPERTY(Config)
	float WakePauseSeconds = 0.8f;

	/** "Go to" stops this far short of the star, in parsecs. */
	UPROPERTY(Config)
	float ArriveParsecs = 0.5f;

	/** "Back off" multiplies the distance from the star at hand by this, "closer" divides it. */
	UPROPERTY(Config)
	float DistanceFactor = 3.0f;

	TUniquePtr<FVoiceRecognizer> Recognizer;
	TFuture<StcVoice*> Loading;
	StcVoice* Voice = nullptr;
	double LoadStart = 0.0;
	TArray<FKey> Keys;
	bool bPushToTalk = false;
	bool bWakeWord = false;
	bool bLoadingStarted = false;
	/** Push to talk: held now, and released and waiting for the final words. */
	bool bTalking = false;
	bool bAwaitingFinal = false;
	double ReleasedAt = 0.0;
	/** When the talk pinch or key was last seen held, and when the current hold began. */
	double LastHeld = -1.0e9;
	double TalkStart = 0.0;
	/** Wake word: the latest words of this utterance, when they last changed, and whether they were acted on. */
	FString LastText;
	double LastChange = 0.0;
	bool bActed = false;
	FString LoggedStatus;
	double LineUntil = 0.0;
	/** Where the viewer started, for "Return to the Sun". */
	FVector Home = FVector::ZeroVector;
	bool bHomeSet = false;
};
