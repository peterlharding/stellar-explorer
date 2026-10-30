#include "StarPointer.h"

#include "BuildStamp.h"
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
#include "MotionControllerComponent.h"
#include "ProceduralMeshComponent.h"
#include "StarField.h"
#include "StarViewerPawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarPointer, Log, All);

namespace
{
	// Lengths in Unreal units; in a headset 1 unit is 1 cm.
	// The ray is drawn as dots from 10 cm to 3 m from the hand, spaced and sized to look
	// even as they recede, but never smaller than 3 mm across.
	constexpr int32 DotCount = 18;
	constexpr double FirstDot = 10.0;
	constexpr double LastDot = 300.0;
	constexpr double DotAngularDiameter = 0.007;
	constexpr double SmallestDot = 0.3;
	// Where the ray starts, relative to the head: about where the shoulder is.
	constexpr double ShoulderDrop = 15.0;
	constexpr double ShoulderWidth = 18.0;
	// Thumb tip to index tip: closer than PinchStart is a pinch. It ends when they part by
	// PinchRelease from the closest they came, or beyond PinchEnd, so it ends even for a hand
	// that rests nearly closed, and a hand near the threshold does not flicker. They must stay
	// apart for PinchDebounceSeconds: as the tips touch they hide each other from the cameras,
	// and the measured gap jumps for a frame or two. A hand can pinch only once seen more open
	// than OpenGap, and each pinch must close by PinchRearm from the widest they opened since
	// the last, so fingers still near each other as one ends do not start another; fingers
	// pinching quickly open only partway between pinches, so PinchRearm is small.
	// (A relaxed pointing hand rests at 4-4.5 cm.)
	constexpr double PinchStart = 3.5;
	constexpr double PinchRelease = 1.0;
	constexpr double PinchDebounceSeconds = 0.1;
	constexpr double PinchRearm = 0.6;
	constexpr double PinchEnd = 4.5;
	constexpr double OpenGap = 4.0;
	// Pinching moves the hand, and the ray with it, so a pinch selects what was pointed at
	// as thumb and index began to close: the last frame, at most PinchLookBackSeconds back,
	// that they were within PinchOnset of their widest in that time.
	constexpr double PinchLookBackSeconds = 0.5;
	constexpr double PinchOnset = 0.1;
	// How long the ray's direction takes to follow the hand, to steady it.
	constexpr double RaySmoothingSeconds = 0.05;
	// A controller trigger counts as a pinch past TriggerPress, and releases below TriggerRelease:
	// the gap between them stops a trigger held near the threshold from flickering.
	constexpr float TriggerPress = 0.6f;
	constexpr float TriggerRelease = 0.4f;
	// Angular radius of the rings around a star.
	constexpr double RingDegrees = 0.7;
	// The panel: its centre's place in front of the head (forward, right, up), its size,
	// the text heights, and how long it takes to follow the head's turns.
	const FVector PanelOffset(100.0, 22.0, -21.0);
	constexpr double PanelWidth = 25.0;
	constexpr double PanelHeight = 12.0;
	constexpr double PanelPadding = 1.6;
	constexpr double FrameThickness = 0.12;
	// Text heights go to SetWorldSize, which takes a float.
	constexpr float TitleSize = 2.4f;
	constexpr float DetailsSize = 1.8f;
	constexpr float HintSize = 1.4f;
	constexpr double PanelFollowSeconds = 0.12;
	constexpr double LightYearsPerParsec = 3.26156;

	// The colours as they are meant to look. The tonemapper washes bright colours out to
	// white, so they are drawn at OverlayBrightness; and text colours are used as they are,
	// unconverted from sRGB, so they are passed as ToFColor(false).
	constexpr float OverlayBrightness = 0.4f;
	const FLinearColor DotColour(0.5f, 0.75f, 1.0f);
	const FLinearColor FrameColour(0.45f, 0.6f, 0.75f);
	const FLinearColor HoverColour(0.3f, 0.65f, 0.9f);
	const FLinearColor SelectionColour(1.0f, 0.7f, 0.25f);
	const FLinearColor DetailsColour(0.65f, 0.7f, 0.75f);
	const FLinearColor HintColour(0.5f, 0.55f, 0.6f);

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

	/**
	 * A point on the panel, from its centre, as the viewer sees it. The panel faces the viewer
	 * along its +X, as text does, so the viewer's right is its -Y.
	 */
	FVector OnPanel(double Right, double Up)
	{
		return FVector(0.0, -Right, Up);
	}

	/** The panel's outline: a rectangle Width by Height of thin bars, each triangle in both windings. */
	void BuildFrame(UProceduralMeshComponent* Frame, double Width, double Height, double Thickness)
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		const auto Bar = [&](double Left, double Bottom, double Right, double Top)
		{
			const int32 A = Vertices.Num();
			Vertices.Append({OnPanel(Left, Bottom), OnPanel(Right, Bottom), OnPanel(Right, Top), OnPanel(Left, Top)});
			Triangles.Append({A, A + 1, A + 2, A, A + 2, A + 3});
			Triangles.Append({A, A + 2, A + 1, A, A + 3, A + 2});
		};
		const double X = Width / 2.0;
		const double Y = Height / 2.0;
		Bar(-X, Y - Thickness, X, Y);
		Bar(-X, -Y, X, -Y + Thickness);
		Bar(-X, -Y + Thickness, -X + Thickness, Y - Thickness);
		Bar(X - Thickness, -Y + Thickness, X, Y - Thickness);
		const TArray<FVector> NoNormals;
		const TArray<FVector2D> NoUVs;
		const TArray<FColor> NoColours;
		const TArray<FProcMeshTangent> NoTangents;
		Frame->CreateMeshSection(0, Vertices, Triangles, NoNormals, NoUVs, NoColours, NoTangents, false);
		Frame->SetCastShadow(false);
		Frame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	/** A distance, with fewer decimals the larger it is. */
	FString FormatDistance(double Value)
	{
		if (Value < 10.0)
		{
			return FString::Printf(TEXT("%.2f"), Value);
		}
		return Value < 1000.0 ? FString::Printf(TEXT("%.1f"), Value) : FString::Printf(TEXT("%.0f"), Value);
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
	// After the camera has followed the head this frame, so the ray starts where the hand is.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Dots = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ray"));
	Dots->SetupAttachment(RootComponent);
	Dots->SetMobility(EComponentMobility::Movable);
	Dots->SetCastShadow(false);
	Dots->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dots->SetVisibility(false);
	HoverRing = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HoverRing"));
	HoverRing->SetupAttachment(RootComponent);
	SelectionRing = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SelectionRing"));
	SelectionRing->SetupAttachment(RootComponent);
	Panel = CreateDefaultSubobject<USceneComponent>(TEXT("Panel"));
	Panel->SetupAttachment(RootComponent);
	Frame = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Frame"));
	Frame->SetupAttachment(Panel);
	// The name at the top left, inside the frame, with the details below it.
	const FVector TopLeft = OnPanel(-PanelWidth / 2.0 + PanelPadding, PanelHeight / 2.0 - PanelPadding);
	Title = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Title"));
	Title->SetupAttachment(Panel);
	Title->SetRelativeLocation(TopLeft);
	Title->SetWorldSize(TitleSize);
	Details = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Details"));
	Details->SetupAttachment(Panel);
	Details->SetRelativeLocation(TopLeft + OnPanel(0.0, -TitleSize * 1.3));
	Details->SetWorldSize(DetailsSize);
	Details->SetTextRenderColor((DetailsColour * OverlayBrightness).ToFColor(false));
	Hint = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Hint"));
	Hint->SetupAttachment(Panel);
	Hint->SetRelativeLocation(OnPanel(-PanelWidth / 2.0 + PanelPadding, -PanelHeight / 2.0 + PanelPadding + HintSize));
	Hint->SetWorldSize(HintSize);
	Hint->SetTextRenderColor((HintColour * OverlayBrightness).ToFColor(false));
	for (UTextRenderComponent* Text : {Title.Get(), Details.Get(), Hint.Get()})
	{
		Text->SetHorizontalAlignment(EHTA_Left);
		Text->SetVerticalAlignment(EVRTA_TextTop);
	}
}

void AStarPointer::BeginPlay()
{
	Super::BeginPlay();
	bHeadset = GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed();

	BuildRing(HoverRing);
	BuildRing(SelectionRing);
	HoverRing->SetMaterial(0, ColouredMaterial(HoverColour));
	SelectionRing->SetMaterial(0, ColouredMaterial(SelectionColour));
	BuildFrame(Frame, PanelWidth, PanelHeight, FrameThickness);
	Frame->SetMaterial(0, ColouredMaterial(FrameColour));
	if (UMaterialInterface* Text = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/M_Label.M_Label")))
	{
		Title->SetTextMaterial(Text);
		Details->SetTextMaterial(Text);
		Hint->SetTextMaterial(Text);
	}
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Dots->SetStaticMesh(Sphere);
	Dots->SetMaterial(0, ColouredMaterial(DotColour));
	for (int32 Dot = 0; Dot < DotCount; ++Dot)
	{
		Dots->AddInstance(FTransform::Identity, true);
	}

	// The desktop's view centre always points; there is nothing to turn on.
	bPointingStarted = !bHeadset;
	FParse::Value(FCommandLine::Get(), TEXT("PointingHand="), PointingHand);
	FParse::Value(FCommandLine::Get(), TEXT("GestureGate="), GestureGateSeconds);
	FParse::Value(FCommandLine::Get(), TEXT("DoublePinch="), DoublePinchSeconds);
	FParse::Value(FCommandLine::Get(), TEXT("RightTriggerKey="), RightTriggerKey);
	FParse::Value(FCommandLine::Get(), TEXT("LeftTriggerKey="), LeftTriggerKey);
	if (!bHeadset)
	{
		Pointer = MouseHand;
	}
	else if (PointingHand.Equals(TEXT("Left"), ESearchCase::IgnoreCase))
	{
		Pointer = 0;
	}
	else if (!PointingHand.Equals(TEXT("Right"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogStarPointer, Warning, TEXT("PointingHand is \"%s\", not Right or Left: pointing with the right"), *PointingHand);
	}
	UE_LOG(LogStarPointer, Display, TEXT("Pointing with hand %d; pinches ignored for %.2f s after a star is kept or let go; two within %.2f s let it go"), Pointer,
		GestureGateSeconds, DoublePinchSeconds);
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
	Instance->SetVectorParameterValue(TEXT("Colour"), Colour * OverlayBrightness);
	return Instance;
}

TArray<AStarPointer::FRay, TInlineAllocator<2>> AStarPointer::GetRays(const FVector& Head, const FRotator& HeadRotation)
{
	TArray<FRay, TInlineAllocator<2>> Rays;
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (!bHeadset)
	{
		const APlayerController* Player = GetWorld()->GetFirstPlayerController();
		// A click is a pinch that closes at once: open until the frame it happens.
		const bool bClicked = Player && Player->IsInputKeyDown(EKeys::LeftMouseButton);
		Rays.Add({Head, HeadRotation.Vector(), bClicked, MouseHand, bClicked ? 0.0 : PinchEnd});
		return Rays;
	}
	const IHandTracker* Tracker = FindHandTracker();
	if (Tracker == nullptr)
	{
		// No hand tracking: a headset with controllers (the Vive), or none yet.
		AppendControllerRays(Rays);
		return Rays;
	}
	bUsingControllers = false;
	// The shoulders turn with the head's yaw but not its pitch or roll.
	const FRotator Body(0.0, HeadRotation.Yaw, 0.0);
	const FVector Right = FRotationMatrix(Body).GetUnitAxis(EAxis::Y);
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
		// The ray passes through the middle of the hand, which moves less in a pinch than the
		// fingers do; the index knuckle stands in if the palm is not tracked.
		const FVector& Palm = Joints[int32(EHandKeypoint::Palm)];
		const FVector Through = Palm.IsNearlyZero() ? Joints[int32(EHandKeypoint::IndexProximal)] : Palm;
		const double Gap = FVector::Distance(Joints[int32(EHandKeypoint::ThumbTip)], Joints[int32(EHandKeypoint::IndexTip)]);
		const bool bWasPinched = State.bPinched;
		State.bSeenOpen |= Gap > OpenGap;
		if (State.bPinched)
		{
			State.PinchClosest = FMath::Min(State.PinchClosest, Gap);
			if (Gap < FMath::Min(PinchEnd, State.PinchClosest + PinchRelease))
			{
				State.OpenSince = -1.0;
			}
			else if (State.OpenSince < 0.0)
			{
				State.OpenSince = Now;
			}
			State.bPinched = State.OpenSince < 0.0 || Now - State.OpenSince < PinchDebounceSeconds;
			State.OpenWidest = Gap;
		}
		else if (State.bSeenOpen && Gap < PinchStart && State.OpenWidest - Gap >= PinchRearm)
		{
			State.bPinched = true;
			State.PinchClosest = Gap;
			State.OpenSince = -1.0;
		}
		else
		{
			State.OpenWidest = FMath::Max(State.OpenWidest, Gap);
		}
		// Log a hand that closed but not enough to pinch, to tune PinchStart by.
		if (State.bSeenOpen && !State.bPinched && !bWasPinched && Gap < OpenGap)
		{
			if (State.ClosestGap == TNumericLimits<double>::Max())
			{
				State.DipOpenedTo = State.OpenWidest;
			}
			State.ClosestGap = FMath::Min(State.ClosestGap, Gap);
		}
		else
		{
			if (Gap >= OpenGap && State.ClosestGap < OpenGap)
			{
				UE_LOG(LogStarPointer, Display, TEXT("Near pinch with hand %d: thumb to index %.1f cm at closest, after opening to %.1f cm"), HandIndex, State.ClosestGap,
					State.DipOpenedTo);
			}
			State.ClosestGap = TNumericLimits<double>::Max();
		}
		const FVector Shoulder = Head - FVector(0.0, 0.0, ShoulderDrop) + Right * (Hand == EControllerHand::Left ? -ShoulderWidth : ShoulderWidth);
		const FVector Direction = (Through - Shoulder).GetSafeNormal();
		const double Follow = 1.0 - FMath::Exp(-GetWorld()->GetDeltaSeconds() / RaySmoothingSeconds);
		State.Direction = State.Direction.IsZero() ? Direction : (State.Direction + (Direction - State.Direction) * Follow).GetSafeNormal();
		Rays.Add({Through, State.Direction, State.bPinched, HandIndex, Gap});
	}
	return Rays;
}

void AStarPointer::AppendControllerRays(TArray<FRay, TInlineAllocator<2>>& Rays)
{
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	const AStarViewerPawn* Viewer = Player ? Cast<AStarViewerPawn>(Player->GetPawn()) : nullptr;
	if (Viewer == nullptr)
	{
		return;
	}
	const double Now = GetWorld()->GetRealTimeSeconds();
	const bool bLog = Now >= NextControllerLog;
	// Right first then left, as the hand path adds them, so the pointing hand's index matches.
	for (const int32 HandIndex : {1, 0})
	{
		FHand& State = Hands[HandIndex];
		const UMotionControllerComponent* Controller = Viewer->GetHandController(HandIndex);
		if (Controller == nullptr || !Controller->IsTracked())
		{
			State = FHand();
			continue;
		}
		bUsingControllers = true;
		const FKey TriggerKey(*(HandIndex == 0 ? LeftTriggerKey : RightTriggerKey));
		const float Trigger = Player->GetInputAnalogKeyState(TriggerKey);
		// Hysteresis around the press threshold; State.bPinched carries between frames.
		State.bPinched = State.bPinched ? Trigger > TriggerRelease : Trigger > TriggerPress;
		if (bLog)
		{
			UE_LOG(LogStarPointer, Display, TEXT("Controller hand %d: trigger \"%s\" = %.2f, pinched %d"), HandIndex,
				*(HandIndex == 0 ? LeftTriggerKey : RightTriggerKey), Trigger, State.bPinched);
		}
		// The ray is the controller's aim pose. A controller barely moves as the trigger is pulled,
		// so, unlike a hand, it needs no smoothing and no look-back: Gap is 0 while pulled and
		// PinchEnd otherwise, which keeps the selection logic in Tick selecting what is pointed at.
		const double Gap = State.bPinched ? 0.0 : PinchEnd;
		Rays.Add({Controller->GetComponentLocation(), Controller->GetForwardVector(), State.bPinched, HandIndex, Gap});
	}
	if (bLog)
	{
		NextControllerLog = Now + 1.0;
	}
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
	bool bPointing = false;
	for (const FRay& Ray : Rays)
	{
		FHand& State = Hands[Ray.Hand];
		Pinched += Ray.bPinched ? 1 : 0;
		// The other hand only counts towards the gesture that quits.
		if (Ray.Hand != Pointer)
		{
			State.bWasPinched = Ray.bPinched;
			continue;
		}
		bPointing = true;
		const int32 Star = StarField->FindStar(Ray.Origin, Ray.Direction, MaxPointingAngleDegrees);
		State.Recent.Add({Now, Star, Ray.Gap});
		State.Recent.RemoveAll([Now](const FSighting& Sighting) { return Sighting.Time < Now - PinchLookBackSeconds; });
		// A pinch (or click) that has just started keeps what this ray pointed at as the
		// fingers began to close; with a star kept, two in quick succession let it go.
		if (Ray.bPinched && !State.bWasPinched)
		{
			double Widest = 0.0;
			for (const FSighting& Sighting : State.Recent)
			{
				Widest = FMath::Max(Widest, Sighting.Gap);
			}
			int32 Before = State.Recent[0].Star;
			for (const FSighting& Sighting : State.Recent)
			{
				if (Sighting.Gap >= Widest - PinchOnset)
				{
					Before = Sighting.Star;
				}
			}
			UE_LOG(LogStarPointer, Display, TEXT("Pinch with hand %d, thumb to index %.1f cm (widest %.1f): pointing at %d now, %d before"), Ray.Hand, Ray.Gap,
				Widest, Star, Before);
			if (Now < GateUntil)
			{
				UE_LOG(LogStarPointer, Display, TEXT("Pinch ignored: %.2f s after the ray was turned on or a star was kept or let go"),
					Now - (GateUntil - GestureGateSeconds));
			}
			else if (!bPointingStarted)
			{
				UE_LOG(LogStarPointer, Display, TEXT("Pointing started"));
				bPointingStarted = true;
				GateUntil = Now + GestureGateSeconds;
			}
			else if (Selected == INDEX_NONE)
			{
				Select(Before);
			}
			else if (Now - LastKeptPinch < DoublePinchSeconds)
			{
				UE_LOG(LogStarPointer, Display, TEXT("Second pinch %.2f s after the first: letting the star go"), Now - LastKeptPinch);
				Select(INDEX_NONE);
			}
			else
			{
				LastKeptPinch = Now;
			}
		}
		State.bWasPinched = Ray.bPinched;

		// A kept star holds the panel, and the ray goes until the star is let go.
		if (bPointingStarted && Selected == INDEX_NONE)
		{
			Hovered = Star;
			for (int32 Dot = 0; Dot < DotCount; ++Dot)
			{
				const double Distance = FirstDot * FMath::Pow(LastDot / FirstDot, double(Dot) / (DotCount - 1));
				const double Diameter = FMath::Max(SmallestDot, Distance * DotAngularDiameter);
				// The basic sphere is 100 units across.
				Dots->UpdateInstanceTransform(Dot, FTransform(FQuat::Identity, Ray.Origin + Ray.Direction * Distance, FVector(Diameter / 100.0)),
					true, Dot == DotCount - 1, true);
			}
		}
	}
	Dots->SetVisibility(bHeadset && bPointing && bPointingStarted && Selected == INDEX_NONE);
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
	PlacePanel(Head, HeadRotation, DeltaSeconds);
}

void AStarPointer::Select(int32 Star)
{
	Selected = Star;
	GateUntil = GetWorld()->GetRealTimeSeconds() + GestureGateSeconds;
	LastKeptPinch = -1.0e9;
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

void AStarPointer::PlacePanel(const FVector& Head, const FRotator& HeadRotation, float DeltaSeconds)
{
	// The panel turns with the head a little behind it, which is steadier to read than one
	// fixed to the view, and keeps its distance as the head moves.
	const FQuat Target = HeadRotation.Quaternion();
	PanelRotation = bPanelPlaced ? FQuat::Slerp(PanelRotation, Target, 1.0 - FMath::Exp(-DeltaSeconds / PanelFollowSeconds)) : Target;
	bPanelPlaced = true;
	// Facing back along the view, as text faces along its +X.
	Panel->SetWorldLocationAndRotation(Head + PanelRotation.RotateVector(PanelOffset), PanelRotation * FQuat(FVector::UpVector, PI));

	if (BothPinchedSeconds > 0.0)
	{
		Title->SetText(FText::FromString(bUsingControllers ? TEXT("Hold both triggers to exit") : TEXT("Keep pinching to exit")));
		Title->SetTextRenderColor((SelectionColour * OverlayBrightness).ToFColor(false));
		Details->SetText(FText::FromString(FString::Printf(TEXT("Exiting in %.0f s"), FMath::CeilToDouble(HoldToExitSeconds - BothPinchedSeconds))));
		Hint->SetText(FText::GetEmpty());
		return;
	}
	if (Selected != INDEX_NONE)
	{
		ShowStar(Selected, SelectionColour);
		Hint->SetText(FText::FromString(!bHeadset ? TEXT("Double-click to let it go")
				: bUsingControllers               ? TEXT("Pull the trigger twice to let it go")
												  : TEXT("Pinch twice to let it go")));
		return;
	}
	Hint->SetText(FText::FromString(!bPointingStarted ? (bUsingControllers ? TEXT("Then pull the trigger again to keep a star") : TEXT("Then pinch again to keep a star"))
			: !bHeadset                             ? TEXT("Click to keep a star")
			: bUsingControllers                     ? TEXT("Pull the trigger to keep a star, both to exit")
													: TEXT("Pinch to keep a star, both hands to exit")));
	if (Hovered != INDEX_NONE)
	{
		ShowStar(Hovered, HoverColour);
		return;
	}
	Title->SetText(FText::FromString(!bPointingStarted ? (bUsingControllers ? TEXT("Pull the trigger to start pointing") : TEXT("Pinch to start pointing"))
			: bHeadset                              ? TEXT("Point at a star")
													: TEXT("Look at a star")));
	Title->SetTextRenderColor((HintColour * OverlayBrightness).ToFColor(false));
	// With nothing to describe, the build, so a session can be matched to its code.
	Details->SetText(FText::FromString(FString::Printf(TEXT("Build %s\n%s, %s"), STELLAR_SPIKE_COMMIT, STELLAR_SPIKE_BRANCH, STELLAR_SPIKE_BUILT)));
}

void AStarPointer::ShowStar(int32 Star, const FLinearColor& Colour)
{
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	const double Distance = FVector::Distance(StarField->GetPosition(Star), Player->PlayerCameraManager->GetCameraLocation());
	const double Parsecs = Distance / AStarField::UnitsPerParsec;
	// Rounded first, and +0.0 so that a magnitude like -0.01 shows as 0.0, not -0.0.
	const double Magnitude = FMath::RoundToDouble(AStarField::ApparentMagnitude(StarField->GetAbsoluteMagnitude(Star), Distance) * 10.0) / 10.0 + 0.0;
	const FString& Type = StarField->GetSpectralType(Star);
	FString Text = Type.IsEmpty() ? FString() : Type + TEXT(", ");
	Text += FString::Printf(TEXT("magnitude %.1f\n%s pc (%s ly) from here"), Magnitude, *FormatDistance(Parsecs), *FormatDistance(Parsecs * LightYearsPerParsec));
	Title->SetText(FText::FromString(StarField->GetName(Star)));
	Title->SetTextRenderColor((Colour * OverlayBrightness).ToFColor(false));
	Details->SetText(FText::FromString(Text));
}
