using UnrealBuildTool;

public class LastBlockTarget : TargetRules
{
	public LastBlockTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LastBlock");
	}
}
