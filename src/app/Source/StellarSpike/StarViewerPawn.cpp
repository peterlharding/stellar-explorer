#include "StarViewerPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "MotionControllerComponent.h"

namespace
{
	/** How far the view turns as the cursor moves one pixel, dragging. */
	constexpr double LookDegreesPerPixel = 0.15;
	constexpr float ShiftSpeedFactor = 10.0f;
}

AStarViewerPawn::AStarViewerPawn()
{
	PrimaryActorTick.bCanEverTick = true;
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
	bHeadset = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

void AStarViewerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// The engine's mouse look turns the view whenever the mouse moves over the window, button
	// or not, unless the mouse is captured; with it free (Config/DefaultInput.ini) the view
	// spun as the cursor crossed it. Tick turns it on a right-button drag instead.
	PlayerInputComponent->AxisBindings.RemoveAll([](const FInputAxisBinding& Binding) {
		return Binding.AxisName == TEXT("DefaultPawn_Turn") || Binding.AxisName == TEXT("DefaultPawn_LookUp");
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

	if (UFloatingPawnMovement* Movement = Cast<UFloatingPawnMovement>(GetMovementComponent()))
	{
		if (BaseSpeed <= 0.0f)
		{
			BaseSpeed = Movement->MaxSpeed;
		}
		const bool bFast = Player->IsInputKeyDown(EKeys::LeftShift) || Player->IsInputKeyDown(EKeys::RightShift);
		Movement->MaxSpeed = BaseSpeed * (bFast ? ShiftSpeedFactor : 1.0f);
	}

	// The button and the cursor as Slate sees them: with the mouse left free, the game's own
	// input saw only the first right-button press after launch. A drag starts over the view.
	FVector2D InView;
	const bool bHeld = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::RightMouseButton);
	if (!bHeld || (!DragFrom.IsSet() && !Player->GetMousePosition(InView.X, InView.Y)))
	{
		DragFrom.Reset();
		return;
	}
	const FVector2D Cursor = FSlateApplication::Get().GetCursorPos();
	if (DragFrom.IsSet())
	{
		// Drag right to turn right, up to look up; never over the poles.
		const FVector2D Moved = Cursor - DragFrom.GetValue();
		FRotator Rotation = Player->GetControlRotation();
		Rotation.Yaw += Moved.X * LookDegreesPerPixel;
		Rotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch - Moved.Y * LookDegreesPerPixel), -89.0, 89.0);
		Player->SetControlRotation(Rotation);
	}
	DragFrom = Cursor;
}
