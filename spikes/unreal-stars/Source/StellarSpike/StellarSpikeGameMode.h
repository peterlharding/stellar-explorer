#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StellarSpikeGameMode.generated.h"

class AStarField;

/**
 * Builds the scene: the star field and a free-flying camera.
 *
 * Command line options, for measuring:
 *   -Autopilot          circle the Sun instead of taking input
 *   -LookAt=<ra>,<dec>  stand at the Sun facing RA (hours), Dec (degrees)
 *   -QuitAfter=<s>      log the frame times, take a screenshot and quit
 */
UCLASS()
class AStellarSpikeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AStellarSpikeGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

private:
	void Finish();

	UPROPERTY()
	TObjectPtr<AStarField> StarField;

	bool bAutopilot = false;
	TOptional<FVector> LookAt;
	double QuitAfter = 0.0;
	double Elapsed = 0.0;
	bool bScreenshotRequested = false;
	TArray<float> FrameTimes;
};
