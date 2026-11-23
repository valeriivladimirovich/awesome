using UnrealBuildTool;

public class LastBlockEditorTarget : TargetRules
{
	public LastBlockEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LastBlock");
	}
}
