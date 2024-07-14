// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CommonAutomation : ModuleRules
{
	public CommonAutomation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PrivateDefinitions.AddRange(
			new string []
			{
				"REUSE_GAME_INSTANCE=0",
			}
		);
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"NavigationSystem",
				"EngineSettings",
				"DeveloperSettings",
				"CommonAutomationRuntime",
				"AssetRegistry", 
				"GameProjectGeneration",
				"UnrealEd",
				"StructUtils",
			}
		);
	}
}
