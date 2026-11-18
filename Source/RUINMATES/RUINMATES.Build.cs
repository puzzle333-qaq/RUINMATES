using UnrealBuildTool;

public class RUINMATES : ModuleRules
{
	public RUINMATES(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"EnhancedInput",
			"InputCore",
			"Niagara",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"Slate",
			"SlateCore",
			"UMG",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
