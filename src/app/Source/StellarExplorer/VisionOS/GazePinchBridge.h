// Between GazePinch.swift and the C++ (GazePinch.cpp), on visionOS: the header Unreal's build
// tool gives the Swift compiler (SwiftInteropHeader in StellarExplorer.Build.cs), so the Swift can
// call back with each pinch.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A look-and-pinch, from the system's spatial events, on the main thread, each time it changes.
 * Phase: 0 active, 1 ended, 2 cancelled. Kind: 1 an indirect pinch (looked at and pinched), 2 a
 * direct touch, 0 other. The ray is where the wearer looked as the pinch began (HasRay 0 if the
 * system gave none), and the hand where the pinch is now (HasHand 0 if none), in metres in the
 * immersive space: right-handed, y up.
 */
void StellarGazePinch(uint64_t Id, int32_t Phase, int32_t Kind, int32_t HasRay, double OriginX, double OriginY, double OriginZ, double DirectionX,
	double DirectionY, double DirectionZ, int32_t HasHand, double HandX, double HandY, double HandZ);

#ifdef __cplusplus
}
#endif
