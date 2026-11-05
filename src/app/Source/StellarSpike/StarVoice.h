#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "VoiceRecognizer.h"
#include "stellar_core.h"
#include "StarVoice.generated.h"

class AStarActions;
class AStarPointer;

/**
 * Spoken commands: "Show Vega", "Go to Delta Pavonis", "Go there" (to the star kept or
 * pointed at), "Back off" and "Closer" (from and to it), "Return to the Sun", "Let go",
 * "Hide info" and "Show info".
 *
 * The platform's recogniser (FVoiceRecognizer) hears the words; the core (stc_voice_*)
 * turns them into a command and a star, from the names AStarActions reads, and
 * AStarActions does it. Two ways to be heard, either or both (Listening):
 *  - Push to talk: hold a pinch on the hand that does not point (Vision Pro), a grip
 *    button (Vive) or V (desktop), say the command, and let go.
 *  - Wake word: say "Stellar, go to Sirius" at any time; the command is acted on once
 *    it has stood unchanged for CommandPauseSeconds (WakePauseSeconds with a star's name,
 *    which the recogniser may still correct; "show within" waits for its distance, and a
 *    while for its unit), whatever else is still being said, or once the words stop for
 *    WakePauseSeconds.
 * What was heard shows just above the panel, and the time from the end of speaking to
 * acting is logged, for the voice spike.
 */
UCLASS(Config = Game)
class AStarVoice : public AActor
{
	GENERATED_BODY()

public:
	AStarVoice();

	void Setup(AStarActions* InActions, AStarPointer* InPointer);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Start the recogniser, with the star names as hints. */
	void StartListening();
	bool IsTalkHeld() const;
	/** Parse what was heard and do it. With bNeedWake, only if the wake word was in it. Returns whether it was a command. */
	bool Handle(const FString& Heard, bool bNeedWake, double SpokenEnd);
	/** The next utterance: in wake-word mode, start listening afresh. */
	void NextUtterance();
	void ShowLine(const FString& Text, bool bActive, double Seconds);

	UPROPERTY()
	TObjectPtr<AStarActions> Actions;

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

	/**
	 * With the wake word, a command needing no star's name is acted on once it has stood this
	 * long, though other words go on: speech nearby need not hold it up. A named star waits
	 * WakePauseSeconds, as the recogniser may yet correct the name.
	 */
	UPROPERTY(Config)
	float CommandPauseSeconds = 0.4f;

	TUniquePtr<FVoiceRecognizer> Recognizer;
	/** The names, from AStarActions; null until listening starts. */
	const StcVoice* Voice = nullptr;
	TArray<FKey> Keys;
	bool bPushToTalk = false;
	bool bWakeWord = false;
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
	/** Wake word: the command in the words so far (an StcVoiceAction and a star), and since when it has been that. */
	int32 LastAction = STC_VOICE_NONE;
	int64 LastStar = -1;
	/** With "show within", the distance heard so far, in parsecs, and its unit (StcVoiceCommand.radius_unit). */
	double LastRadius = 0.0;
	int32 LastUnit = 0;
	double CommandSince = 0.0;
	FString LoggedStatus;
	double LineUntil = 0.0;
};
