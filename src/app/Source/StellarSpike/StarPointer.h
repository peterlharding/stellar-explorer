#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StarPointer.generated.h"

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
 * index tip) selects. In a headset with controllers instead (the Vive) the ray is the controller's
 * aim and the trigger selects. The other hand (or controller) is tracked only for the gesture that
 * quits. On the desktop the ray runs under the cursor and the mouse works as in Blender, the
 * star kept the pivot: a click keeps the star under it (on empty sky, lets go), middle-drag
 * orbits, Shift + middle-drag pans, the wheel zooms, "." or F faces it (FDesktopMouse). The star pointed at is ringed and described
 * in a panel at the bottom right of the view, which follows the head. In a headset the ray
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
	};

	static constexpr int32 MouseHand = 2;

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
	 * The keys a controller's trigger appears as, when the headset has controllers (the Vive). The
	 * keys are device-specific, so each is a comma-separated list tried in turn, the largest value
	 * winning: a Vive wand and a Valve Index controller use different names. -RightTriggerKey= /
	 * -LeftTriggerKey= override without a rebuild; -LogInput lists every key the engine registered.
	 */
	UPROPERTY(Config)
	FString RightTriggerKey = TEXT("Vive_Right_Trigger_Axis,ValveIndex_Right_Trigger_Axis");

	UPROPERTY(Config)
	FString LeftTriggerKey = TEXT("Vive_Left_Trigger_Axis,ValveIndex_Left_Trigger_Axis");

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
	/** Whether the ray is on: in a headset it starts off, and a pinch turns it on. */
	bool bPointingStarted = false;
	/** The panel's rotation, which follows the head's a little behind. */
	FQuat PanelRotation = FQuat::Identity;
	bool bPanelPlaced = false;
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
	static constexpr double HoldToExitSeconds = 2.0;
};
