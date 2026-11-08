#include "DesktopMouse.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

namespace
{
	/** A press moved further than this, in screen pixels, is a drag, not a click. */
	constexpr double DragPixels = 4.0;
}

TWeakPtr<SWidget> FDesktopMouse::Excluded;

bool FDesktopMouse::OverView(FSlateApplication& SlateApp, const FVector2D& Position)
{
	if (GEngine == nullptr || GEngine->GameViewport == nullptr)
	{
		return false;
	}
	const TSharedPtr<SWindow> Game = GEngine->GameViewport->GetWindow();
	const FWidgetPath Path = SlateApp.LocateWindowUnderMouse(Position, SlateApp.GetInteractiveTopLevelWindows());
	if (!Game.IsValid() || !Path.IsValid() || Path.GetWindow() != Game)
	{
		return false;
	}
	const TSharedPtr<SWidget> Panel = Excluded.Pin();
	return !Panel.IsValid() || !Path.ContainsWidget(Panel.Get());
}

bool FDesktopMouse::IsCursorOverView()
{
	return FSlateApplication::IsInitialized() && OverView(FSlateApplication::Get(), FSlateApplication::Get().GetCursorPos());
}

bool FDesktopMouse::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event)
{
	const FKey Button = Event.GetEffectingButton();
	if ((Button == EKeys::LeftMouseButton || Button == EKeys::RightMouseButton || Button == EKeys::MiddleMouseButton) && !Held.IsValid() &&
		OverGame(SlateApp, Event))
	{
		Held = Button;
		PressedAt = Last = Event.GetScreenSpacePosition();
		bDragging = false;
		bSwallowRelease = false;
	}
	return false;
}

bool FDesktopMouse::HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& Event)
{
	// Slate sends this in place of the second press.
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && OverGame(SlateApp, Event))
	{
		Clicks.Add({true});
		Held = EKeys::LeftMouseButton;
		PressedAt = Last = Event.GetScreenSpacePosition();
		bDragging = false;
		bSwallowRelease = true;
	}
	return false;
}

bool FDesktopMouse::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& Event)
{
	if (!Held.IsValid())
	{
		return false;
	}
	const FVector2D Position = Event.GetScreenSpacePosition();
	if (!bDragging && FVector2D::Distance(Position, PressedAt) > DragPixels)
	{
		bDragging = true;
		// The few pixels before the drag was known count too.
		Last = PressedAt;
	}
	if (bDragging)
	{
		const FVector2D Moved = Position - Last;
		Last = Position;
		if (Held != EKeys::MiddleMouseButton)
		{
			Look += Moved;
		}
		else if (Event.IsShiftDown())
		{
			Pan += Moved;
		}
		// Control or Command: on the Mac the engine reports the Control key as Command.
		else if (Event.IsControlDown() || Event.IsCommandDown())
		{
			ZoomDrag -= static_cast<float>(Moved.Y);
		}
		else
		{
			Orbit += Moved;
		}
	}
	return false;
}

bool FDesktopMouse::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != Held)
	{
		return false;
	}
	if (Held == EKeys::LeftMouseButton && !bDragging && !bSwallowRelease)
	{
		Clicks.Add({false});
	}
	Held = FKey();
	bDragging = false;
	bSwallowRelease = false;
	return false;
}

bool FDesktopMouse::HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& Event, const FPointerEvent* Gesture)
{
	if (Gesture == nullptr && OverGame(SlateApp, Event))
	{
		Wheel += Event.GetWheelDelta();
	}
	return false;
}

FVector2D FDesktopMouse::TakeLook()
{
	const FVector2D Taken = Look;
	Look = FVector2D::ZeroVector;
	return Taken;
}

FVector2D FDesktopMouse::TakeOrbit()
{
	const FVector2D Taken = Orbit;
	Orbit = FVector2D::ZeroVector;
	return Taken;
}

FVector2D FDesktopMouse::TakePan()
{
	const FVector2D Taken = Pan;
	Pan = FVector2D::ZeroVector;
	return Taken;
}

float FDesktopMouse::TakeZoomDrag()
{
	const float Taken = ZoomDrag;
	ZoomDrag = 0.0f;
	return Taken;
}

float FDesktopMouse::TakeWheel()
{
	const float Taken = Wheel;
	Wheel = 0.0f;
	return Taken;
}

TArray<FDesktopMouse::FClick> FDesktopMouse::TakeClicks()
{
	return MoveTemp(Clicks);
}
