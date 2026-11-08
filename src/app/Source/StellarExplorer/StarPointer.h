#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StarPointer.generated.h"

class AStarActions;
class AStarField;
class FDesktopMouse;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Point at a star to see what it is, and pinch to keep it.
 *
 * In a headset with hand tracking (the Vision Pro) the pointing hand (PointingHand, right unless
 * set otherwise) casts a ray from an estimated shoulder through the hand, and a pinch (thumb tip to
 * index tip) selects, if bHandRay is on; by default it is off, and look and pinch does it all. In a headset with controllers instead (the Vive) the ray is the controller's
 * aim and the trigger selects. The other hand (or controller) is tracked only for the gesture that
 * quits. On the desktop the ray runs under the cursor and the mouse works as in Blender, the
 * star kept the pivot: a click keeps the star under it (on empty sky, lets go), middle-drag
 * orbits, Shift + middle-drag pans, the wheel zooms, "." or F faces it (FDesktopMouse).
 *
 * How far out stars are shown (AStarField::SetViewRadius) is set by a slider on the hand menu,
 * or by pinching with both hands and moving them apart (further out) or together (nearer in);
 * with controllers, by the view-radius dial the left controller's menu button or grip opens, the
 * trackpad setting the radius and its click turning it off and on.
 *
 * In a headset the panel has a grab bar below it (pinch it and move to carry the panel to
 * another place in the view), with a button beside it that hides the panel. On the Vision Pro
 * a pinch also acts on what the eyes were on as it began (GazePinch): look at a star and pinch
 * to keep it, at a button to press it, at the panel or a bar to move it with the hand. The commands needing no star's name (Go there, Back off, ...,
 * Hide info) are on a menu opened and closed by a button on the back of the other hand's wrist,
 * both pressed with the pointing hand's index finger; with controllers instead, on a list opened by a "..."
 * button above the panel or by the right controller's menu button, its items scrolled through with
 * the trackpad (the Index's thumbstick) and run by the trackpad's click. The ray shows whenever it points at the panel, even with a star
 * kept. -PanelControls shows them all on the desktop too, to check the layout. The star
 * pointed at is ringed and described in a panel at the bottom right of the view, which follows
 * the head. In a headset the panel goes after a few seconds with nothing pointed at or kept,
 * and "Hide info" hides it until "Show info". In a headset the ray
 * is off at start, and a first pinch turns it on. A pinch then keeps the
 * star: it is ringed in another colour, the panel stays on it, and the ray goes until two
 * pinches in quick succession let the star go. Pinching both hands for
 * HoldToExitSeconds quits. -PointerTest selects whatever is pointed at after 3 s, for
 * checking it from a script.
 */
UCLASS(Config = Game)
class AStarPointer : public AActor
{
	GENERATED_BODY()

public:
	AStarPointer();

	void SetStarField(AStarField* InStarField) { StarField = InStarField; }

	/** What the panel's actions list runs its commands through. */
	void SetActions(AStarActions* InActions) { Actions = InActions; }

	/** Keep a star as a pinch would, or with INDEX_NONE let the kept one go: for spoken commands. */
	void Keep(int32 Star);

	/**
	 * Which way is up on the desktop's screen, in this app's frame: the celestial north pole
	 * at start, or another (the galactic north pole) by SetViewUp. Every turn keeps it up.
	 */
	void SetViewUp(const FVector& Up);
	/** The view looking along Forward with ViewUp up. */
	FRotator ViewRotation(const FVector& Forward) const;

	/** The star kept, and the one pointed at; INDEX_NONE for none. */
	int32 GetKept() const { return Selected; }
	int32 GetPointed() const { return Hovered; }

	/** The star kept, or else the one pointed at, or INDEX_NONE: what "go there" means. */
	int32 GetTarget() const { return Selected != INDEX_NONE ? Selected : Hovered; }

	/**
	 * Move the view as the mouse would, in its units (pixels dragged, notches of the wheel):
	 * for commands ("orbit 100 0"), and for testing the mouse's moves without a mouse.
	 */
	void MoveView(const FVector2D& Turn, const FVector2D& Orbit, const FVector2D& Pan, float Zoom)
	{
		Queued.Turn += Turn;
		Queued.Orbit += Orbit;
		Queued.Pan += Pan;
		Queued.Zoom += Zoom;
	}

	/** In a headset with hands, whether the hand that does not point is pinched on its own (push to talk). */
	bool IsOtherHandPinched() const;

	/** Show or hide the panel describing the star; hidden, it stays so until shown again. */
	void SetInfoShown(bool bShown);
	bool IsInfoShown() const { return bInfoShown; }

	/** A line just above the panel, for what voice heard and did; empty hides it. Active is brighter. */
	void SetVoiceLine(const FString& Text, bool bActive);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FRay
	{
		FVector Origin;
		FVector Direction;
		bool bPinched = false;
		/** Index into Hands: 0 left, 1 right, 2 the mouse. */
		int32 Hand = MouseHand;
		/** Thumb tip to index tip, in Unreal units. */
		double Gap = 0.0;
	};

	/** What a hand's ray pointed at, and how far apart thumb and index were. */
	struct FSighting
	{
		double Time;
		int32 Star;
		double Gap;
		/** The part of the panel the ray met (PartNone if none), and how far along the ray. */
		int32 Part;
		double Along;
	};

	struct FHand
	{
		bool bPinched = false;
		bool bWasPinched = false;
		/** Seen with thumb and index apart since tracking (re)started: only then can it pinch. */
		bool bSeenOpen = false;
		/** While pinched, the closest thumb and index have come: the pinch ends as they part from it. */
		double PinchClosest = 0.0;
		/** While pinched, when thumb and index last moved apart enough to end it, or negative. */
		double OpenSince = -1.0;
		/** Between pinches, the widest thumb and index have been: the next must close from it. */
		double OpenWidest = 0.0;
		/** While thumb and index are close without pinching, the closest they came, for the log. */
		double ClosestGap = TNumericLimits<double>::Max();
		/** For the log: how wide they had opened when that close approach began. */
		double DipOpenedTo = 0.0;
		/** The ray's direction, smoothed over a few frames. */
		FVector Direction = FVector::ZeroVector;
		/** Recent frames: a pinch selects what was pointed at before the fingers began to close. */
		TArray<FSighting, TInlineAllocator<64>> Recent;
		/** Tracked as a hand this frame (not a controller), with its index fingertip and palm. */
		bool bHandTracked = false;
		FVector IndexTip = FVector::ZeroVector;
		FVector Palm = FVector::ZeroVector;
		/** Out of the palm, a unit vector; zero if the joints to find it are not tracked. */
		FVector PalmNormal = FVector::ZeroVector;
		FVector Wrist = FVector::ZeroVector;
		double Gap = 0.0;
	};

	static constexpr int32 MouseHand = 2;

	/** Hands with no ray (bHandRay off): look and pinch only. */
	bool IsLookOnly() const { return bHeadset && !bUsingControllers && !bHandRay; }

	/** The rays this frame: one per tracked hand in a headset, the view's centre on the desktop. */
	TArray<FRay, TInlineAllocator<2>> GetRays(const FVector& Head, const FRotator& HeadRotation);
	/** The rays from motion controllers, when the headset has those instead of hand tracking. */
	void AppendControllerRays(TArray<FRay, TInlineAllocator<2>>& Rays);
	/** Whether either motion controller is tracked this frame, without disturbing the hand state. */
	bool AnyControllerTracked() const;
	void Select(int32 Star);
	/** On the desktop: the mouse's gestures and MoveView's (FDesktopMouse). */
	void UseMouse(const FVector& Head, const FRotator& HeadRotation);
	void PlaceRing(UProceduralMeshComponent* Ring, int32 Star, const FVector& Head) const;
	void PlacePanel(const FVector& Head, const FRotator& HeadRotation, float DeltaSeconds);
	/** The view-radius block: follow the head like the info panel, and set the slider and text to the radius. */
	void PlaceRadiusPanel(const FVector& Head, const FRotator& HeadRotation, float DeltaSeconds);
	/** The part of the panel, its grab bar, its actions button or its list a ray meets, and how far along it. */
	int32 HitPanel(const FVector& Origin, const FVector& Direction, double& Along) const;
	/** A pinch on a part of the panel: grab it, open or close the list, or run an item. */
	void PressPanel(int32 Part, const FVector& Origin, const FVector& Direction, double Along);
	/** The hand menu: open and close it by the wrist button, and press its items, with the pointing finger. */
	void UpdateHandMenu(const FVector& Head, const FRotator& HeadRotation, float DeltaSeconds);
	/** Show only the stars within Parsecs (0: every star) from the hand menu's slider or the two-hand gesture, saying so above the panel. */
	void SetViewRadius(double Parsecs);
	/** Both hands pinched and moved apart or together: the view radius follows. */
	void UpdateResize(int32 Pinched, float DeltaSeconds);
	/** Close the hand menu, logging where the fingertip went while it was open. */
	void CloseHandMenu();
	/** The label of an item of the actions list and the hand menu: "Hide info" or "Show info" as it is. */
	FString MenuLabel(int32 Item) const;
	/** Run an item of the actions list or the hand menu, and show what it did. */
	void RunMenuItem(int32 Item);
	/**
	 * The hand menu's part at a point on it (right, up from its centre): an item, HandPartBar,
	 * HandPartHide, or INDEX_NONE; items reach Margin beyond the column's ends.
	 */
	int32 HandMenuPartAt(double Right, double Up, double Margin) const;
	/**
	 * Look-and-pinch (GazePinch): a pinch presses what the wearer looked at as it began, a hand
	 * menu item or a hide button, or keeps the star; on the panel or a bar, the hand moves it
	 * until the pinch ends.
	 */
	void UseGazePinches(const FVector& Head);
	/** Show a line above the panel for a while, as voice and the console do. */
	void ShowNote(const FString& Text);
	void ShowStar(int32 Star, const FLinearColor& Colour);
	UMaterialInstanceDynamic* ColouredMaterial(const FLinearColor& Colour);

	UPROPERTY()
	TObjectPtr<AStarField> StarField;

	/** "Right" or "Left": the hand that points, from DefaultGame.ini, or -PointingHand= on the command line. */
	UPROPERTY(Config)
	FString PointingHand = TEXT("Right");

	/**
	 * Seconds after a star is kept or let go during which pinches are ignored, so a pinch that
	 * the tracking sees twice does not undo itself. From DefaultGame.ini, or -GestureGate=.
	 */
	UPROPERTY(Config)
	float GestureGateSeconds = 1.0f;

	/** Two pinches this close together, with a star kept, let it go. From DefaultGame.ini, or -DoublePinch=. */
	UPROPERTY(Config)
	float DoublePinchSeconds = 1.0f;

	/**
	 * With hand tracking, whether the pointing hand has a ray to point with; off, look and
	 * pinch does it all. From DefaultGame.ini, or -HandRay to turn it on.
	 */
	UPROPERTY(Config)
	bool bHandRay = false;

	/**
	 * The keys a controller's trigger appears as, when the headset has controllers (the Vive). The
	 * keys are device-specific, so each is a comma-separated list tried in turn, the largest value
	 * winning: a Vive wand and a Valve Index controller use different names. -RightTriggerKey= /
	 * -LeftTriggerKey= override without a rebuild; -LogInput lists every key the engine registered.
	 */
	UPROPERTY(Config)
	FString RightTriggerKey = TEXT("Vive_Right_Trigger_Axis,ValveIndex_Right_Trigger_Axis");

	UPROPERTY(Config)
	FString LeftTriggerKey = TEXT("Vive_Left_Trigger_Axis,ValveIndex_Left_Trigger_Axis");

	/**
	 * The keys the right controller's menu button appears as, device-specific like the trigger keys:
	 * a comma-separated list tried in turn. Pressing it opens and closes the actions list (the same
	 * list the panel's "..." button opens), so the commands are reached without pointing at the panel.
	 * The left controller's menu button or grip opens the view-radius dial instead (RadiusButtonKey).
	 * -MenuButtonKey= overrides without a rebuild; -LogInput lists every registered key.
	 */
	UPROPERTY(Config)
	FString MenuButtonKey = TEXT("Vive_Right_Menu_Click,ValveIndex_Right_B_Click");

	/**
	 * The keys that open the view-radius dial on the left controller, where the trackpad's vertical
	 * axis sets how far out stars are shown and its click turns the radius off and on. The left menu
	 * button and the left grip both open it: on some runtimes the left menu button is swallowed and
	 * never reaches the app (the right one is fine), so the grip is a reliable fallback. Comma-separated
	 * and device-specific like the others; -RadiusButtonKey= overrides, -LogInput lists registered keys.
	 */
	UPROPERTY(Config)
	FString RadiusButtonKey = TEXT("Vive_Left_Menu_Click,Vive_Left_Grip_Click,ValveIndex_Left_B_Click,ValveIndex_Left_Grip_Click");

	/**
	 * With the actions list open, the trackpad (the Vive) or thumbstick (the Index) scrolls a
	 * highlight up and down the items: its vertical axis, device-specific like the trigger keys, a
	 * comma-separated list polled in turn, the largest magnitude winning. The items are too small
	 * to aim the ray at, so the list is driven like a list instead. -MenuScrollKey= overrides
	 * without a rebuild; -LogInput lists every registered key.
	 */
	UPROPERTY(Config)
	FString MenuScrollKey = TEXT("Vive_Left_Trackpad_Y,Vive_Right_Trackpad_Y,ValveIndex_Left_Thumbstick_Y,ValveIndex_Right_Thumbstick_Y");

	/** The trackpad's (or thumbstick's) click, which runs the highlighted item of the actions list. -MenuSelectKey= overrides. */
	UPROPERTY(Config)
	FString MenuSelectKey = TEXT("Vive_Left_Trackpad_Click,Vive_Right_Trackpad_Click,ValveIndex_Left_Thumbstick_Click,ValveIndex_Right_Thumbstick_Click");

	/**
	 * The view-radius dial reads only the left controller's trackpad (the Index's left thumbstick): a
	 * vertical axis, polled like the others and counted only while the pad is touched. The dial is on
	 * the left controller that opens it, so the right pad is left for the actions list. -RadiusDialKey=
	 * overrides without a rebuild.
	 */
	UPROPERTY(Config)
	FString RadiusDialKey = TEXT("Vive_Left_Trackpad_Y,ValveIndex_Left_Thumbstick_Y");

	/** The pointing hand's ray, drawn as dots. */
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Dots;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> HoverRing;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> SelectionRing;

	/** The panel: a frame with the star's name and its details inside. */
	UPROPERTY()
	TObjectPtr<USceneComponent> Panel;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Frame;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Title;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Details;

	/** What a pinch would do now, at the bottom of the panel. */
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Hint;

	/** What voice heard and did, just above the panel. */
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> VoiceLine;

	/** The pill under the panel: pinch it and move to carry the panel, as a visionOS window's bar. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> GrabBar;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GrabBarMaterial;

	/** The button beside the grab bar that hides the panel, a cross in a ring. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> HideRing;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> HideCross;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HideMaterial;

	/** The "..." button above the panel's right end, which opens the actions list. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ActionsFrame;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> ActionsText;

	/** The actions list, to the panel's left: the commands that need no star's name. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> MenuFrame;

	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> MenuItems;

	/**
	 * The view-radius block the left controller opens: a framed panel floating opposite the info
	 * panel, with a horizontal slider (track and knob) showing how far out stars are shown, the
	 * cutoff in words below it, and a hint. The left trackpad slides the knob; its click shows every
	 * star or the last radius. The controller's answer to the Vision Pro hand-menu slider.
	 */
	UPROPERTY()
	TObjectPtr<USceneComponent> RadiusPanel;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> RadiusFrame;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> RadiusTitle;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> RadiusValue;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> RadiusHint;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> RadiusTrack;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> RadiusKnob;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> RadiusKnobMaterial;

	/** The hand menu, beside the other hand: a framed button for each item. */
	UPROPERTY()
	TObjectPtr<USceneComponent> HandMenu;

	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> HandFrames;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> HandFrameMaterials;

	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> HandItems;

	/** Under the hand menu's buttons, the slider for how far out stars are shown, and what it is set to. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> HandSliderTrack;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HandSliderKnob;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HandSliderKnobMaterial;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> HandSliderLabel;

	/** Under the hand menu: a bar to move it by, and beside it a button that closes it. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> HandBar;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HandBarMaterial;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> HandHideRing;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> HandHideCross;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HandHideMaterial;

	/** The button on the back of the other hand's wrist that opens and closes the hand menu. */
	UPROPERTY()
	TObjectPtr<USceneComponent> WristButton;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> WristRing;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> WristText;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WristMaterial;

	/** A dot on the hand menu under the pointing fingertip: where the app sees it, to aim the press by. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> FingerDot;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FingerDotMaterial;

	UPROPERTY()
	TObjectPtr<AStarActions> Actions;

	struct FViewMove
	{
		FVector2D Turn = FVector2D::ZeroVector;
		FVector2D Orbit = FVector2D::ZeroVector;
		FVector2D Pan = FVector2D::ZeroVector;
		float Zoom = 0.0f;
		bool IsEmpty() const { return Turn.IsZero() && Orbit.IsZero() && Pan.IsZero() && Zoom == 0.0f; }
	};
	/** View moves asked for by command, made with the mouse's next. */
	FViewMove Queued;
	FVector ViewUp = FVector::UpVector;
	/** Whether looking along Forward is too near straight up or down (to ViewUp) to turn further. */
	bool IsNearPole(const FVector& Forward) const;
	/** When the mouse last moved the view: the hover ring stays off for a moment after. */
	double LastViewMove = -1.0e9;

	FHand Hands[3];
	/** The desktop's mouse, read through Slate; null in a headset. */
	TSharedPtr<FDesktopMouse> Mouse;
	int32 Hovered = INDEX_NONE;
	int32 Selected = INDEX_NONE;
	/** Index into Hands of the pointing hand: 0 left, 1 right, or MouseHand on the desktop. */
	int32 Pointer = 1;
	/** When a pinch last came while a star was kept: a second soon after lets the star go. */
	double LastKeptPinch = -1.0e9;
	/** Pinches before this time are ignored: see GestureGateSeconds. */
	double GateUntil = -1.0e9;
	/** Whether the ray is on: in a headset it starts off, and a pinch turns it on; with no hand ray, the first star kept by look and pinch. */
	bool bPointingStarted = false;
	/** The panel's rotation, which follows the head's a little behind. */
	FQuat PanelRotation = FQuat::Identity;
	/** Where the panel's centre is, in the head's frame (forward, right, up): moved by its bar. */
	FVector PanelPlace = FVector(100.0, 22.0, -21.0);
	/** The view-radius block's centre, in the head's frame: opposite the info panel, to its left. */
	FVector RadiusPanelPlace = FVector(100.0, -22.0, -21.0);
	/** The part of the panel the pointing ray meets this frame, or PartNone. */
	int32 Aimed = 0;
	double AimedAlong = 0.0;
	bool bMenuOpen = false;
	/** The highlighted item of the actions list while it is open and driven by the controller trackpad, or INDEX_NONE. */
	int32 MenuCursor = INDEX_NONE;
	/** The trackpad must return near its centre between scroll steps, so one flick steps once. */
	bool bMenuScrollArmed = true;
	/** The view-radius dial, opened by the left controller's menu button; the trackpad then sets the radius, its click turns it off and on. */
	bool bRadiusOpen = false;
	/** The radius the dial's click restores when it turns the radius back on, in parsecs; 0 until one is set. */
	double RadiusBeforeOff = 0.0;
	/** The radius block's rotation, which follows the head a little behind, and whether it has been placed yet. */
	FQuat RadiusPanelRotation = FQuat::Identity;
	bool bRadiusPlaced = false;
	/** The left trackpad is being slid this frame: the knob and value light up while it is. */
	bool bRadiusAdjusting = false;
	/** Carrying the panel by its bar: how far along the ray it was grabbed, and from the panel's centre. */
	bool bCarrying = false;
	double CarryAlong = 0.0;
	FVector CarryGrip = FVector::ZeroVector;
	/** When a line shown by ShowNote is to go. */
	double NoteUntil = 0.0;
	bool bPanelPlaced = false;
	/** Shown, or hidden by "Hide info" until "Show info". */
	bool bInfoShown = true;
	/** Whether the panel's frame and text showed this frame: hidden, the ray passes through it. */
	bool bInfoVisible = true;
	/** When the panel was last in use (a star pointed at or kept, the ray on it): it goes a while after. */
	double LastInUse = 0.0;
	/** -PanelControls: the panel's controls and the hand menu on the desktop too, to check their layout. */
	bool bShowControls = false;
	/** The hand menu: open, where, from the viewer's pawn, and which way it faces. */
	bool bHandMenuShown = false;
	FVector HandMenuFromViewer = FVector::ZeroVector;
	FQuat HandMenuRotation = FQuat::Identity;
	/** The wrist button needs the fingertip away from it before each touch. */
	bool bWristArmed = false;
	/** The pointing fingertip is at the hand menu or the wrist button this frame: it presses, not points. */
	bool bFingerAtMenu = false;
	/** The part under the fingertip (HandMenuPartAt), or INDEX_NONE; the one last pressed, lit until HandPressedUntil. */
	int32 HandAimed = INDEX_NONE;
	int32 HandPressed = INDEX_NONE;
	double HandPressedUntil = 0.0;
	/** A press needs the fingertip in front of the menu first; it is set again as the finger draws back. */
	bool bPokeArmed = false;
	double PokeDepth = 0.0;
	double PokeGateUntil = 0.0;
	/** Listening for look-and-pinch (GazePinch), once the engine's immersive layer is there. */
	bool bGazeListening = false;
	/**
	 * What a look-and-pinch is moving (MoveNone, MovePanel or MoveHandMenu) and by which pinch:
	 * where it and the hand were as it began, and how much further it goes than the hand.
	 */
	int32 GazeMoving = 0;
	uint64 GazeMovingId = 0;
	FVector GazeMoveFrom = FVector::ZeroVector;
	FVector GazeHandFrom = FVector::ZeroVector;
	double GazeMoveGain = 1.0;
	/** A pinch the system reports twice begins twice: the second within GazeEchoSeconds is ignored. */
	double GazeEchoUntil = 0.0;
	/** The fingertip is on the hand menu's slider, setting how far out stars are shown. */
	bool bSliderTouched = false;
	/**
	 * Both hands pinched: how far apart the fingertips were, and the view radius, as it began;
	 * moved apart or together by ResizeStart, the view radius follows, and they no longer quit.
	 */
	bool bBothWerePinched = false;
	bool bResizing = false;
	/** When each hand's pinch began (0 left, 1 right), as seen here; negative while not pinched. */
	double PinchBegan[2] = {-1.0, -1.0};
	/** When both hands were last seen pinched together, from; the fingertips' distance, smoothed. */
	double BothPinchedSince = 0.0;
	double ResizeGap = 0.0;
	/** Whether the distance and radius to resize from have been taken, ResizeArmSeconds in. */
	bool bResizeArmed = false;
	double ResizeFromGap = 0.0;
	double ResizeFromRadius = 0.0;
	/** For the log, while the menu shows: the nearest the fingertip came to it, and where, in its frame. */
	double FingerNearest = TNumericLimits<double>::Max();
	FVector FingerNearestAt = FVector::ZeroVector;
	int32 FingerFrames = 0;
	double BothPinchedSeconds = 0.0;
	bool bHeadset = false;
	/** Set once a tracked controller is seen, so the panel says "trigger" rather than "pinch". */
	bool bUsingControllers = false;
	/** -LogInput: dump the registered input keys and log trigger values each second, for bring-up. */
	bool bLogInput = false;
	/** Throttles the per-second trigger-value log that -LogInput turns on. */
	double NextControllerLog = 0.0;
	/** The input mode last logged (0 none, 1 hands, 2 controllers), so a change is logged just once. */
	int32 LoggedInputMode = -1;

	static constexpr double MaxPointingAngleDegrees = 2.0;
	/** Long enough not to happen by accident: at 2 s a pinch on a system prompt, the other hand resting, quit. */
	static constexpr double HoldToExitSeconds = 4.0;
};
