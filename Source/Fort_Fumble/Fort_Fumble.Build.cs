// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Fort_Fumble : ModuleRules
{
	public Fort_Fumble(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[]
		{
			ModuleDirectory,
			System.IO.Path.Combine(ModuleDirectory, "Core"),
			System.IO.Path.Combine(ModuleDirectory, "Terrain"),
			System.IO.Path.Combine(ModuleDirectory, "Tower"),
			System.IO.Path.Combine(ModuleDirectory, "Enemy"),
			System.IO.Path.Combine(ModuleDirectory, "Defender"),
			System.IO.Path.Combine(ModuleDirectory, "Economy"),
			System.IO.Path.Combine(ModuleDirectory, "Game")
		});

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ProceduralMeshComponent",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
