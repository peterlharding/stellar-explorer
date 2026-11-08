#include "GazePinch.h"

#if PLATFORM_VISIONOS

#include "Engine/Engine.h"
#include "IOS/IOSAppDelegate.h"
#include "IXRTrackingSystem.h"
#include "Misc/ScopeLock.h"
#include "VisionOS/GazePinchBridge.h"

DEFINE_LOG_CATEGORY_STATIC(LogGazePinch, Log, All);

extern "C" void StellarListenForGazePinches(void* Layer);

namespace
{
	FCriticalSection Lock;
	/** Events as the system gave them: metres in the immersive space, right-handed, y up. */
	TArray<GazePinch::FEvent> Pending;
	/** The pinches seen active and not yet ended: the first of each begins it, the rest move it. */
	TSet<uint64> Active;
}

extern "C" void StellarGazePinch(uint64_t Id, int32_t Phase, int32_t Kind, int32_t HasRay, double OriginX, double OriginY, double OriginZ, double DirectionX,
	double DirectionY, double DirectionZ, int32_t HasHand, double HandX, double HandY, double HandZ)
{
	FScopeLock Guard(&Lock);
	GazePinch::FEvent Event;
	Event.Id = Id;
	Event.Kind = Kind;
	if (Phase != 0)
	{
		if (Active.Remove(Id) == 0)
		{
			return;
		}
		Event.Phase = GazePinch::EPhase::Ended;
	}
	else
	{
		bool bSeen = false;
		Active.Add(Id, &bSeen);
		Event.Phase = bSeen ? GazePinch::EPhase::Moved : GazePinch::EPhase::Began;
	}
	Event.bHasRay = HasRay != 0;
	Event.Origin = FVector(OriginX, OriginY, OriginZ);
	Event.Direction = FVector(DirectionX, DirectionY, DirectionZ);
	Event.bHasHand = HasHand != 0;
	Event.Hand = FVector(HandX, HandY, HandZ);
	Pending.Add(Event);
}

bool GazePinch::Listen()
{
	CP_OBJECT_cp_layer_renderer* Layer = [IOSAppDelegate GetDelegate].SwiftLayer;
	if (Layer == nullptr)
	{
		return false;
	}
	// The layer's events are the main thread's; the block keeps the layer until it runs.
	dispatch_async(dispatch_get_main_queue(), ^{
		StellarListenForGazePinches((__bridge void*)Layer);
	});
	UE_LOG(LogGazePinch, Display, TEXT("Listening for look-and-pinch"));
	return true;
}

TArray<GazePinch::FEvent> GazePinch::Take()
{
	TArray<FEvent> Events;
	{
		FScopeLock Guard(&Lock);
		Events = MoveTemp(Pending);
		Pending.Reset();
	}
	if (Events.IsEmpty() || !GEngine->XRSystem.IsValid())
	{
		return Events;
	}
	// Into Unreal's axes (x forward, y right, z up, from -z forward, x right, y up), at its
	// scale, then from the tracking space into the world, as the head and hands are.
	const FTransform TrackingToWorld = GEngine->XRSystem->GetTrackingToWorldTransform();
	const double Scale = GEngine->XRSystem->GetWorldToMetersScale();
	const auto Axes = [](const FVector& V) { return FVector(-V.Z, V.X, V.Y); };
	for (FEvent& Event : Events)
	{
		Event.Origin = TrackingToWorld.TransformPosition(Axes(Event.Origin) * Scale);
		Event.Direction = TrackingToWorld.TransformVectorNoScale(Axes(Event.Direction)).GetSafeNormal();
		Event.Hand = TrackingToWorld.TransformPosition(Axes(Event.Hand) * Scale);
	}
	return Events;
}

#else

bool GazePinch::Listen()
{
	return false;
}

TArray<GazePinch::FEvent> GazePinch::Take()
{
	return {};
}

#endif
