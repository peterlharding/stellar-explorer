#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "StarViewerPawn.generated.h"

class UCameraComponent;
class UMotionControllerComponent;

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

	/** On the desktop: E and Q move up and down, and Shift moves ten times faster. */
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/**
	 * A motion controller, 0 the left hand and 1 the right, tracked in a headset that has
	 * controllers rather than hand tracking (the Vive). Its aim pose is where it points.
	 * Always valid to call; the component reports IsTracked() when a controller is present.
	 */
	const UMotionControllerComponent* GetHandController(int32 Hand) const { return Hand == 0 ? LeftController : RightController; }

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
};
