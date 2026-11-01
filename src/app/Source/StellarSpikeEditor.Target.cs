using UnrealBuildTool;

public class StellarSpikeEditorTarget : TargetRules
{
	public StellarSpikeEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("StellarSpike");
	}
}
