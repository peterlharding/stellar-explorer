#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StarPointer.generated.h"

class AStarField;
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
 * quits. On the desktop the ray is the view's centre and the left mouse button selects. The star pointed at is ringed and described
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

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

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

	FHand Hands[3];
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
