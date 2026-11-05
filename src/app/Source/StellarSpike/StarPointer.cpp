#include "StarPointer.h"

#include "BuildStamp.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "GazePinch.h"
#include "DesktopMouse.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "IHandTracker.h"
#include "IXRTrackingSystem.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "MotionControllerComponent.h"
#include "ProceduralMeshComponent.h"
#include "StarActions.h"
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
	// The panel: its size, the text heights, and how long it takes to follow the head's
	// turns. Its place in front of the head is AStarPointer::PanelPlace, which its bar moves.
	constexpr double PanelWidth = 25.0;
	constexpr double PanelHeight = 12.0;
	constexpr double PanelPadding = 1.6;
	constexpr double FrameThickness = 0.12;
	// Text heights go to SetWorldSize, which takes a float.
	constexpr float TitleSize = 2.4f;
	constexpr float DetailsSize = 1.8f;
	constexpr float HintSize = 1.4f;
	constexpr double PanelFollowSeconds = 0.12;
	// The parts of the panel a ray can meet (AStarPointer::HitPanel).
	constexpr int32 PartNone = 0;
	constexpr int32 PartBody = 1;
	constexpr int32 PartBar = 2;
	constexpr int32 PartActions = 3;
	constexpr int32 PartHide = 4;
	/** The first item of the actions list; the others follow. */
	constexpr int32 PartItem = 10;
	// The grab bar under the panel, the "..." button above its right end, and the actions list
	// to its left, in the panel's units; and how far outside each a ray still counts.
	constexpr double BarWidth = 7.0;
	constexpr double BarHeight = 0.7;
	constexpr double BarGap = 1.2;
	constexpr double ButtonWidth = 3.6;
	constexpr double ButtonHeight = 2.4;
	constexpr double ButtonGap = 0.8;
	constexpr double MenuWidth = 15.0;
	constexpr double ItemHeight = 2.6;
	constexpr double MenuGap = 1.2;
	constexpr double MenuPadding = 0.8;
	constexpr double HitMargin = 0.6;
	// The button that hides the panel, left of the bar, as a visionOS window's close button is,
	// and how far from its centre a ray still counts.
	constexpr double HideRadius = 0.8;
	constexpr double HideGap = 1.6;
	constexpr double HideReach = 2.0;
	/** After a pinch on the panel, this long before the next: a pinch seen twice must not toggle twice. */
	constexpr double PanelGateSeconds = 0.4;
	/** How long a line shown by ShowNote stays. */
	constexpr double NoteSeconds = 6.0;
	/**
	 * The actions list and the hand menu: the commands that need no star's name, run through
	 * AStarActions. The last reads "Hide info" or "Show info", as the panel is (MenuLabel).
	 */
	const TCHAR* const MenuLabels[] = {TEXT("Go there"), TEXT("Back off"), TEXT("Closer"), TEXT("Return to the Sun"), TEXT("Let go"), TEXT("Hide info")};
	constexpr int32 MenuCount = UE_ARRAY_COUNT(MenuLabels);
	constexpr int32 InfoItem = MenuCount - 1;
	constexpr double MenuHeight = MenuCount * ItemHeight + 2.0 * MenuPadding;
	// Where they sit on the panel (right, up from its centre).
	constexpr double BarUp = -PanelHeight / 2.0 - BarGap - BarHeight / 2.0;
	constexpr double ButtonRight = PanelWidth / 2.0 - ButtonWidth / 2.0;
	constexpr double HideRight = -BarWidth / 2.0 - HideGap - HideRadius;
	constexpr double ButtonUp = PanelHeight / 2.0 + ButtonGap + ButtonHeight / 2.0;
	constexpr double MenuRight = -PanelWidth / 2.0 - MenuGap - MenuWidth / 2.0;
	constexpr double MenuTop = PanelHeight / 2.0;
	constexpr double ItemUp(int32 Item)
	{
		return MenuTop - MenuPadding - (Item + 0.5) * ItemHeight;
	}
	constexpr double LightYearsPerParsec = 3.26156;
	/** The size of a dot hidden beyond the panel: too small to see, never zero. */
	constexpr double HiddenDot = 1e-3;
	/** The ring under the cursor comes back this long after the view last moved. */
	constexpr double HoverAfterMoveSeconds = 0.5;
	/** How far the view turns as the mouse drags one pixel. */
	constexpr double LookDegreesPerPixel = 0.2;
	/** Turning up or down stops short of straight up or down, where the view would flip. */
	constexpr double MaxPitchDegrees = 88.0;
	/** Panning moves this fraction of the distance to the star kept for each pixel dragged. */
	constexpr double PanPerPixel = 0.002;
	/** Each notch of the wheel takes the view this fraction of its distance from the star kept. */
	constexpr double ZoomFactor = 0.85;
	/** Ctrl + middle-drag: notches of the wheel for each pixel dragged. */
	constexpr float ZoomNotchesPerPixel = 0.05f;
	/** Zooming stops this near a star: 0.005 pc, in Unreal units, beyond the near plane. */
	constexpr double SmallestStep = 1.5;
	/** A kept star further than this from straight ahead, across or up, gets directions on the panel. */
	constexpr double InViewDegrees = 25.0;
	/** In a headset the panel goes this long after it was last in use (a star pointed at or kept). */
	constexpr double InfoIdleSeconds = 3.0;

	// The hand menu, in centimetres. A button on the back of the wrist of the hand that does not
	// point, shown while the back of that hand is towards the head, opens and closes it, touched
	// with the pointing hand's index fingertip. (Not the palm: turned to the face, the palm brings
	// up visionOS's own Home button, which leaves the app.) The menu is a column of buttons
	// beside the wrist, towards the pointing hand, upright and facing the head; it stays where it
	// opened, travelling with the viewer, so the hand can drop while the other presses.
	constexpr double WristStandOff = 2.5;
	constexpr double WristFacingDegrees = 60.0;
	/** Only a hand this near the head, held up to look at, not one resting by the side. */
	constexpr double WristReach = 70.0;
	constexpr double WristRadius = 1.2;
	/** The fingertip lights the button within WristNear, touches it within WristTouch, and must go beyond WristRearm before the next touch. */
	constexpr double WristNear = 6.0;
	constexpr double WristTouch = 2.0;
	constexpr double WristRearm = 5.0;
	/** Within this of the menu, the pointing hand presses it rather than pointing. */
	constexpr double FingerAtMenu = 12.0;
	constexpr double HandButtonWidth = 9.0;
	constexpr double HandButtonHeight = 2.4;
	constexpr double HandButtonGap = 0.5;
	constexpr float HandTextSize = 0.8f;
	constexpr double HandTextPadding = 0.8;
	/** From the wrist button to the menu's centre: across by a gap and half the menu, and up alongside the hand. */
	constexpr double HandMenuAcross = 3.0 + HandButtonWidth / 2.0;
	constexpr double HandMenuRaise = 4.0;
	constexpr double HandMenuHeight = MenuCount * HandButtonHeight + (MenuCount - 1) * HandButtonGap;
	// The fingertip presses a button by passing through its plane, from in front of it, within
	// it (and PokeMargin around); it lights the button from PokeHover in front. Another press
	// needs the fingertip back in front by PokeRearm, and PokeGateSeconds since the last.
	constexpr double PokeHover = 5.0;
	constexpr double PokeBehind = 2.0;
	constexpr double PokeMargin = 0.5;
	constexpr double PokeRearm = 1.0;
	constexpr double PokeGateSeconds = 0.5;
	constexpr double PressedSeconds = 0.3;
	/** The fingertip's dot shows within this of the menu, in front or to the side; its diameter grows with the distance in front. */
	constexpr double FingerDotReach = 10.0;
	constexpr double FingerDotSmallest = 0.4;
	constexpr double FingerDotGrowth = 0.1;
	/** Up from the hand menu's centre to an item's. */
	constexpr double HandItemUp(int32 Item)
	{
		return HandMenuHeight / 2.0 - HandButtonHeight / 2.0 - Item * (HandButtonHeight + HandButtonGap);
	}
	// Under the hand menu, the bar that moves it and, to its left, the button that closes it, as
	// under the panel; and how far from each a look or the fingertip still counts.
	constexpr double HandBarWidth = 4.0;
	constexpr double HandBarHeight = 0.4;
	constexpr double HandBarGap = 1.0;
	constexpr double HandBarReach = 0.8;
	constexpr double HandHideRadius = 0.55;
	constexpr double HandHideGap = 0.8;
	constexpr double HandHideReach = 1.1;
	constexpr double HandBarUp = -HandMenuHeight / 2.0 - HandBarGap - HandBarHeight / 2.0;
	constexpr double HandHideRight = -HandBarWidth / 2.0 - HandHideGap - HandHideRadius;
	/** The hand menu's parts after its items (AStarPointer::HandMenuPartAt). */
	constexpr int32 HandPartBar = MenuCount;
	constexpr int32 HandPartHide = MenuCount + 1;

	// What a look-and-pinch moves with the hand, which takes it further than the hand goes by
	// as much as it is further from the head, but never nearer or further than these.
	constexpr int32 MoveNone = 0;
	constexpr int32 MovePanel = 1;
	constexpr int32 MoveHandMenu = 2;
	constexpr double PanelNearest = 40.0;
	constexpr double PanelFurthest = 250.0;
	constexpr double HandMenuNearest = 20.0;
	constexpr double HandMenuFurthest = 80.0;
	constexpr double GazeEchoSeconds = 0.15;

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
	const FLinearColor BarColour(0.4f, 0.45f, 0.5f);

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

	/** A cross, each arm Arm long from the centre, Thickness wide, in the panel's plane, each triangle in both windings. */
	void BuildCross(UProceduralMeshComponent* Mesh, double Arm, double Thickness)
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		const double Across = Thickness / 2.0 / UE_SQRT_2;
		for (const double Slope : {1.0, -1.0})
		{
			// From bottom left to top right, then from top left to bottom right.
			const FVector2D From(-Arm / UE_SQRT_2, -Slope * Arm / UE_SQRT_2);
			const FVector2D Side(-Slope * Across, Across);
			const int32 A = Vertices.Num();
			Vertices.Append({OnPanel(From.X - Side.X, From.Y - Side.Y), OnPanel(-From.X - Side.X, -From.Y - Side.Y), OnPanel(-From.X + Side.X, -From.Y + Side.Y),
				OnPanel(From.X + Side.X, From.Y + Side.Y)});
			Triangles.Append({A, A + 1, A + 2, A, A + 2, A + 3});
			Triangles.Append({A, A + 2, A + 1, A, A + 3, A + 2});
		}
		const TArray<FVector> NoNormals;
		const TArray<FVector2D> NoUVs;
		const TArray<FColor> NoColours;
		const TArray<FProcMeshTangent> NoTangents;
		Mesh->CreateMeshSection(0, Vertices, Triangles, NoNormals, NoUVs, NoColours, NoTangents, false);
		Mesh->SetCastShadow(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	/** A filled rectangle Width by Height about the origin in the panel's plane, each triangle in both windings. */
	void BuildQuad(UProceduralMeshComponent* Mesh, double Width, double Height)
	{
		const TArray<FVector> Vertices = {OnPanel(-Width / 2.0, -Height / 2.0), OnPanel(Width / 2.0, -Height / 2.0), OnPanel(Width / 2.0, Height / 2.0),
			OnPanel(-Width / 2.0, Height / 2.0)};
		const TArray<int32> Triangles = {0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2};
		const TArray<FVector> NoNormals;
		const TArray<FVector2D> NoUVs;
		const TArray<FColor> NoColours;
		const TArray<FProcMeshTangent> NoTangents;
		Mesh->CreateMeshSection(0, Vertices, Triangles, NoNormals, NoUVs, NoColours, NoTangents, false);
		Mesh->SetCastShadow(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	// Above the frame, level with its left edge, its baseline a padding's height above it.
	VoiceLine = CreateDefaultSubobject<UTextRenderComponent>(TEXT("VoiceLine"));
	VoiceLine->SetupAttachment(Panel);
	VoiceLine->SetRelativeLocation(OnPanel(-PanelWidth / 2.0, PanelHeight / 2.0 + PanelPadding / 2.0));
	VoiceLine->SetWorldSize(DetailsSize);
	// Empty until there is something to say: a new text component reads "Text".
	VoiceLine->SetText(FText::GetEmpty());
	for (UTextRenderComponent* Text : {Title.Get(), Details.Get(), Hint.Get(), VoiceLine.Get()})
	{
		Text->SetHorizontalAlignment(EHTA_Left);
		Text->SetVerticalAlignment(EVRTA_TextTop);
	}
	VoiceLine->SetVerticalAlignment(EVRTA_TextBottom);

	GrabBar = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GrabBar"));
	GrabBar->SetupAttachment(Panel);
	GrabBar->SetRelativeLocation(OnPanel(0.0, BarUp));
	HideRing = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HideRing"));
	HideRing->SetupAttachment(Panel);
	HideRing->SetRelativeLocation(OnPanel(HideRight, BarUp));
	HideRing->SetRelativeScale3D(FVector(HideRadius));
	HideCross = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HideCross"));
	HideCross->SetupAttachment(Panel);
	HideCross->SetRelativeLocation(OnPanel(HideRight, BarUp));
	ActionsFrame = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ActionsFrame"));
	ActionsFrame->SetupAttachment(Panel);
	ActionsFrame->SetRelativeLocation(OnPanel(ButtonRight, ButtonUp));
	ActionsText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ActionsText"));
	ActionsText->SetupAttachment(Panel);
	ActionsText->SetRelativeLocation(OnPanel(ButtonRight, ButtonUp));
	ActionsText->SetWorldSize(TitleSize);
	ActionsText->SetText(FText::FromString(TEXT("...")));
	ActionsText->SetHorizontalAlignment(EHTA_Center);
	ActionsText->SetVerticalAlignment(EVRTA_TextCenter);
	MenuFrame = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MenuFrame"));
	MenuFrame->SetupAttachment(Panel);
	MenuFrame->SetRelativeLocation(OnPanel(MenuRight, MenuTop - MenuHeight / 2.0));
	for (int32 Item = 0; Item < MenuCount; ++Item)
	{
		UTextRenderComponent* Text = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("MenuItem%d"), Item));
		Text->SetupAttachment(Panel);
		Text->SetRelativeLocation(OnPanel(MenuRight - MenuWidth / 2.0 + PanelPadding, ItemUp(Item)));
		Text->SetWorldSize(DetailsSize);
		Text->SetText(FText::FromString(MenuLabels[Item]));
		Text->SetHorizontalAlignment(EHTA_Left);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		MenuItems.Add(Text);
	}

	HandMenu = CreateDefaultSubobject<USceneComponent>(TEXT("HandMenu"));
	HandMenu->SetupAttachment(RootComponent);
	WristButton = CreateDefaultSubobject<USceneComponent>(TEXT("WristButton"));
	WristButton->SetupAttachment(RootComponent);
	WristRing = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WristRing"));
	WristRing->SetupAttachment(WristButton);
	WristRing->SetRelativeScale3D(FVector(WristRadius));
	WristText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("WristText"));
	WristText->SetupAttachment(WristButton);
	WristText->SetWorldSize(HandTextSize);
	WristText->SetText(FText::FromString(TEXT("...")));
	WristText->SetHorizontalAlignment(EHTA_Center);
	WristText->SetVerticalAlignment(EVRTA_TextCenter);
	WristButton->SetVisibility(false, true);
	FingerDot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FingerDot"));
	FingerDot->SetupAttachment(HandMenu);
	FingerDot->SetCastShadow(false);
	FingerDot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HandMenu->SetVisibility(false, true);
	for (int32 Item = 0; Item < MenuCount; ++Item)
	{
		UProceduralMeshComponent* Button = CreateDefaultSubobject<UProceduralMeshComponent>(*FString::Printf(TEXT("HandButton%d"), Item));
		Button->SetupAttachment(HandMenu);
		Button->SetRelativeLocation(OnPanel(0.0, HandItemUp(Item)));
		HandFrames.Add(Button);
		UTextRenderComponent* Text = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("HandItem%d"), Item));
		Text->SetupAttachment(HandMenu);
		Text->SetRelativeLocation(OnPanel(-HandButtonWidth / 2.0 + HandTextPadding, HandItemUp(Item)));
		Text->SetWorldSize(HandTextSize);
		Text->SetText(FText::FromString(MenuLabels[Item]));
		Text->SetHorizontalAlignment(EHTA_Left);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		HandItems.Add(Text);
	}
	HandBar = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HandBar"));
	HandBar->SetupAttachment(HandMenu);
	HandBar->SetRelativeLocation(OnPanel(0.0, HandBarUp));
	HandHideRing = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HandHideRing"));
	HandHideRing->SetupAttachment(HandMenu);
	HandHideRing->SetRelativeLocation(OnPanel(HandHideRight, HandBarUp));
	HandHideRing->SetRelativeScale3D(FVector(HandHideRadius));
	HandHideCross = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HandHideCross"));
	HandHideCross->SetupAttachment(HandMenu);
	HandHideCross->SetRelativeLocation(OnPanel(HandHideRight, HandBarUp));
}

void AStarPointer::BeginPlay()
{
	Super::BeginPlay();
	bHeadset = GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed();

	BuildRing(HoverRing);
	BuildRing(SelectionRing);
	BuildRing(WristRing);
	WristMaterial = ColouredMaterial(FrameColour);
	WristRing->SetMaterial(0, WristMaterial);
	HoverRing->SetMaterial(0, ColouredMaterial(HoverColour));
	SelectionRing->SetMaterial(0, ColouredMaterial(SelectionColour));
	BuildFrame(Frame, PanelWidth, PanelHeight, FrameThickness);
	Frame->SetMaterial(0, ColouredMaterial(FrameColour));
	// The bar, the button and the list are for the headset's ray; the desktop has the console.
	BuildQuad(GrabBar, BarWidth, BarHeight);
	GrabBarMaterial = ColouredMaterial(BarColour);
	GrabBar->SetMaterial(0, GrabBarMaterial);
	BuildRing(HideRing);
	BuildCross(HideCross, HideRadius * 0.45, FrameThickness);
	HideMaterial = ColouredMaterial(FrameColour);
	HideRing->SetMaterial(0, HideMaterial);
	HideCross->SetMaterial(0, HideMaterial);
	BuildFrame(ActionsFrame, ButtonWidth, ButtonHeight, FrameThickness);
	ActionsFrame->SetMaterial(0, ColouredMaterial(FrameColour));
	BuildFrame(MenuFrame, MenuWidth, MenuHeight, FrameThickness);
	MenuFrame->SetMaterial(0, ColouredMaterial(FrameColour));
	// Thinner than the panel's frame, as the menu is nearer: about as thick to the eye.
	for (UProceduralMeshComponent* Button : HandFrames)
	{
		BuildFrame(Button, HandButtonWidth, HandButtonHeight, FrameThickness / 2.0);
		UMaterialInstanceDynamic* Material = ColouredMaterial(FrameColour);
		Button->SetMaterial(0, Material);
		HandFrameMaterials.Add(Material);
	}
	BuildQuad(HandBar, HandBarWidth, HandBarHeight);
	HandBarMaterial = ColouredMaterial(BarColour);
	HandBar->SetMaterial(0, HandBarMaterial);
	BuildRing(HandHideRing);
	BuildCross(HandHideCross, HandHideRadius * 0.45, FrameThickness / 2.0);
	HandHideMaterial = ColouredMaterial(FrameColour);
	HandHideRing->SetMaterial(0, HandHideMaterial);
	HandHideCross->SetMaterial(0, HandHideMaterial);
	if (UMaterialInterface* Text = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/M_Label.M_Label")))
	{
		Title->SetTextMaterial(Text);
		Details->SetTextMaterial(Text);
		Hint->SetTextMaterial(Text);
		VoiceLine->SetTextMaterial(Text);
		ActionsText->SetTextMaterial(Text);
		for (UTextRenderComponent* Item : MenuItems)
		{
			Item->SetTextMaterial(Text);
		}
		for (UTextRenderComponent* Item : HandItems)
		{
			Item->SetTextMaterial(Text);
		}
		WristText->SetTextMaterial(Text);
	}
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Dots->SetStaticMesh(Sphere);
	Dots->SetMaterial(0, ColouredMaterial(DotColour));
	FingerDot->SetStaticMesh(Sphere);
	FingerDotMaterial = ColouredMaterial(DotColour);
	FingerDot->SetMaterial(0, FingerDotMaterial);
	for (int32 Dot = 0; Dot < DotCount; ++Dot)
	{
		Dots->AddInstance(FTransform::Identity, true);
	}

	// The desktop's cursor always points; there is nothing to turn on.
	bPointingStarted = !bHeadset;
	if (!bHeadset && FSlateApplication::IsInitialized())
	{
		Mouse = MakeShared<FDesktopMouse>();
		FSlateApplication::Get().RegisterInputPreProcessor(Mouse);
	}
	FParse::Value(FCommandLine::Get(), TEXT("PointingHand="), PointingHand);
	FParse::Value(FCommandLine::Get(), TEXT("GestureGate="), GestureGateSeconds);
	FParse::Value(FCommandLine::Get(), TEXT("DoublePinch="), DoublePinchSeconds);
	FParse::Value(FCommandLine::Get(), TEXT("RightTriggerKey="), RightTriggerKey);
	FParse::Value(FCommandLine::Get(), TEXT("LeftTriggerKey="), LeftTriggerKey);
	bLogInput = FParse::Param(FCommandLine::Get(), TEXT("LogInput"));
	bShowControls = FParse::Param(FCommandLine::Get(), TEXT("PanelControls"));
	bHandRay |= FParse::Param(FCommandLine::Get(), TEXT("HandRay"));
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
	UE_LOG(LogStarPointer, Display, TEXT("Pointing with hand %d%s; pinches ignored for %.2f s after a star is kept or let go; two within %.2f s let it go"), Pointer,
		bHandRay ? TEXT("") : TEXT(" (controllers only: hands look and pinch)"), GestureGateSeconds, DoublePinchSeconds);

#if PLATFORM_WINDOWS
	// With -LogInput, list the controller input keys the engine registered, so a new controller's
	// exact trigger name can be seen and set in Config/Windows/WindowsInput.ini.
	if (bLogInput)
	{
		TArray<FKey> AllKeys;
		EKeys::GetAllKeys(AllKeys);
		for (const FKey& Key : AllKeys)
		{
			const FString KeyName = Key.ToString();
			if (KeyName.Contains(TEXT("Trigger")) || KeyName.Contains(TEXT("MotionController")))
			{
				UE_LOG(LogStarPointer, Display, TEXT("Available input key: %s"), *KeyName);
			}
		}
	}
#endif
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
		// The ray under the cursor, when it is over the view; clicks are DesktopMouse's.
		const APlayerController* Player = GetWorld()->GetFirstPlayerController();
		FVector Origin;
		FVector Direction;
		if (Player != nullptr && FDesktopMouse::IsCursorOverView() && Player->DeprojectMousePositionToWorld(Origin, Direction))
		{
			Rays.Add({Head, Direction, false, MouseHand, PinchEnd});
		}
		return Rays;
	}
	// Set again below for each hand tracked as a hand this frame.
	for (FHand& State : Hands)
	{
		State.bHandTracked = false;
	}
	// Prefer motion controllers when one is tracked (the Vive); otherwise hand tracking (the Vision
	// Pro). The Vive's runtime reports a valid hand tracker even with controllers in use, so the
	// controller check comes first. Gated to Windows because on the Vision Pro the aim pose comes
	// from the hands and must not be mistaken for a controller.
#if PLATFORM_WINDOWS
	const bool bController = AnyControllerTracked();
#else
	const bool bController = false;
#endif
	const IHandTracker* Tracker = FindHandTracker();
	const int32 Mode = bController ? 2 : (Tracker != nullptr ? 1 : 0);
	if (Mode != LoggedInputMode)
	{
		UE_LOG(LogStarPointer, Display, TEXT("Headset input: controller tracked=%d, hand tracker=%d -> %s"), bController, Tracker != nullptr,
			bController ? TEXT("controllers") : (Tracker != nullptr ? TEXT("hands") : TEXT("none")));
		LoggedInputMode = Mode;
	}
	if (bController)
	{
		AppendControllerRays(Rays);
		return Rays;
	}
	if (Tracker == nullptr)
	{
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
		// For the hand menu: out of the palm is across the knuckles, little to index, crossed
		// with up the hand, wrist to middle knuckle; the other way round for the right hand.
		State.bHandTracked = true;
		State.IndexTip = Joints[int32(EHandKeypoint::IndexTip)];
		State.Palm = Through;
		State.Gap = Gap;
		const FVector& Wrist = Joints[int32(EHandKeypoint::Wrist)];
		State.Wrist = Wrist;
		const FVector& Middle = Joints[int32(EHandKeypoint::MiddleProximal)];
		const FVector& Index = Joints[int32(EHandKeypoint::IndexProximal)];
		const FVector& Little = Joints[int32(EHandKeypoint::LittleProximal)];
		const bool bJoints = !Wrist.IsNearlyZero() && !Middle.IsNearlyZero() && !Index.IsNearlyZero() && !Little.IsNearlyZero();
		State.PalmNormal = bJoints ? (FVector::CrossProduct(Middle - Wrist, Little - Index) * (Hand == EControllerHand::Left ? 1.0 : -1.0)).GetSafeNormal()
								   : FVector::ZeroVector;
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

bool AStarPointer::AnyControllerTracked() const
{
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	const AStarViewerPawn* Viewer = Player ? Cast<AStarViewerPawn>(Player->GetPawn()) : nullptr;
	if (Viewer == nullptr)
	{
		return false;
	}
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		const UMotionControllerComponent* Controller = Viewer->GetHandController(Hand);
		if (Controller != nullptr && Controller->IsTracked())
		{
			return true;
		}
	}
	return false;
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
	const bool bLog = bLogInput && Now >= NextControllerLog;
	// Point with a tracked controller: the configured hand if its controller is tracked, otherwise
	// the other, so a single controller works whichever hand holds it (and a sleeping one is not
	// pointed with). Derive the preference from PointingHand, which is stable, not from Pointer,
	// which this reassigns.
	const int32 Configured = PointingHand.Equals(TEXT("Left"), ESearchCase::IgnoreCase) ? 0 : 1;
	const UMotionControllerComponent* ConfiguredController = Viewer->GetHandController(Configured);
	const UMotionControllerComponent* OtherController = Viewer->GetHandController(1 - Configured);
	if (ConfiguredController != nullptr && ConfiguredController->IsTracked())
	{
		Pointer = Configured;
	}
	else if (OtherController != nullptr && OtherController->IsTracked())
	{
		Pointer = 1 - Configured;
	}
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
		// The trigger key is device-specific, so poll each candidate and take the largest value.
		const FString& TriggerKeys = HandIndex == 0 ? LeftTriggerKey : RightTriggerKey;
		TArray<FString> KeyNames;
		TriggerKeys.ParseIntoArray(KeyNames, TEXT(","));
		float Trigger = 0.0f;
		for (const FString& KeyName : KeyNames)
		{
			Trigger = FMath::Max(Trigger, Player->GetInputAnalogKeyState(FKey(*KeyName.TrimStartAndEnd())));
		}
		// Hysteresis around the press threshold; State.bPinched carries between frames.
		State.bPinched = State.bPinched ? Trigger > TriggerRelease : Trigger > TriggerPress;
		if (bLog)
		{
			UE_LOG(LogStarPointer, Display, TEXT("Controller hand %d: trigger [%s] = %.2f, pinched %d"), HandIndex, *TriggerKeys, Trigger, State.bPinched);
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
	UpdateHandMenu(Head, HeadRotation, DeltaSeconds);
	UseGazePinches(Head);
	Hovered = INDEX_NONE;
	Aimed = PartNone;
	int32 Pinched = 0;
	bool bPointing = false;
	bool bAimingPanel = false;
	for (const FRay& Ray : Rays)
	{
		FHand& State = Hands[Ray.Hand];
		Pinched += Ray.bPinched ? 1 : 0;
		// The other hand only counts towards the gesture that quits; and at the hand menu or its
		// wrist button the pointing hand presses them, so it neither points nor pinches. With no
		// hand ray, neither hand points: look and pinch does.
		if (Ray.Hand != Pointer || bFingerAtMenu || IsLookOnly())
		{
			State.bWasPinched = Ray.bPinched;
			continue;
		}
		bPointing = true;
		const int32 Star = StarField->FindStar(Ray.Origin, Ray.Direction, MaxPointingAngleDegrees);
		// In a headset the ray may meet the panel, its bar, its button or its list first.
		double Along = 0.0;
		const int32 Part = bHeadset && bPointingStarted ? HitPanel(Ray.Origin, Ray.Direction, Along) : PartNone;
		Aimed = Part;
		AimedAlong = Along;
		State.Recent.Add({Now, Star, Ray.Gap, Part, Along});
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
			int32 PartBefore = State.Recent[0].Part;
			double AlongBefore = State.Recent[0].Along;
			for (const FSighting& Sighting : State.Recent)
			{
				if (Sighting.Gap >= Widest - PinchOnset)
				{
					Before = Sighting.Star;
					PartBefore = Sighting.Part;
					AlongBefore = Sighting.Along;
				}
			}
			UE_LOG(LogStarPointer, Display, TEXT("Pinch with hand %d, thumb to index %.1f cm (widest %.1f): pointing at %d now, %d before"), Ray.Hand, Ray.Gap,
				Widest, Star, Before);
			if ((PartBefore != PartNone || bMenuOpen) && Now >= GateUntil)
			{
				// The panel's, not a star's: pinching off the panel with the list open closes it.
				UE_LOG(LogStarPointer, Display, TEXT("Pinch on the panel, part %d"), PartBefore);
				if (PartBefore != PartNone)
				{
					PressPanel(PartBefore, Ray.Origin, Ray.Direction, AlongBefore);
				}
				else
				{
					bMenuOpen = false;
				}
				GateUntil = Now + PanelGateSeconds;
			}
			else if (Now < GateUntil)
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

		// Carrying the panel by its bar: it goes where the ray goes, at the distance grabbed,
		// and stays in that place in the view when let go.
		if (bCarrying)
		{
			if (Ray.bPinched)
			{
				PanelPlace = PanelRotation.UnrotateVector(Ray.Origin + Ray.Direction * CarryAlong + CarryGrip - Head);
			}
			else
			{
				bCarrying = false;
				UE_LOG(LogStarPointer, Display, TEXT("Panel put down at %s (forward, right, up from the head)"), *PanelPlace.ToString());
			}
		}

		// A kept star holds the panel, and in a headset the ray goes until the star is let go,
		// except over the panel, which it then stops at; on the desktop the cursor still rings
		// the star under it.
		bAimingPanel = Part != PartNone || bCarrying;
		if (bPointingStarted && (Selected == INDEX_NONE || !bHeadset || bAimingPanel))
		{
			if (!bAimingPanel)
			{
				Hovered = Star;
			}
			const double Reach = bCarrying ? CarryAlong : bAimingPanel ? Along : LastDot;
			for (int32 Dot = 0; Dot < DotCount; ++Dot)
			{
				const double Distance = FirstDot * FMath::Pow(LastDot / FirstDot, double(Dot) / (DotCount - 1));
				// Beyond the panel a dot is hidden by shrinking it, never to nothing: a zero scale
				// cannot be inverted, and one such instance hid every dot of the ray. The slack lets
				// the last dot of an ordinary ray, which rounding puts a hair past LastDot, show.
				const bool bShown = Distance <= Reach * (1.0 + 1e-6);
				const double Diameter = bShown ? FMath::Max(SmallestDot, Distance * DotAngularDiameter) : HiddenDot;
				// The basic sphere is 100 units across.
				Dots->UpdateInstanceTransform(Dot, FTransform(FQuat::Identity, Ray.Origin + Ray.Direction * Distance, FVector(Diameter / 100.0)),
					true, Dot == DotCount - 1, true);
			}
		}
	}
	Dots->SetVisibility(bHeadset && bPointing && bPointingStarted && (Selected == INDEX_NONE || bAimingPanel));
	if (Mouse.IsValid() || !Queued.IsEmpty())
	{
		UseMouse(Head, HeadRotation);
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

	// While the view is being moved the star under the cursor is beside the point: no ring.
	const bool bMovingView = Now - LastViewMove < HoverAfterMoveSeconds;
	PlaceRing(HoverRing, Hovered != Selected && !bMovingView ? Hovered : INDEX_NONE, Head);
	PlaceRing(SelectionRing, Selected, Head);
	PlacePanel(Head, HeadRotation, DeltaSeconds);
	if (NoteUntil > 0.0 && Now > NoteUntil)
	{
		SetVoiceLine(FString(), false);
		NoteUntil = 0.0;
	}
}

int32 AStarPointer::HitPanel(const FVector& Origin, const FVector& Direction, double& Along) const
{
	// In the panel's own frame, where it is the plane X = 0, facing +X, right along -Y (OnPanel).
	const FTransform& Transform = Panel->GetComponentTransform();
	const FVector From = Transform.InverseTransformPosition(Origin);
	const FVector Towards = Transform.InverseTransformVectorNoScale(Direction);
	if (FMath::Abs(Towards.X) < UE_KINDA_SMALL_NUMBER)
	{
		return PartNone;
	}
	const double At = -From.X / Towards.X;
	if (At <= 0.0)
	{
		return PartNone;
	}
	const FVector Hit = From + Towards * At;
	const double Right = -Hit.Y;
	const double Up = Hit.Z;
	const auto Within = [Right, Up](double CentreRight, double CentreUp, double Width, double Height, double Margin) {
		return FMath::Abs(Right - CentreRight) <= Width / 2.0 + Margin && FMath::Abs(Up - CentreUp) <= Height / 2.0 + Margin;
	};
	Along = At;
	if (bMenuOpen)
	{
		for (int32 Item = 0; Item < MenuCount; ++Item)
		{
			if (Within(MenuRight, ItemUp(Item), MenuWidth, ItemHeight, 0.0))
			{
				return PartItem + Item;
			}
		}
		if (Within(MenuRight, MenuTop - MenuHeight / 2.0, MenuWidth, MenuHeight, HitMargin))
		{
			return PartBody;
		}
	}
	if (ActionsFrame->IsVisible() && Within(ButtonRight, ButtonUp, ButtonWidth, ButtonHeight, HitMargin))
	{
		return PartActions;
	}
	// Hidden, the panel and its bar let the ray through to the stars.
	if (!bInfoVisible)
	{
		return PartNone;
	}
	// The bar is thin: it counts for more than its height, as a visionOS window's does.
	if (Within(0.0, BarUp, BarWidth, BarHeight * 3.0, HitMargin))
	{
		return PartBar;
	}
	if (HideRing->IsVisible() && Within(HideRight, BarUp, 0.0, 0.0, HideReach))
	{
		return PartHide;
	}
	return Within(0.0, 0.0, PanelWidth, PanelHeight, 0.0) ? PartBody : PartNone;
}

void AStarPointer::PressPanel(int32 Part, const FVector& Origin, const FVector& Direction, double Along)
{
	if (Part == PartBar)
	{
		bCarrying = true;
		CarryAlong = Along;
		CarryGrip = Panel->GetComponentLocation() - (Origin + Direction * Along);
	}
	else if (Part == PartActions)
	{
		bMenuOpen = !bMenuOpen;
	}
	else if (Part == PartHide)
	{
		RunMenuItem(InfoItem);
	}
	else if (Part >= PartItem && Part < PartItem + MenuCount)
	{
		bMenuOpen = false;
		RunMenuItem(Part - PartItem);
	}
}

FString AStarPointer::MenuLabel(int32 Item) const
{
	return Item == InfoItem ? FString(bInfoShown ? TEXT("Hide info") : TEXT("Show info")) : FString(MenuLabels[Item]);
}

void AStarPointer::RunMenuItem(int32 Item)
{
	const FString Command = MenuLabel(Item);
	const FString Result = Actions != nullptr ? Actions->Run(Command) : FString(TEXT("No actions"));
	UE_LOG(LogStarPointer, Display, TEXT("Menu: \"%s\": %s"), *Command, *Result);
	ShowNote(Result);
}

void AStarPointer::SetInfoShown(bool bShown)
{
	bInfoShown = bShown;
	// Shown, it stays for a while even with nothing pointed at, to be seen to be back.
	LastInUse = GetWorld()->GetRealTimeSeconds();
	UE_LOG(LogStarPointer, Display, TEXT("Info %s"), bShown ? TEXT("shown") : TEXT("hidden"));
}

void AStarPointer::UpdateHandMenu(const FVector& Head, const FRotator& HeadRotation, float DeltaSeconds)
{
	const double Now = GetWorld()->GetRealTimeSeconds();
	const FQuat Facing = HeadRotation.Quaternion();
	// Beside the wrist, towards the pointing hand: to its right for a left hand.
	const double Side = Pointer == 0 ? -1.0 : 1.0;
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = Player != nullptr ? Player->GetPawn() : nullptr;
	const FVector Base = Pawn != nullptr ? Pawn->GetActorLocation() : FVector::ZeroVector;
	const bool bHands = bHeadset && Pointer != MouseHand && !bUsingControllers;
	const FHand& Other = Hands[bHands ? 1 - Pointer : 0];
	const FHand& Finger = Hands[bHands ? Pointer : 1];
	bFingerAtMenu = false;
	// Upright to the world in a headset, not to the head, which tilts to look down at the wrist
	// and would tilt the menu with it; on the desktop, to the view, which may be rolled.
	const FVector Upright = bHeadset ? FVector::UpVector : Facing.GetUpVector();

	// The wrist button, standing off the back of the wrist, while the back of the hand is towards the head.
	bool bButton = false;
	FVector ButtonPlace = FVector::ZeroVector;
	if (bHands && Other.bHandTracked && !Other.PalmNormal.IsZero() && !Other.Wrist.IsNearlyZero())
	{
		ButtonPlace = Other.Wrist - Other.PalmNormal * WristStandOff;
		const FVector ToHead = Head - ButtonPlace;
		bButton = ToHead.Size() < WristReach &&
			FVector::DotProduct(-Other.PalmNormal, ToHead.GetSafeNormal()) >= FMath::Cos(FMath::DegreesToRadians(WristFacingDegrees));
	}
	else if (bShowControls && !bHeadset)
	{
		// To check the layout on the desktop: where a left wrist held up would put them.
		bButton = true;
		ButtonPlace = Head + Facing.RotateVector(FVector(45.0, -20.0 - HandMenuAcross, -4.0 - HandMenuRaise));
	}
	WristButton->SetVisibility(bButton, true);
	bool bWristAimed = false;
	if (bButton)
	{
		WristButton->SetWorldLocationAndRotation(ButtonPlace, FRotationMatrix::MakeFromXZ(Head - ButtonPlace, Upright).ToQuat());
		if (bHeadset && Finger.bHandTracked)
		{
			const double Distance = FVector::Distance(Finger.IndexTip, ButtonPlace);
			bWristAimed = Distance < WristNear;
			bFingerAtMenu |= bWristAimed;
			if (bWristArmed && Distance < WristTouch && Now >= PokeGateUntil)
			{
				bWristArmed = false;
				PokeGateUntil = Now + PokeGateSeconds;
				if (bHandMenuShown)
				{
					CloseHandMenu();
				}
				else
				{
					// Fixed where it opens, relative to the viewer, who may travel with it open.
					// Across as the head sees it, level, and up to the world.
					const FVector Across = FRotationMatrix::MakeFromXZ(ButtonPlace - Head, Upright).GetUnitAxis(EAxis::Y);
					const FVector Place = ButtonPlace + Across * Side * HandMenuAcross + Upright * HandMenuRaise;
					HandMenuFromViewer = Place - Base;
					HandMenuRotation = FRotationMatrix::MakeFromXZ(Head - Place, Upright).ToQuat();
					bHandMenuShown = true;
					bPokeArmed = false;
					FingerNearest = TNumericLimits<double>::Max();
					FingerFrames = 0;
					UE_LOG(LogStarPointer, Display, TEXT("Hand menu opened, %.0f cm from the head"), FVector::Distance(Head, Place));
				}
			}
			bWristArmed |= Distance > WristRearm;
		}
		else
		{
			bWristArmed = false;
		}
	}
	else
	{
		// Shown again, the fingertip may already be on it: that is not a touch.
		bWristArmed = false;
	}
	if (WristMaterial != nullptr)
	{
		WristMaterial->SetVectorParameterValue(TEXT("Colour"), (bWristAimed || bHandMenuShown ? SelectionColour : FrameColour) * OverlayBrightness);
	}
	WristText->SetTextRenderColor(((bWristAimed || bHandMenuShown ? SelectionColour : DetailsColour) * OverlayBrightness).ToFColor(false));

	// Controllers came, or the hands went to the desktop: no menu to press.
	if (bHandMenuShown && !bHands)
	{
		CloseHandMenu();
	}
	const bool bShow = bHandMenuShown || (bShowControls && !bHeadset);
	HandMenu->SetVisibility(bShow, true);
	FingerDot->SetVisibility(false);
	HandAimed = INDEX_NONE;
	if (!bShow)
	{
		return;
	}
	if (bHandMenuShown)
	{
		HandMenu->SetWorldLocationAndRotation(Base + HandMenuFromViewer, HandMenuRotation);
	}
	else
	{
		const FVector Place = Head + Facing.RotateVector(FVector(45.0, -20.0, -4.0));
		HandMenu->SetWorldLocationAndRotation(Place, FRotationMatrix::MakeFromXZ(Head - Place, Facing.GetUpVector()).ToQuat());
	}

	// The pointing finger: in the menu's frame, in front (towards the head) is +X, right is -Y.
	if (bHeadset && Finger.bHandTracked)
	{
		const FVector Tip = HandMenu->GetComponentTransform().InverseTransformPosition(Finger.IndexTip);
		const double Depth = Tip.X;
		const double Right = -Tip.Y;
		const double Up = Tip.Z;
		const int32 Under = HandMenuPartAt(Right, Up, PokeMargin);
		if (Under != INDEX_NONE && Depth <= PokeHover && Depth >= -PokeBehind)
		{
			HandAimed = Under;
		}
		// How far the fingertip is from the menu's buttons, for the log.
		const double Across = FMath::Max(FMath::Abs(Right) - HandButtonWidth / 2.0, 0.0);
		const double Along = FMath::Max(FMath::Max(Up - HandMenuHeight / 2.0, HandBarUp - Up), 0.0);
		const double Nearest = FMath::Sqrt(Depth * Depth + Across * Across + Along * Along);
		++FingerFrames;
		bFingerAtMenu |= Nearest < FingerAtMenu;
		if (Nearest < FingerNearest)
		{
			FingerNearest = Nearest;
			FingerNearestAt = FVector(Depth, Right, Up);
		}
		// The dot under the fingertip, on the menu's plane, a little in front of it so the frames do not hide it.
		if (Across <= FingerDotReach && Along <= FingerDotReach && Depth >= -PokeBehind && Depth <= FingerDotReach)
		{
			const double Diameter = FingerDotSmallest + FingerDotGrowth * FMath::Max(Depth, 0.0);
			FingerDot->SetRelativeLocation(FVector(0.05, Tip.Y, Tip.Z));
			// The basic sphere is 100 units across.
			FingerDot->SetRelativeScale3D(FVector(Diameter / 100.0));
			FingerDot->SetVisibility(true);
			if (FingerDotMaterial != nullptr)
			{
				FingerDotMaterial->SetVectorParameterValue(TEXT("Colour"), (HandAimed != INDEX_NONE ? SelectionColour : DotColour) * OverlayBrightness);
			}
		}
		// The bar is for moving the menu by look-and-pinch: a poke on it does nothing.
		if (bPokeArmed && PokeDepth > 0.0 && Depth <= 0.0 && Depth >= -PokeBehind && Under != INDEX_NONE && Under != HandPartBar && Now >= PokeGateUntil)
		{
			UE_LOG(LogStarPointer, Display, TEXT("Hand menu pressed: item %d"), Under);
			bPokeArmed = false;
			PokeGateUntil = Now + PokeGateSeconds;
			if (Under == HandPartHide)
			{
				CloseHandMenu();
			}
			else
			{
				HandPressed = Under;
				HandPressedUntil = Now + PressedSeconds;
				RunMenuItem(Under);
			}
		}
		bPokeArmed |= Depth > PokeRearm;
		PokeDepth = Depth;
	}
	else
	{
		// Seen again, the fingertip may already be behind the menu: that is not a press.
		bPokeArmed = false;
	}
	for (int32 Item = 0; Item < MenuCount; ++Item)
	{
		const bool bPressed = Item == HandPressed && Now < HandPressedUntil;
		HandItems[Item]->SetText(FText::FromString(MenuLabel(Item)));
		HandItems[Item]->SetTextRenderColor(((Item == HandAimed || bPressed ? SelectionColour : DetailsColour) * OverlayBrightness).ToFColor(false));
		if (HandFrameMaterials.IsValidIndex(Item) && HandFrameMaterials[Item] != nullptr)
		{
			HandFrameMaterials[Item]->SetVectorParameterValue(TEXT("Colour"), (bPressed ? SelectionColour : FrameColour) * OverlayBrightness);
		}
	}
	if (HandBarMaterial != nullptr)
	{
		HandBarMaterial->SetVectorParameterValue(TEXT("Colour"), (HandAimed == HandPartBar || GazeMoving == MoveHandMenu ? SelectionColour : BarColour) * OverlayBrightness);
	}
	if (HandHideMaterial != nullptr)
	{
		HandHideMaterial->SetVectorParameterValue(TEXT("Colour"), (HandAimed == HandPartHide ? SelectionColour : FrameColour) * OverlayBrightness);
	}
}

int32 AStarPointer::HandMenuPartAt(double Right, double Up, double Margin) const
{
	if (FMath::Abs(Right) <= HandButtonWidth / 2.0 + Margin)
	{
		for (int32 Item = 0; Item < MenuCount; ++Item)
		{
			if (FMath::Abs(Up - HandItemUp(Item)) <= (HandButtonHeight + HandButtonGap) / 2.0 + (Item == 0 || Item == MenuCount - 1 ? Margin : 0.0))
			{
				return Item;
			}
		}
	}
	if (FMath::Abs(Up - HandBarUp) <= HandBarReach && FMath::Abs(Right) <= HandBarWidth / 2.0 + Margin)
	{
		return HandPartBar;
	}
	if (FMath::Abs(Up - HandBarUp) <= HandHideReach && FMath::Abs(Right - HandHideRight) <= HandHideReach)
	{
		return HandPartHide;
	}
	return INDEX_NONE;
}

void AStarPointer::UseGazePinches(const FVector& Head)
{
	if (!bHeadset)
	{
		return;
	}
	if (!bGazeListening)
	{
		bGazeListening = GazePinch::Listen();
	}
	const double Now = GetWorld()->GetRealTimeSeconds();
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = Player != nullptr ? Player->GetPawn() : nullptr;
	const FVector Base = Pawn != nullptr ? Pawn->GetActorLocation() : FVector::ZeroVector;
	for (const GazePinch::FEvent& Event : GazePinch::Take())
	{
		// The pinch moving the panel or the hand menu: it follows the hand until the pinch ends.
		if (Event.Phase != GazePinch::EPhase::Began)
		{
			if (GazeMoving == MoveNone || Event.Id != GazeMovingId)
			{
				continue;
			}
			if (Event.Phase == GazePinch::EPhase::Ended)
			{
				UE_LOG(LogStarPointer, Display, TEXT("Look and pinch: %s put down %.0f cm from the head"), GazeMoving == MovePanel ? TEXT("panel") : TEXT("hand menu"),
					FVector::Distance(Head, GazeMoving == MovePanel ? Panel->GetComponentLocation() : HandMenu->GetComponentLocation()));
				GazeMoving = MoveNone;
				continue;
			}
			if (!Event.bHasHand)
			{
				continue;
			}
			const FVector Offset = GazeMoveFrom + (Event.Hand - GazeHandFrom) * GazeMoveGain - Head;
			const bool bPanel = GazeMoving == MovePanel;
			const double Distance = FMath::Clamp(Offset.Size(), bPanel ? PanelNearest : HandMenuNearest, bPanel ? PanelFurthest : HandMenuFurthest);
			const FVector Place = Head + Offset.GetSafeNormal() * Distance;
			if (bPanel)
			{
				PanelPlace = PanelRotation.UnrotateVector(Place - Head);
			}
			else
			{
				HandMenuFromViewer = Place - Base;
				HandMenuRotation = FRotationMatrix::MakeFromXZ(Head - Place, FVector::UpVector).ToQuat();
			}
			continue;
		}
		if (!Event.bHasRay)
		{
			UE_LOG(LogStarPointer, Display, TEXT("Look and pinch: the pinch came without where the eyes were"));
			continue;
		}
		// A pinch reported twice, or one hand's while the other moves something.
		if (Now < GazeEchoUntil || GazeMoving != MoveNone)
		{
			UE_LOG(LogStarPointer, Display, TEXT("Look and pinch: ignored, %s"), GazeMoving != MoveNone ? TEXT("moving") : TEXT("too soon after the last"));
			continue;
		}
		GazeEchoUntil = Now + GazeEchoSeconds;
		// The pointing hand's own pinch, seen through hand tracking too, is not to act again.
		GateUntil = FMath::Max(GateUntil, Now + PanelGateSeconds);

		// The hand menu first, as it is nearest, then the panel, then the stars. The hand menu's
		// part is where the look meets its plane, from in front.
		int32 HandPart = INDEX_NONE;
		if (bHandMenuShown)
		{
			const FTransform& Menu = HandMenu->GetComponentTransform();
			const FVector From = Menu.InverseTransformPosition(Event.Origin);
			const FVector Towards = Menu.InverseTransformVectorNoScale(Event.Direction);
			if (Towards.X < -UE_KINDA_SMALL_NUMBER)
			{
				const FVector Hit = From + Towards * (-From.X / Towards.X);
				HandPart = HandMenuPartAt(-Hit.Y, Hit.Z, PokeMargin);
			}
		}
		double Along = 0.0;
		const int32 Part = HandPart == INDEX_NONE ? HitPanel(Event.Origin, Event.Direction, Along) : PartNone;
		const auto StartMoving = [&](int32 What, const FVector& From, const TCHAR* Name) {
			if (!Event.bHasHand)
			{
				UE_LOG(LogStarPointer, Display, TEXT("Look and pinch: the %s, but the pinch came without its hand, to move it by"), Name);
				return;
			}
			GazeMoving = What;
			GazeMovingId = Event.Id;
			GazeMoveFrom = From;
			GazeHandFrom = Event.Hand;
			GazeMoveGain = FMath::Max(1.0, FVector::Distance(Head, From) / FMath::Max(FVector::Distance(Head, Event.Hand), 1.0));
			UE_LOG(LogStarPointer, Display, TEXT("Look and pinch: moving the %s, %.1f times as far as the hand"), Name, GazeMoveGain);
		};
		if (HandPart != INDEX_NONE && HandPart < MenuCount)
		{
			UE_LOG(LogStarPointer, Display, TEXT("Look and pinch: hand menu item %d"), HandPart);
			HandPressed = HandPart;
			HandPressedUntil = Now + PressedSeconds;
			RunMenuItem(HandPart);
		}
		else if (HandPart == HandPartHide)
		{
			UE_LOG(LogStarPointer, Display, TEXT("Look and pinch: the hand menu's close button"));
			CloseHandMenu();
		}
		else if (HandPart == HandPartBar)
		{
			StartMoving(MoveHandMenu, HandMenu->GetComponentLocation(), TEXT("hand menu"));
		}
		else if (Part == PartBar || Part == PartBody)
		{
			StartMoving(MovePanel, Panel->GetComponentLocation(), TEXT("panel"));
		}
		else if (Part != PartNone)
		{
			UE_LOG(LogStarPointer, Display, TEXT("Look and pinch: the panel's part %d"), Part);
			PressPanel(Part, Event.Origin, Event.Direction, Along);
		}
		else
		{
			const int32 Star = StarField->FindStar(Event.Origin, Event.Direction, MaxPointingAngleDegrees);
			UE_LOG(LogStarPointer, Display, TEXT("Look and pinch: %s"), Star != INDEX_NONE ? *StarField->GetName(Star) : TEXT("no star"));
			if (Star == INDEX_NONE)
			{
				continue;
			}
			// As with the ray: two in quick succession on the star kept let it go.
			LastInUse = Now;
			bPointingStarted = true;
			if (Star != Selected)
			{
				Select(Star);
			}
			else if (Now - LastKeptPinch < DoublePinchSeconds)
			{
				UE_LOG(LogStarPointer, Display, TEXT("Look and pinch: second %.2f s after the first, letting the star go"), Now - LastKeptPinch);
				Select(INDEX_NONE);
			}
			else
			{
				LastKeptPinch = Now;
			}
		}
	}
}

void AStarPointer::CloseHandMenu()
{
	bHandMenuShown = false;
	if (GazeMoving == MoveHandMenu)
	{
		GazeMoving = MoveNone;
	}
	// Where the fingertip went, in the menu's frame (in front, right, up, in cm), to tell a press
	// that missed from one never tried.
	if (FingerFrames == 0)
	{
		UE_LOG(LogStarPointer, Display, TEXT("Hand menu closed; the pointing hand was not tracked while it was open"));
		return;
	}
	UE_LOG(LogStarPointer, Display, TEXT("Hand menu closed; the fingertip came within %.1f cm of it, at %.1f in front, %.1f right, %.1f up (%d frames tracked)"),
		FingerNearest, FingerNearestAt.X, FingerNearestAt.Y, FingerNearestAt.Z, FingerFrames);
}

void AStarPointer::ShowNote(const FString& Text)
{
	SetVoiceLine(Text, false);
	NoteUntil = GetWorld()->GetRealTimeSeconds() + NoteSeconds;
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

void AStarPointer::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Mouse.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Mouse);
	}
	Mouse.Reset();
	Super::EndPlay(Reason);
}

void AStarPointer::UseMouse(const FVector& Head, const FRotator& HeadRotation)
{
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = Player != nullptr ? Player->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		return;
	}
	// As in Blender, with the star kept as the pivot. Each step works from where the view
	// is after the one before, since the camera manager reports last frame's.
	FVector Camera = Head;
	FQuat Facing = HeadRotation.Quaternion();
	const bool bPivot = Selected != INDEX_NONE;
	const FVector Pivot = bPivot ? StarField->GetPosition(Selected) : Camera;

	// A left or right drag turns the view in place; so does a middle-drag with no star to orbit.
	// The mouse's gestures, and any asked for by command (MoveView).
	FVector2D Turn = Queued.Turn;
	FVector2D Orbit = Queued.Orbit;
	FVector2D Panned = Queued.Pan;
	float Zoom = Queued.Zoom;
	Queued = FViewMove();
	if (Mouse.IsValid())
	{
		Turn += Mouse->TakeLook();
		Orbit += Mouse->TakeOrbit();
		Panned += Mouse->TakePan();
		Zoom += Mouse->TakeWheel() + Mouse->TakeZoomDrag() * ZoomNotchesPerPixel;
		if (Mouse->IsDragging() || !Turn.IsZero() || !Orbit.IsZero() || !Panned.IsZero() || Zoom != 0.0f)
		{
			LastViewMove = GetWorld()->GetRealTimeSeconds();
		}
	}
	// Both turn about the screen's own axes, a horizontal drag about its vertical and a
	// vertical drag about its horizontal: turning about the sky's pole looked like rolling the
	// screen whenever the view was not upright to it. The view is then set upright to ViewUp
	// again, which leaves the line of sight, and so the star at its centre, where it is.
	const auto ScreenTurn = [&Facing](const FVector2D& By) {
		const FQuat Across(Facing.GetUpVector(), FMath::DegreesToRadians(By.X * LookDegreesPerPixel));
		const FQuat Up(Facing.GetRightVector(), FMath::DegreesToRadians(By.Y * LookDegreesPerPixel));
		return Across * Up;
	};
	if (bPivot)
	{
		if (!Orbit.IsNearlyZero())
		{
			// Round the star: the camera and its facing turn together, so the star stays put.
			FQuat Rotation = ScreenTurn(Orbit);
			if (IsNearPole((Rotation * Facing).GetForwardVector()))
			{
				Rotation = ScreenTurn(FVector2D(Orbit.X, 0.0)); // never over the top, where the view would flip
			}
			Camera = Pivot + Rotation.RotateVector(Camera - Pivot);
			Facing = Rotation * Facing;
		}
	}
	else
	{
		Turn += Orbit;
	}
	if (!Turn.IsNearlyZero())
	{
		// In place, the sky moving with the hand: the opposite way to orbiting.
		FQuat Rotation = ScreenTurn(-Turn);
		if (IsNearPole((Rotation * Facing).GetForwardVector()))
		{
			Rotation = ScreenTurn(FVector2D(-Turn.X, 0.0));
		}
		Facing = Rotation * Facing;
	}

	// Shift + middle-drag slides the view across, the sky following the hand, faster the
	// further the star kept.
	const double Reach = bPivot ? FMath::Max(FVector::Distance(Camera, Pivot), SmallestStep) : AStarField::UnitsPerParsec;
	if (!Panned.IsNearlyZero())
	{
		Camera += (-Facing.GetRightVector() * Panned.X + Facing.GetUpVector() * Panned.Y) * Reach * PanPerPixel;
	}

	// The wheel, or Ctrl + middle-drag, zooms: towards the star kept, along the line to it so
	// it stays where it is on screen, each notch a fraction of the way; else straight ahead.
	if (Zoom != 0.0f)
	{
		if (bPivot)
		{
			// Away from the star along the line from it, or, on top of it (no line), straight back.
			const double Distance = FVector::Distance(Camera, Pivot);
			const double Wanted = FMath::Max(Distance * FMath::Pow(ZoomFactor, static_cast<double>(Zoom)), SmallestStep);
			const FVector Away = Distance > UE_KINDA_SMALL_NUMBER ? (Camera - Pivot) / Distance : -Facing.GetForwardVector();
			Camera = Pivot + Away * Wanted;
		}
		else
		{
			Camera += Facing.GetForwardVector() * 0.5 * AStarField::UnitsPerParsec * Zoom;
		}
	}

	// "." or F turns the view to the star kept, as Blender's View Selected frames it.
	if (bPivot && (Player->WasInputKeyJustPressed(EKeys::Period) || Player->WasInputKeyJustPressed(EKeys::F) ||
					  Player->WasInputKeyJustPressed(EKeys::Decimal)))
	{
		Facing = (Pivot - Camera).Rotation().Quaternion();
	}

	if (!Camera.Equals(Head))
	{
		Pawn->AddActorWorldOffset(Camera - Head);
	}
	if (!Facing.Equals(HeadRotation.Quaternion()))
	{
		Player->SetControlRotation(ViewRotation(Facing.GetForwardVector()));
	}

	// A click keeps the star under the cursor, in place of any kept; a click on empty sky,
	// or a double-click, lets go.
	if (!Mouse.IsValid())
	{
		return;
	}
	for (const FDesktopMouse::FClick& Click : Mouse->TakeClicks())
	{
		Select(Click.bDouble ? INDEX_NONE : Hovered);
	}
}

void AStarPointer::SetViewUp(const FVector& Up)
{
	ViewUp = Up.GetSafeNormal();
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (Player == nullptr || Player->PlayerCameraManager == nullptr)
	{
		return;
	}
	// With another pole up the view rolls, which the camera manager limits to under a right
	// angle by default. Not -180 to 180: its clamp takes a full turn as no range at all, and
	// held the roll at 180, the view upside down.
	Player->PlayerCameraManager->ViewRollMin = -179.9f;
	Player->PlayerCameraManager->ViewRollMax = 179.9f;
	Player->SetControlRotation(ViewRotation(Player->PlayerCameraManager->GetCameraRotation().Vector()));
}

FRotator AStarPointer::ViewRotation(const FVector& Forward) const
{
	// Looking at the pole itself there is no up to keep: keep the way it is.
	if (FMath::Abs(FVector::DotProduct(Forward.GetSafeNormal(), ViewUp)) > 0.9999)
	{
		return Forward.Rotation();
	}
	return FRotationMatrix::MakeFromXZ(Forward, ViewUp).Rotator();
}

bool AStarPointer::IsNearPole(const FVector& Forward) const
{
	return FMath::Abs(FVector::DotProduct(Forward.GetSafeNormal(), ViewUp)) > FMath::Cos(FMath::DegreesToRadians(90.0 - MaxPitchDegrees));
}

void AStarPointer::Keep(int32 Star)
{
	// Spoken, a command needs no pinch to turn the ray on first; when the star is let go the
	// ray is there to point with.
	bPointingStarted = true;
	Select(Star);
}

bool AStarPointer::IsOtherHandPinched() const
{
	if (!bHeadset || Pointer == MouseHand)
	{
		return false;
	}
	return Hands[1 - Pointer].bPinched && !Hands[Pointer].bPinched;
}

void AStarPointer::SetVoiceLine(const FString& Text, bool bActive)
{
	VoiceLine->SetText(FText::FromString(Text));
	VoiceLine->SetTextRenderColor(((bActive ? SelectionColour : DetailsColour) * OverlayBrightness).ToFColor(false));
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
	// Facing the head, as text faces along its +X, upright to the view: square on wherever its
	// bar has carried it.
	const FVector Place = Head + PanelRotation.RotateVector(PanelPlace);
	Panel->SetWorldLocationAndRotation(Place, FRotationMatrix::MakeFromXZ(Head - Place, PanelRotation.GetUpVector()).ToQuat());

	// In a headset the panel goes a while after it was last in use, and "Hide info" hides it
	// until "Show info"; it shows the countdown to quitting whatever.
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (!bPointingStarted || Hovered != INDEX_NONE || Selected != INDEX_NONE || Aimed != PartNone || bCarrying || bMenuOpen || GazeMoving == MovePanel)
	{
		LastInUse = Now;
	}
	bInfoVisible = BothPinchedSeconds > 0.0 || (bInfoShown && (!bHeadset || Now - LastInUse < InfoIdleSeconds));
	Frame->SetVisibility(bInfoVisible);
	Title->SetVisibility(bInfoVisible);
	Details->SetVisibility(bInfoVisible);
	Hint->SetVisibility(bInfoVisible);

	// The bar, the button and the list: brighter under the ray, and only in a headset (or on
	// the desktop with -PanelControls, the list open, to check their layout). The button and its
	// list are for controllers, which have no hand menu, and stay with the panel hidden, to
	// show it again from.
	const bool bButton = (bHeadset && bUsingControllers) || bShowControls;
	const bool bList = (bButton && bMenuOpen) || bShowControls;
	bMenuOpen &= bButton;
	GrabBar->SetVisibility((bHeadset || bShowControls) && bInfoVisible);
	HideRing->SetVisibility(GrabBar->IsVisible());
	HideCross->SetVisibility(GrabBar->IsVisible());
	ActionsFrame->SetVisibility(bButton);
	ActionsText->SetVisibility(bButton);
	MenuFrame->SetVisibility(bList);
	if (GrabBarMaterial != nullptr)
	{
		GrabBarMaterial->SetVectorParameterValue(TEXT("Colour"), (Aimed == PartBar || bCarrying || GazeMoving == MovePanel ? SelectionColour : BarColour) * OverlayBrightness);
	}
	if (HideMaterial != nullptr)
	{
		HideMaterial->SetVectorParameterValue(TEXT("Colour"), (Aimed == PartHide ? SelectionColour : FrameColour) * OverlayBrightness);
	}
	ActionsText->SetTextRenderColor(((Aimed == PartActions || bMenuOpen ? SelectionColour : DetailsColour) * OverlayBrightness).ToFColor(false));
	for (int32 Item = 0; Item < MenuItems.Num(); ++Item)
	{
		MenuItems[Item]->SetVisibility(bList);
		MenuItems[Item]->SetText(FText::FromString(MenuLabel(Item)));
		MenuItems[Item]->SetTextRenderColor(((Aimed == PartItem + Item ? SelectionColour : DetailsColour) * OverlayBrightness).ToFColor(false));
	}

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
		// A star kept by name may be anywhere: say which way to turn until it is in view.
		const FVector Local = HeadRotation.UnrotateVector(StarField->GetPosition(Selected) - Head).GetSafeNormal();
		const double Right = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
		const double Up = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Local.Z, -1.0, 1.0)));
		if (FMath::Abs(Right) > InViewDegrees || FMath::Abs(Up) > InViewDegrees)
		{
			Hint->SetText(FText::FromString(FString::Printf(TEXT("Turn %s %.0f\u00B0, %s %.0f\u00B0"), Right > 0.0 ? TEXT("right") : TEXT("left"),
				FMath::Abs(Right), Up > 0.0 ? TEXT("up") : TEXT("down"), FMath::Abs(Up))));
			return;
		}
		Hint->SetText(FText::FromString(!bHeadset ? TEXT("Middle-drag to orbit; click sky to let go")
				: bUsingControllers               ? TEXT("Pull the trigger twice to let it go")
				: IsLookOnly()                    ? TEXT("Look at it and pinch twice to let it go")
												  : TEXT("Pinch twice to let it go")));
		return;
	}
	Hint->SetText(FText::FromString(IsLookOnly() ? (Pointer == 0 ? TEXT("To keep it; right wrist: menu") : TEXT("To keep it; left wrist: menu"))
			: !bPointingStarted                     ? (bUsingControllers ? TEXT("Then pull the trigger again to keep a star") : TEXT("Then pinch again to keep a star"))
			: !bHeadset                             ? TEXT("Click a star to keep it; drag to look")
			: bUsingControllers                     ? TEXT("Pull the trigger to keep a star, both to exit")
			: Pointer == 0                          ? TEXT("Pinch to keep a star; right wrist: menu")
													: TEXT("Pinch to keep a star; left wrist: menu")));
	if (Hovered != INDEX_NONE)
	{
		ShowStar(Hovered, HoverColour);
		return;
	}
	Title->SetText(FText::FromString(IsLookOnly()      ? TEXT("Look at a star and pinch")
			: !bPointingStarted                    ? (bUsingControllers ? TEXT("Pull the trigger to start pointing") : TEXT("Pinch to start pointing"))
			: bHeadset                              ? TEXT("Point at a star")
													: TEXT("Move over a star")));
	Title->SetTextRenderColor((HintColour * OverlayBrightness).ToFColor(false));
	// With nothing to describe, the build, so a session can be matched to its code.
	Details->SetText(FText::FromString(FString::Printf(TEXT("v%s, build %s\n%s, %s"), *StellarVersion(), *StellarShortCommit(), STELLAR_SPIKE_BRANCH, STELLAR_SPIKE_BUILT)));
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
