#include "StarViewerPawn.h"

#include "Camera/CameraComponent.h"

AStarViewerPawn::AStarViewerPawn()
{
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->bLockToHmd = true;
}

void AStarViewerPawn::UseHeadset()
{
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}
