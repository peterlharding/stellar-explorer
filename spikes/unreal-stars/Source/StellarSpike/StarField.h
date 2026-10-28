#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StarField.generated.h"

class UProceduralMeshComponent;

/**
 * Every star in catalogue.sqlite as one mesh: four vertices per star, all at the
 * star's position. The material (Scripts/make_star_material.py) turns each four
 * into a camera-facing sprite whose size and brightness follow from the star's
 * absolute magnitude and its distance from the camera.
 */
UCLASS()
class AStarField : public AActor
{
	GENERATED_BODY()

public:
	AStarField();

	/** Unreal units per parsec. */
	static constexpr double UnitsPerParsec = 100.0;

	int32 GetStarCount() const { return StarCount; }

protected:
	virtual void BeginPlay() override;

private:
	bool LoadStars(const FString& Path);

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Mesh;

	int32 StarCount = 0;
};
