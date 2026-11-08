#include "StarViewerPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "MotionControllerComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarViewerPawn, Log, All);

namespace
{
	constexpr float ShiftSpeedFactor = 10.0f;
	// Dimmer than the ray's dots, and colour-coded like a ship's navigation lights: the left (port)
	// hand red, the right (starboard) green. (The dots glow at 0.4; the wands sit below them.)
	constexpr float WandBrightness = 0.22f;
	const FLinearColor LeftWandColour = FLinearColor(1.0f, 0.18f, 0.12f) * WandBrightness;
	const FLinearColor RightWandColour = FLinearColor(0.18f, 1.0f, 0.3f) * WandBrightness;
	// The basic cylinder is 100 units tall along its local Z and 100 across (radius 50).
	constexpr float CylinderLength = 100.0f;
	constexpr float CylinderRadius = 50.0f;
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

	// A rod on each controller, pointing along its aim; built in BeginPlay, where the mesh, the
	// material and the command-line size are to hand. Hidden until a controller is seen tracked.
	LeftWand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftWand"));
	LeftWand->SetupAttachment(LeftController);
	RightWand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightWand"));
	RightWand->SetupAttachment(RightController);
	for (UStaticMeshComponent* Wand : {LeftWand.Get(), RightWand.Get()})
	{
		Wand->SetCastShadow(false);
		Wand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Wand->SetVisibility(false);
	}
}

void AStarViewerPawn::BeginPlay()
{
	Super::BeginPlay();
	FParse::Value(FCommandLine::Get(), TEXT("WandLength="), WandLength);
	FParse::Value(FCommandLine::Get(), TEXT("WandRadius="), WandRadius);
	bShowWand = !FParse::Param(FCommandLine::Get(), TEXT("NoWand"));

	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/M_Pointer.M_Pointer")))
	{
		LeftWandMaterial = UMaterialInstanceDynamic::Create(Material, this);
		LeftWandMaterial->SetVectorParameterValue(TEXT("Colour"), LeftWandColour);
		RightWandMaterial = UMaterialInstanceDynamic::Create(Material, this);
		RightWandMaterial->SetVectorParameterValue(TEXT("Colour"), RightWandColour);
	}
	const auto SetupWand = [&](UStaticMeshComponent* Wand, UMaterialInstanceDynamic* Material)
	{
		if (Cylinder != nullptr)
		{
			Wand->SetStaticMesh(Cylinder);
		}
		if (Material != nullptr)
		{
			Wand->SetMaterial(0, Material);
		}
		// Lay the cylinder's length (its local Z) along the controller's forward (+X), thin it to a
		// rod, and slide it forward so it runs from the aim origin out to WandLength, the ray going
		// on from its tip. Relative scale is in the component's own frame, before this rotation.
		Wand->SetRelativeRotation(FRotationMatrix::MakeFromZ(FVector::ForwardVector).Rotator());
		Wand->SetRelativeScale3D(FVector(WandRadius / CylinderRadius, WandRadius / CylinderRadius, WandLength / CylinderLength));
		Wand->SetRelativeLocation(FVector(WandLength / 2.0f, 0.0f, 0.0f));
	};
	SetupWand(LeftWand, LeftWandMaterial);
	SetupWand(RightWand, RightWandMaterial);
	UE_LOG(LogStarViewerPawn, Display, TEXT("Wand: %s, %.1f cm long, %.1f cm across"), bShowWand ? TEXT("on") : TEXT("off (-NoWand)"), WandLength, 2.0f * WandRadius);
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
	// Each wand shows while its controller is tracked: the Vive shows both, so either hand can be
	// found; hand tracking and the desktop leave them untracked, so the wands stay hidden.
	LeftWand->SetVisibility(bShowWand && LeftController->IsTracked());
	RightWand->SetVisibility(bShowWand && RightController->IsTracked());

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
