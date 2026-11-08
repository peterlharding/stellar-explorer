using UnrealBuildTool;

public class StellarExplorerTarget : TargetRules
{
	public StellarExplorerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("StellarExplorer");
	}
}
