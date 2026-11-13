// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClassroomLevelling : ModuleRules
{
	public ClassroomLevelling(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ClassroomLevelling",
			"ClassroomLevelling/Variant_Platforming",
			"ClassroomLevelling/Variant_Platforming/Animation",
			"ClassroomLevelling/Variant_Combat",
			"ClassroomLevelling/Variant_Combat/AI",
			"ClassroomLevelling/Variant_Combat/Animation",
			"ClassroomLevelling/Variant_Combat/Gameplay",
			"ClassroomLevelling/Variant_Combat/Interfaces",
			"ClassroomLevelling/Variant_Combat/UI",
			"ClassroomLevelling/Variant_SideScrolling",
			"ClassroomLevelling/Variant_SideScrolling/AI",
			"ClassroomLevelling/Variant_SideScrolling/Gameplay",
			"ClassroomLevelling/Variant_SideScrolling/Interfaces",
			"ClassroomLevelling/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
