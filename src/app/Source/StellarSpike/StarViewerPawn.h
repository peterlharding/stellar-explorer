#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "StarViewerPawn.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class UMotionControllerComponent;
class UStaticMeshComponent;

/**
 * The viewer: a free-flying pawn on the desktop, a tracked head in a headset.
 * The camera follows the headset when there is one.
 */
UCLASS()
class AStarViewerPawn : public ADefaultPawn
{
	GENERATED_BODY()

public:
	AStarViewerPawn();

	/** In a headset the head turns the view; only yaw comes from the controller, so the horizon stays level. */
	void UseHeadset();

	/** On the desktop: E and Q move up and down, and Shift moves ten times faster. In a headset: show a wand on each tracked controller. */
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/**
	 * A motion controller, 0 the left hand and 1 the right, tracked in a headset that has
	 * controllers rather than hand tracking (the Vive). Its aim pose is where it points.
	 * Always valid to call; the component reports IsTracked() when a controller is present.
	 */
	const UMotionControllerComponent* GetHandController(int32 Hand) const { return Hand == 0 ? LeftController : RightController; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	bool bHeadset = false;
	/** The movement's top speed as the game mode set it, before Shift multiplies it. */
	float BaseSpeed = 0.0f;

	UPROPERTY()
	TObjectPtr<UMotionControllerComponent> LeftController;

	UPROPERTY()
	TObjectPtr<UMotionControllerComponent> RightController;

	/**
	 * A thin rod at each controller, shown while it is tracked (the Vive), so the held hand is easy
	 * to keep track of; the ray runs on from its tip. A simple shape, not a lifelike model. Hidden
	 * on a headset with hand tracking and on the desktop, where no controller reports tracked.
	 */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LeftWand;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> RightWand;

	/** One material per hand: the left (port) wand red, the right (starboard) green. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> LeftWandMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> RightWandMaterial;

	/** The wand's length and radius in cm, from -WandLength= / -WandRadius=; -NoWand hides it. */
	float WandLength = 12.0f;
	float WandRadius = 0.45f;
	bool bShowWand = true;
};
