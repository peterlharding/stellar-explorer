#include "StarViewerPawn.h"

#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"

AStarViewerPawn::AStarViewerPawn()
{
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->bLockToHmd = true;

	// The controllers track in their own right when the headset has them; on a headset with hand
	// tracking, or none, they simply never report IsTracked(). The aim pose points where the
	// controller points, which is what a ray wants (the grip pose is the handle).
	LeftController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftController"));
	LeftController->SetupAttachment(RootComponent);
	LeftController->MotionSource = FName(TEXT("LeftAim"));
	RightController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightController"));
	RightController->SetupAttachment(RootComponent);
	RightController->MotionSource = FName(TEXT("RightAim"));
}

void AStarViewerPawn::UseHeadset()
{
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}
