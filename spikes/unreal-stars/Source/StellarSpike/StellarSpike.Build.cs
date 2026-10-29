using UnrealBuildTool;

public class StellarSpike : ModuleRules
{
	public StellarSpike(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bWarningsAsErrors = true;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "ProceduralMeshComponent", "SQLiteCore", "HeadMountedDisplay", "InputCore" });
		if (Target.Platform == UnrealTargetPlatform.VisionOS)
		{
			// VisionOSStartup.cpp calls the engine's IOSSceneDelegate.
			PrivateDependencyModuleNames.Add("ApplicationCore");
		}
	}
}
