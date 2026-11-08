// Look-and-pinch on visionOS: the system's spatial events for the immersive layer, which carry
// where the wearer looked as each pinch began. Unreal's own Swift (SwiftMain.swift) does not ask
// for them; GazePinch.cpp hands this the layer once the engine has it.

#if os(visionOS)

import CompositorServices
import SwiftUI

/** Listen for the layer's spatial events, passing each to StellarGazePinch, as it begins, moves and ends. Call on the main thread. */
@_cdecl("StellarListenForGazePinches")
public func StellarListenForGazePinches(_ layer: UnsafeMutableRawPointer) {
	let renderer = Unmanaged<LayerRenderer>.fromOpaque(layer).takeUnretainedValue()
	MainActor.assumeIsolated {
		renderer.onSpatialEvent = { events in
			for event in events {
				let phase: Int32 = event.phase == .active ? 0 : (event.phase == .ended ? 1 : 2)
				let kind: Int32 = event.kind == .indirectPinch ? 1 : (event.kind == .directPinch ? 2 : 0)
				let id = UInt64(bitPattern: Int64(event.id.hashValue))
				let ray = event.selectionRay
				let hand = event.inputDevicePose?.pose3D.position
				StellarGazePinch(id, phase, kind,
					ray != nil ? 1 : 0, ray?.origin.x ?? 0, ray?.origin.y ?? 0, ray?.origin.z ?? 0, ray?.direction.x ?? 0, ray?.direction.y ?? 0, ray?.direction.z ?? 0,
					hand != nil ? 1 : 0, hand?.x ?? 0, hand?.y ?? 0, hand?.z ?? 0)
			}
		}
	}
}

#endif
