#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"

/**
 * The mouse on the desktop, read as Slate sees it before anything else: the game's own input
 * saw only the first press of a button after launch with the mouse left free, and a drag must
 * be told from a click. Only what starts over the game's window counts, not the console's.
 *
 * Collects, for the game thread to take each frame, as Blender reads the mouse: middle-drag
 * orbits, Shift + middle-drag pans, Ctrl + middle-drag and the wheel zoom; a left or right
 * drag turns the view in place; a left click (let go without dragging) and a double-click.
 */
class FDesktopMouse : public IInputProcessor
{
public:
	struct FClick
	{
		bool bDouble = false;
	};

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override;
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override;
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& Event, const FPointerEvent* Gesture) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("StellarDesktopMouse"); }

	/** What was dragged since the last call, in screen pixels, by gesture. */
	FVector2D TakeLook();
	FVector2D TakeOrbit();
	FVector2D TakePan();
	/** Ctrl + middle-drag, up to zoom in, in pixels. */
	float TakeZoomDrag();
	/** The wheel turned since the last call, in notches, forward positive. */
	float TakeWheel();
	TArray<FClick> TakeClicks();
	/** A drag is under way: past the few pixels that make it one, or the middle or right button held. */
	bool IsDragging() const { return Held.IsValid() && (bDragging || Held != EKeys::LeftMouseButton); }

	/** A widget over the view whose mouse is its own (the console's panel): not clicks or drags on the sky. */
	static void SetExcluded(const TSharedPtr<SWidget>& Widget) { Excluded = Widget; }
	/** Whether the cursor is over the view itself: not outside it, nor over the excluded widget. */
	static bool IsCursorOverView();

private:
	/** Whether the event is over the game's own window, and not over the excluded widget. */
	static bool OverGame(FSlateApplication& SlateApp, const FPointerEvent& Event) { return OverView(SlateApp, Event.GetScreenSpacePosition()); }
	static bool OverView(FSlateApplication& SlateApp, const FVector2D& Position);

	static TWeakPtr<SWidget> Excluded;

	FKey Held;
	FVector2D PressedAt = FVector2D::ZeroVector;
	FVector2D Last = FVector2D::ZeroVector;
	bool bDragging = false;
	/** After a double-click, the release that follows is not another click. */
	bool bSwallowRelease = false;
	FVector2D Look = FVector2D::ZeroVector;
	FVector2D Orbit = FVector2D::ZeroVector;
	FVector2D Pan = FVector2D::ZeroVector;
	float ZoomDrag = 0.0f;
	float Wheel = 0.0f;
	TArray<FClick> Clicks;
};
