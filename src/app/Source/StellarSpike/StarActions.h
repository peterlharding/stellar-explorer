#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "GameFramework/Actor.h"
#include "stellar_core.h"
#include "StarActions.generated.h"

class AStarField;
class AStarPointer;

/**
 * The named actions, one place for every way in: voice, the desktop console, and later the
 * panel and the settings screen. "Show" and "go to" a star by name, "go there" (to the star
 * kept or pointed at), "back off" and "closer" (from and to it), "return to the Sun", "let go",
 * "hide info" and "show info" (the panel describing the star), and how far out stars are shown:
 * "show more", "show less", "show within 20 light years" (or parsecs), "show everything"; and
 * "quit", said twice.
 *
 * A command arrives as text, typed or heard, and the core (stc_voice_*) turns it into an
 * action and a star, from the catalogue's names and their spoken forms, which this actor
 * reads in the background at start.
 */
UCLASS(Config = Game)
class AStarActions : public AActor
{
	GENERATED_BODY()

public:
	AStarActions();

	void Setup(AStarField* InStarField, AStarPointer* InPointer);

	virtual void Tick(float DeltaSeconds) override;

	/** The names, once read; null until then, or if they could not be. */
	const StcVoice* GetNames() const { return Names; }

	/** Do what text says, with no wake word needed; returns what to show: done, or why not. */
	FString Run(const FString& Text);

	/** Whether "quit" was said once and waits to be said again. */
	bool IsQuitAsked() const;

	/** Do a parsed command; Heard is the text it came from. Returns what to show. */
	FString Perform(const StcVoiceCommand& Command, const FString& Heard);

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Show only the stars within Parsecs (0: every star), and say how far out they are shown. */
	FString SetViewRadius(double Parsecs);
	/** Move the viewer to ArriveParsecs short of a star, on the line from where the head is now; returns where the head goes. */
	FVector GoNear(int32 Star);
	/**
	 * On the desktop, turn the view from From to the star, so a move keeps it centred. From is
	 * where the head is after a move: the camera reports where it was in the last frame.
	 */
	void Face(int32 Star, const FVector& From);
	/** Move the viewer so the head ends at Target, keeping where the head is within the tracked space. */
	void MoveHeadTo(const FVector& Target);

	UPROPERTY()
	TObjectPtr<AStarField> StarField;

	UPROPERTY()
	TObjectPtr<AStarPointer> Pointer;

	/** "Go to" stops this far short of the star, in parsecs. */
	UPROPERTY(Config)
	float ArriveParsecs = 0.5f;

	/** "Back off" multiplies the distance from the star at hand by this, "closer" divides it. */
	UPROPERTY(Config)
	float DistanceFactor = 3.0f;

	/** When "quit" was last asked for: a second within QuitConfirmSeconds quits. */
	double QuitAskedAt = -1.0e9;

	TFuture<StcVoice*> Loading;
	StcVoice* Names = nullptr;
	double LoadStart = 0.0;
	bool bLoadingStarted = false;
};
