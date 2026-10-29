#include "StarPointer.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "GameFramework/PlayerController.h"
#include "IHandTracker.h"
#include "IXRTrackingSystem.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "ProceduralMeshComponent.h"
#include "StarField.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarPointer, Log, All);

namespace
{
	// Lengths in Unreal units; in a headset 1 unit is 1 cm.
	// The ray is drawn as dots from 10 cm to 3 m from the hand, spaced and sized to look
	// even as they recede, but never smaller than 1.5 mm across.
	constexpr int32 DotCount = 18;
	constexpr double FirstDot = 10.0;
	constexpr double LastDot = 300.0;
	constexpr double DotAngularDiameter = 0.0035;
	constexpr double SmallestDot = 0.15;
	// Where the ray starts, relative to the head: about where the shoulder is.
	constexpr double ShoulderDrop = 15.0;
	constexpr double ShoulderWidth = 18.0;
	// Thumb tip to index tip: closer than this is a pinch, and it ends only when they
	// part by more than the second, so a hand near the threshold does not flicker.
	constexpr double PinchStart = 2.5;
	constexpr double PinchEnd = 4.0;
	// Pinching moves the hand, and the ray with it: select what was pointed at this long before.
	constexpr double PinchLeadSeconds = 0.15;
	// Angular radius of the rings around a star, and the label's distance and text height.
	constexpr double RingDegrees = 0.7;
	constexpr double LabelDistance = 120.0;
	constexpr double LabelSize = 2.5;
	constexpr double LightYearsPerParsec = 3.26156;

	const FLinearColor DotColour(0.1f, 0.18f, 0.25f);
	const FLinearColor HoverColour(0.3f, 0.65f, 0.9f);
	const FLinearColor SelectionColour(1.0f, 0.7f, 0.25f);

	/** A flat ring in the plane facing +X, of radius 1, each triangle in both windings. */
	void BuildRing(UProceduralMeshComponent* Ring)
	{
		constexpr int32 Segments = 48;
		constexpr double Inner = 0.82;
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		for (int32 Segment = 0; Segment <= Segments; ++Segment)
		{
			const double Angle = 2.0 * PI * Segment / Segments;
			const FVector Direction(0.0, FMath::Cos(Angle), FMath::Sin(Angle));
			Vertices.Add(Direction);
			Vertices.Add(Direction * Inner);
		}
		for (int32 Segment = 0; Segment < Segments; ++Segment)
		{
			const int32 A = 2 * Segment;
			Triangles.Append({A, A + 2, A + 1, A + 1, A + 2, A + 3});
			Triangles.Append({A, A + 1, A + 2, A + 1, A + 3, A + 2});
		}
		const TArray<FVector> NoNormals;
		const TArray<FVector2D> NoUVs;
		const TArray<FColor> NoColours;
		const TArray<FProcMeshTangent> NoTangents;
		Ring->CreateMeshSection(0, Vertices, Triangles, NoNormals, NoUVs, NoColours, NoTangents, false);
		Ring->SetCastShadow(false);
		Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ring->SetVisibility(false);
	}

	/** A text rotation that faces a viewer looking along Direction, upright for them. */
	FRotator FacingViewer(const FVector& Direction)
	{
		// Text faces along its +X; Rotation() keeps roll at zero, so the text stays upright
		// relative to the horizon even when the star is far above or below.
		return (-Direction).Rotation();
	}

	IHandTracker* FindHandTracker()
	{
		for (IHandTracker* Tracker : IModularFeatures::Get().GetModularFeatureImplementations<IHandTracker>(IHandTracker::GetModularFeatureName()))
		{
			if (Tracker->IsHandTrackingStateValid())
			{
				return Tracker;
			}
		}
		return nullptr;
	}
}

AStarPointer::AStarPointer()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the camera has followed the head this frame, so the rays start where the hands are.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		Dots[Hand] = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Hand == 0 ? TEXT("FirstRay") : TEXT("SecondRay"));
		Dots[Hand]->SetupAttachment(RootComponent);
		Dots[Hand]->SetMobility(EComponentMobility::Movable);
		Dots[Hand]->SetCastShadow(false);
		Dots[Hand]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Dots[Hand]->SetVisibility(false);
	}
	HoverRing = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HoverRing"));
	HoverRing->SetupAttachment(RootComponent);
	SelectionRing = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SelectionRing"));
	SelectionRing->SetupAttachment(RootComponent);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextTop);
	Label->SetWorldSize(LabelSize);
	Label->SetTextRenderColor(SelectionColour.ToFColor(true));
	Label->SetVisibility(false);
}

void AStarPointer::BeginPlay()
{
	Super::BeginPlay();
	bHeadset = GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed();

	BuildRing(HoverRing);
	BuildRing(SelectionRing);
	HoverRing->SetMaterial(0, ColouredMaterial(HoverColour));
	SelectionRing->SetMaterial(0, ColouredMaterial(SelectionColour));
	if (UMaterialInterface* Text = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/M_Label.M_Label")))
	{
		Label->SetTextMaterial(Text);
	}
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	for (UInstancedStaticMeshComponent* Ray : Dots)
	{
		Ray->SetStaticMesh(Sphere);
		Ray->SetMaterial(0, ColouredMaterial(DotColour));
		for (int32 Dot = 0; Dot < DotCount; ++Dot)
		{
			Ray->AddInstance(FTransform::Identity, true);
		}
	}
}

UMaterialInstanceDynamic* AStarPointer::ColouredMaterial(const FLinearColor& Colour)
{
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/M_Pointer.M_Pointer"));
	if (Material == nullptr)
	{
		UE_LOG(LogStarPointer, Error, TEXT("/Game/Generated/M_Pointer is missing: run Scripts/make_star_material.py"));
		return nullptr;
	}
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
	Instance->SetVectorParameterValue(TEXT("Colour"), Colour);
	return Instance;
}

TArray<AStarPointer::FRay, TInlineAllocator<2>> AStarPointer::GetRays(const FVector& Head, const FRotator& HeadRotation)
{
	TArray<FRay, TInlineAllocator<2>> Rays;
	if (!bHeadset)
	{
		const APlayerController* Player = GetWorld()->GetFirstPlayerController();
		Rays.Add({Head, HeadRotation.Vector(), Player && Player->IsInputKeyDown(EKeys::LeftMouseButton), MouseHand});
		return Rays;
	}
	const IHandTracker* Tracker = FindHandTracker();
	if (Tracker == nullptr)
	{
		return Rays;
	}
	// The shoulders turn with the head's yaw but not its pitch or roll.
	const FRotator Body(0.0, HeadRotation.Yaw, 0.0);
	const FVector Right = FRotationMatrix(Body).GetUnitAxis(EAxis::Y);
	// Right hand first, so it is the one whose star is ringed when both point.
	for (const EControllerHand Hand : {EControllerHand::Right, EControllerHand::Left})
	{
		const int32 HandIndex = Hand == EControllerHand::Left ? 0 : 1;
		FHand& State = Hands[HandIndex];
		// Not GetKeypointState: that returns the last joints it had even for a hand no longer
		// tracked, and before any, all of them at one point, which reads as a pinch.
		TArray<FVector> Joints;
		TArray<FQuat> Rotations;
		TArray<float> Radii;
		bool bTracked = false;
		if (!Tracker->GetAllKeypointStates(Hand, Joints, Rotations, Radii, bTracked) || !bTracked || Joints.Num() < EHandKeypointCount)
		{
			State = FHand();
			continue;
		}
		const FVector Knuckle = Joints[int32(EHandKeypoint::IndexProximal)];
		const double Gap = FVector::Distance(Joints[int32(EHandKeypoint::ThumbTip)], Joints[int32(EHandKeypoint::IndexTip)]);
		State.bSeenOpen |= Gap > PinchEnd;
		State.bPinched = State.bSeenOpen && (State.bPinched ? Gap < PinchEnd : Gap < PinchStart);
		const FVector Shoulder = Head - FVector(0.0, 0.0, ShoulderDrop) + Right * (Hand == EControllerHand::Left ? -ShoulderWidth : ShoulderWidth);
		Rays.Add({Knuckle, (Knuckle - Shoulder).GetSafeNormal(), State.bPinched, HandIndex, Gap});
	}
	return Rays;
}

void AStarPointer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (StarField == nullptr || Player == nullptr || Player->PlayerCameraManager == nullptr)
	{
		return;
	}
	const FVector Head = Player->PlayerCameraManager->GetCameraLocation();
	const FRotator HeadRotation = Player->PlayerCameraManager->GetCameraRotation();

	const TArray<FRay, TInlineAllocator<2>> Rays = GetRays(Head, HeadRotation);
	const double Now = GetWorld()->GetRealTimeSeconds();
	Hovered = INDEX_NONE;
	int32 Pinched = 0;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const bool bShown = bHeadset && Rays.IsValidIndex(Index);
		Dots[Index]->SetVisibility(bShown);
		if (!Rays.IsValidIndex(Index))
		{
			continue;
		}
		const FRay& Ray = Rays[Index];
		const int32 Star = StarField->FindStar(Ray.Origin, Ray.Direction, MaxPointingAngleDegrees);
		if (Index == 0)
		{
			Hovered = Star;
		}
		if (bShown)
		{
			for (int32 Dot = 0; Dot < DotCount; ++Dot)
			{
				const double Distance = FirstDot * FMath::Pow(LastDot / FirstDot, double(Dot) / (DotCount - 1));
				const double Diameter = FMath::Max(SmallestDot, Distance * DotAngularDiameter);
				// The basic sphere is 100 units across.
				Dots[Index]->UpdateInstanceTransform(Dot, FTransform(FQuat::Identity, Ray.Origin + Ray.Direction * Distance, FVector(Diameter / 100.0)),
					true, Dot == DotCount - 1, true);
			}
		}
		Pinched += Ray.bPinched ? 1 : 0;

		FHand& State = Hands[Ray.Hand];
		State.Recent.Add({Now, Star});
		State.Recent.RemoveAll([Now](const TPair<double, int32>& Entry) { return Entry.Key < Now - 4.0 * PinchLeadSeconds; });
		// A pinch (or click) that has just started selects what this ray pointed at just
		// before, or clears the selection.
		if (Ray.bPinched && !State.bWasPinched)
		{
			int32 Before = State.Recent[0].Value;
			for (const TPair<double, int32>& Entry : State.Recent)
			{
				if (Entry.Key <= Now - PinchLeadSeconds)
				{
					Before = Entry.Value;
				}
			}
			UE_LOG(LogStarPointer, Display, TEXT("Pinch with hand %d, thumb to index %.1f cm: pointing at %d now, %d before"), Ray.Hand, Ray.Gap, Star, Before);
			Select(Before);
		}
		State.bWasPinched = Ray.bPinched;
	}
	// A hand that is no longer tracked has no pinch to continue.
	for (FHand& State : Hands)
	{
		State.bWasPinched &= State.bPinched;
	}

	// Both hands pinched and held: quit.
	BothPinchedSeconds = Pinched == 2 ? BothPinchedSeconds + DeltaSeconds : 0.0;
	if (BothPinchedSeconds >= HoldToExitSeconds)
	{
		UE_LOG(LogStarPointer, Display, TEXT("Both hands pinched for %.0f s: exiting"), HoldToExitSeconds);
		FPlatformMisc::RequestExit(false, TEXT("StarPointer"));
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("PointerTest")) && Selected == INDEX_NONE && GetWorld()->GetTimeSeconds() > 3.0)
	{
		Select(Hovered);
	}

	PlaceRing(HoverRing, Hovered != Selected ? Hovered : INDEX_NONE, Head);
	PlaceRing(SelectionRing, Selected, Head);
	PlaceLabel(Head, HeadRotation);
}

void AStarPointer::Select(int32 Star)
{
	Selected = Star;
	if (Star == INDEX_NONE)
	{
		UE_LOG(LogStarPointer, Display, TEXT("Selection cleared"));
		return;
	}
	UE_LOG(LogStarPointer, Display, TEXT("Selected %s (%s) at %s"), *StarField->GetName(Star), *StarField->GetSpectralType(Star),
		*(StarField->GetPosition(Star) / AStarField::UnitsPerParsec).ToString());
}

void AStarPointer::PlaceRing(UProceduralMeshComponent* Ring, int32 Star, const FVector& Head) const
{
	Ring->SetVisibility(Star != INDEX_NONE);
	if (Star == INDEX_NONE)
	{
		return;
	}
	const FVector Position = StarField->GetPosition(Star);
	const double Distance = FVector::Distance(Position, Head);
	Ring->SetWorldLocationAndRotation(Position, (Head - Position).Rotation());
	Ring->SetWorldScale3D(FVector(Distance * FMath::Tan(FMath::DegreesToRadians(RingDegrees))));
}

void AStarPointer::PlaceLabel(const FVector& Head, const FRotator& HeadRotation)
{
	const bool bExiting = BothPinchedSeconds > 0.0;
	Label->SetVisibility(Selected != INDEX_NONE || bExiting);
	if (bExiting)
	{
		// In front of the viewer, wherever they look.
		const FVector Position = Head + HeadRotation.Vector() * LabelDistance;
		Label->SetText(FText::FromString(FString::Printf(TEXT("Keep pinching to exit: %.0f"), FMath::CeilToDouble(HoldToExitSeconds - BothPinchedSeconds))));
		Label->SetWorldLocationAndRotation(Position, FacingViewer(HeadRotation.Vector()));
		return;
	}
	if (Selected == INDEX_NONE)
	{
		return;
	}
	const FVector Star = StarField->GetPosition(Selected);
	const double Distance = FVector::Distance(Star, Head);
	const double Parsecs = Distance / AStarField::UnitsPerParsec;
	const FString& Type = StarField->GetSpectralType(Selected);
	FString Text = StarField->GetName(Selected);
	if (!Type.IsEmpty())
	{
		Text += TEXT("\n") + Type;
	}
	// Rounded first, and +0.0 so that a magnitude like -0.01 shows as 0.0, not -0.0.
	const double Magnitude = FMath::RoundToDouble(AStarField::ApparentMagnitude(StarField->GetAbsoluteMagnitude(Selected), Distance) * 10.0) / 10.0 + 0.0;
	Text += FString::Printf(TEXT("\n%.2f pc (%.1f ly) from here\nmagnitude %.1f"), Parsecs, Parsecs * LightYearsPerParsec, Magnitude);
	Label->SetText(FText::FromString(Text));
	// Just below the star as seen from here, near enough to read, and square on to the
	// viewer however far up or down the star is.
	const FVector Direction = (Star - Head).GetSafeNormal();
	const FRotator Facing = FacingViewer(Direction);
	const FVector Up = FRotationMatrix(Facing).GetUnitAxis(EAxis::Z);
	const FVector Position = Head + Direction * LabelDistance - Up * LabelSize * 1.5;
	Label->SetWorldLocationAndRotation(Position, Facing);
}
