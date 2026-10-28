#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "StarViewerPawn.generated.h"

class UCameraComponent;

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

private:
	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;
};
