#pragma once

#include "CoreMinimal.h"

/**
 * Look-and-pinch on visionOS: where the wearer looked as each pinch began, and where the
 * pinching hand goes until it ends, from the system's spatial events (VisionOS/GazePinch.swift).
 * The app never sees where the eyes are between pinches, only this. Elsewhere there are none.
 */
namespace GazePinch
{
	enum class EPhase : uint8
	{
		Began,
		Moved,
		Ended,
	};

	/** A pinch beginning, its hand moving, or its end, in this app's world. */
	struct FEvent
	{
		uint64 Id = 0;
		EPhase Phase = EPhase::Began;
		/** 1 looked at and pinched, 2 a direct touch, 0 other. */
		int32 Kind = 0;
		/** The ray the wearer looked along as the pinch began, when the system gave one. */
		bool bHasRay = false;
		FVector Origin = FVector::ZeroVector;
		FVector Direction = FVector::ZeroVector;
		/** Where the pinch is now, when the system gave it. */
		bool bHasHand = false;
		FVector Hand = FVector::ZeroVector;
	};

	/** Start listening, once the engine has its immersive layer; true once listening. Call each frame until it is. */
	bool Listen();

	/** The events since the last call, in order. */
	TArray<FEvent> Take();
}
