#include "StarViewerPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "MotionControllerComponent.h"

namespace
{
	constexpr float ShiftSpeedFactor = 10.0f;
}

AStarViewerPawn::AStarViewerPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->bLockToHmd = true;
	// On the desktop the view is the control rotation, which the mouse, the commands and the
	// scripted views set; without this the camera took the pawn's own rotation, which follows
	// it only in yaw and only through the engine's input, so nothing else could turn the view.
	Camera->bUsePawnControlRotation = true;

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
	bHeadset = true;
	// In a headset the head turns the view.
	Camera->bUsePawnControlRotation = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

void AStarViewerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// The engine's mouse look turns the view whenever the mouse moves over the window, button
	// or not, unless the mouse is captured, and it is left free (Config/DefaultInput.ini); the
	// pointer looks on a drag instead (FDesktopMouse). Moving up and down is E and Q alone, in
	// Tick: the engine's binding has Control too, which on a Mac is half of a right-click.
	PlayerInputComponent->AxisBindings.RemoveAll([](const FInputAxisBinding& Binding) {
		return Binding.AxisName == TEXT("DefaultPawn_Turn") || Binding.AxisName == TEXT("DefaultPawn_LookUp") ||
			Binding.AxisName == TEXT("DefaultPawn_MoveUp");
	});
}

void AStarViewerPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* Player = Cast<APlayerController>(GetController());
	if (bHeadset || Player == nullptr)
	{
		return;
	}
	const float Up = (Player->IsInputKeyDown(EKeys::E) ? 1.0f : 0.0f) - (Player->IsInputKeyDown(EKeys::Q) ? 1.0f : 0.0f);
	if (Up != 0.0f)
	{
		AddMovementInput(FVector::UpVector, Up);
	}
	if (UFloatingPawnMovement* Movement = Cast<UFloatingPawnMovement>(GetMovementComponent()))
	{
		if (BaseSpeed <= 0.0f)
		{
			BaseSpeed = Movement->MaxSpeed;
		}
		const bool bFast = Player->IsInputKeyDown(EKeys::LeftShift) || Player->IsInputKeyDown(EKeys::RightShift);
		Movement->MaxSpeed = BaseSpeed * (bFast ? ShiftSpeedFactor : 1.0f);
	}
}
