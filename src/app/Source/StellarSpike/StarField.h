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

	/** The star material's exposure in a headset, where the monitor's is too dim. */
	static constexpr float HeadsetExposure = 0.25f;

	/**
	 * On the desktop: the material's own 0.04 left all but the brightest stars too faint on
	 * a Mac's screen; 0.1, judged against 0.04 and 0.2 looking at Orion, shows the fainter
	 * ones without crowding.
	 */
	static constexpr float DesktopExposure = 0.1f;

	int32 GetStarCount() const { return StarCount; }

	/**
	 * The star a ray points at: of the stars within MaxAngleDegrees of the ray, the one
	 * that looks brightest from its origin. INDEX_NONE if there is none.
	 */
	int32 FindStar(const FVector& Origin, const FVector& Direction, double MaxAngleDegrees) const;

	/** The star with this catalogue id (StcStar.id), or INDEX_NONE. */
	int32 FindById(int64 Id) const
	{
		const int32* Star = IndexById.Find(Id);
		return Star != nullptr ? *Star : INDEX_NONE;
	}

	/** The catalogue the stars came from, for anything else that reads it (voice). */
	const FString& GetCataloguePath() const { return CataloguePath; }

	const FVector& GetPosition(int32 Star) const { return Positions[Star]; }
	const FString& GetName(int32 Star) const { return Names[Star]; }
	const FString& GetSpectralType(int32 Star) const { return SpectralTypes[Star]; }
	double GetAbsoluteMagnitude(int32 Star) const { return AbsoluteMagnitudes[Star]; }

	/** Apparent magnitude of a star with absolute magnitude AbsMag seen from Distance Unreal units. */
	static double ApparentMagnitude(double AbsMag, double Distance);

protected:
	virtual void BeginPlay() override;

private:
	bool LoadStars(const FString& Path);

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Mesh;

	int32 StarCount = 0;
	FString CataloguePath;
	TMap<int64, int32> IndexById;

	// One entry per star, in the order of the mesh's sprites.
	TArray<FVector> Positions;
	TArray<float> AbsoluteMagnitudes;
	TArray<FString> Names;
	TArray<FString> SpectralTypes;
};
