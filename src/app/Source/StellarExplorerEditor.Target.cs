using UnrealBuildTool;

public class StellarExplorerEditorTarget : TargetRules
{
	public StellarExplorerEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("StellarExplorer");
	}
}
