using UnrealBuildTool;

public class StellarSpike : ModuleRules
{
	public StellarSpike(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bWarningsAsErrors = true;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "ProceduralMeshComponent", "HeadMountedDisplay", "InputCore" });
		// Catalogue access and astrometry come from the engine-independent core (ADR 0014),
		// not the engine's SQLite plugin.
		PrivateDependencyModuleNames.Add("StellarCore");
		if (Target.Platform == UnrealTargetPlatform.VisionOS)
		{
			// VisionOSStartup.cpp calls the engine's IOSSceneDelegate.
			PrivateDependencyModuleNames.Add("ApplicationCore");
		}
	}
}
