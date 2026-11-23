using UnrealBuildTool;

public class LastBlock : ModuleRules
{
	public LastBlock(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"AIModule", "GameplayTasks", "NavigationSystem",
			"Niagara", "UMG", "Slate", "SlateCore", "PhysicsCore", "DeveloperSettings"
		});
	}
}
