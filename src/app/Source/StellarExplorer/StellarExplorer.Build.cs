using UnrealBuildTool;

public class StellarExplorer : ModuleRules
{
	public StellarExplorer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bWarningsAsErrors = true;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "ProceduralMeshComponent", "HeadMountedDisplay", "InputCore" });
		// The release version, ProjectVersion in the project settings (BuildStamp.h).
		PrivateDependencyModuleNames.Add("EngineSettings");
		// Catalogue access and astrometry come from the engine-independent core (ADR 0014),
		// not the engine's SQLite plugin.
		PrivateDependencyModuleNames.Add("StellarCore");
		// The desktop look reads the mouse from Slate (StarViewerPawn).
		PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore" });
		// The control service the desktop console talks to (StarControl): JSON lines over TCP.
		PrivateDependencyModuleNames.AddRange(new[] { "Networking", "Sockets", "Json" });
		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.VisionOS)
		{
			// Voice commands use Apple's speech recogniser on the microphone (VoiceRecognizer.cpp).
			PublicFrameworks.AddRange(new[] { "Speech", "AVFoundation" });
		}
		if (Target.Platform == UnrealTargetPlatform.VisionOS)
		{
			// VisionOSStartup.cpp calls the engine's IOSSceneDelegate, and GazePinch.cpp its app delegate.
			PrivateDependencyModuleNames.Add("ApplicationCore");
			// VisionOS/GazePinch.swift asks the system for look-and-pinch; this header is what it sees of the C++.
			SwiftInteropHeader = System.IO.Path.Combine(ModuleDirectory, "VisionOS/GazePinchBridge.h");
		}
	}
}
