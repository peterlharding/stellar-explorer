#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StellarExplorerGameMode.generated.h"

class AStarField;

/**
 * Builds the scene: the star field and a free-flying camera.
 *
 * Command line options, for measuring:
 *   -Autopilot          circle the Sun instead of taking input
 *   -LookAt=<ra>,<dec>  stand at the Sun facing RA (hours), Dec (degrees)
 *   -QuitAfter=<s>      log the frame times, take a screenshot and quit
 *   -TestObject         put a lit cube 1 m in front of a headset's start, to tell
 *                       "nothing renders" from "the stars do not show"
 *   -Console=<host>[:<port>]  be a console for the app on that host (AStarConsole)
 *   -SkySphere[=<pc>]   enclose the view in an opaque black sphere about the head, by default
 *                       beyond the furthest star; always done in a headset
 *                       (see SpawnSkySphere)
 */
UCLASS()
class AStellarExplorerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AStellarExplorerGameMode();

	/**
	 * The viewer stays within this of the Sun: the catalogue's edge, from where it is still a
	 * cloud of stars. Each "back off" triples the distance, and a few took the viewer thousands
	 * of parsecs out, where the whole catalogue was a faint smudge.
	 */
	static constexpr double MaxReachParsecs = 1500.0;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

private:
	void Finish();
	void SpawnTestObject();
	/**
	 * The visionOS compositor shows only pixels with something opaque behind them: the
	 * additive stars alone leave the sky black. An opaque black sphere beyond the furthest
	 * star gives every pixel depth and full alpha without hiding any star.
	 */
	void SpawnSkySphere();
	void LogCamera() const;

	UPROPERTY()
	TObjectPtr<AStarField> StarField;

	/** The sky sphere, when there is one: kept centred on the head. */
	UPROPERTY()
	TObjectPtr<AActor> SkySphere;


	bool bAutopilot = false;
	TOptional<FVector> LookAt;
	double QuitAfter = 0.0;
	double Elapsed = 0.0;
	double NextCameraLog = 0.0;
	bool bScreenshotRequested = false;
	TArray<float> FrameTimes;
};
