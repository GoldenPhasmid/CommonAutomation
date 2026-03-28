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
				"CoreUObject",
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
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
		
		if (Target.Version.MinorVersion >= 5)
		{
			PublicDefinitions.Add("AUTOTEST_FILTER_MASK=EAutomationTestFlags_FilterMask");
			PublicDefinitions.Add("AUTOTEST_APPLICATION_MASK=EAutomationTestFlags_ApplicationContextMask");
		}
		else
		{
			PublicDefinitions.Add("AUTOTEST_FILTER_MASK=EAutomationTestFlags::FilterMask");
			PublicDefinitions.Add("AUTOTEST_APPLICATION_MASK=EAutomationTestFlags::ApplicationContextMask");
		}
	}
}
