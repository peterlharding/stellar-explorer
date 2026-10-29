#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StarPointer.generated.h"

class AStarField;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Point at a star and pinch to name it.
 *
 * In a headset each tracked hand casts a ray from an estimated shoulder through the hand,
 * and a pinch (thumb tip to index tip) selects. On the desktop the ray is the view's
 * centre and the left mouse button selects. The star pointed at is ringed; a selected
 * star is ringed in another colour and labelled. Pinching both hands for HoldToExitSeconds
 * quits. -PointerTest selects whatever is pointed at after 3 s, for checking it from a script.
 */
UCLASS()
class AStarPointer : public AActor
{
	GENERATED_BODY()

public:
	AStarPointer();

	void SetStarField(AStarField* InStarField) { StarField = InStarField; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	struct FRay
	{
		FVector Origin;
		FVector Direction;
		bool bPinched = false;
		/** Index into Hands: 0 left, 1 right, 2 the mouse. */
		int32 Hand = MouseHand;
		/** Thumb tip to index tip, for the log. */
		double Gap = 0.0;
	};

	struct FHand
	{
		bool bPinched = false;
		bool bWasPinched = false;
		/** Seen with thumb and index apart since tracking (re)started: only then can it pinch. */
		bool bSeenOpen = false;
		/** The star pointed at in recent frames, with the time: a pinch selects what was pointed at just before it. */
		TArray<TPair<double, int32>, TInlineAllocator<64>> Recent;
	};

	static constexpr int32 MouseHand = 2;

	/** The rays this frame: one per tracked hand in a headset, the view's centre on the desktop. */
	TArray<FRay, TInlineAllocator<2>> GetRays(const FVector& Head, const FRotator& HeadRotation);
	void Select(int32 Star);
	void PlaceRing(UProceduralMeshComponent* Ring, int32 Star, const FVector& Head) const;
	void PlaceLabel(const FVector& Head, const FRotator& HeadRotation);
	UMaterialInstanceDynamic* ColouredMaterial(const FLinearColor& Colour);

	UPROPERTY()
	TObjectPtr<AStarField> StarField;

	/** Each hand's ray, drawn as dots. */
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Dots[2];

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> HoverRing;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> SelectionRing;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;

	FHand Hands[3];
	int32 Hovered = INDEX_NONE;
	int32 Selected = INDEX_NONE;
	double BothPinchedSeconds = 0.0;
	bool bHeadset = false;

	static constexpr double MaxPointingAngleDegrees = 2.0;
	static constexpr double HoldToExitSeconds = 2.0;
};
