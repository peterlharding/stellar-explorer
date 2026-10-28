using UnrealBuildTool;

public class StellarSpikeTarget : TargetRules
{
	public StellarSpikeTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("StellarSpike");
	}
}
