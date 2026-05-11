// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AILevelDesign : ModuleRules
{
	public AILevelDesign(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
       
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
		);
             
       
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
		);
          
       
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Json",             // 🌟 JSON 처리 필수
				"JsonUtilities",    // 🌟 JSON 처리 필수
				"Projects"          // 🌟 플러그인 내부 경로를 찾기 위해 필수
				// ... add other public dependencies that you statically link with here ...
			}
		);
          
       
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ... 
			}
		);
       
       
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}