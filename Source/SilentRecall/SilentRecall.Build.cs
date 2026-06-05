// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SilentRecall : ModuleRules
{
	public SilentRecall(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PrivateIncludePaths.Add(ModuleDirectory);
       
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "MotionWarping", "UMG", "DataLayerEditor", "WorldPartitionEditor",  "CableComponent", "ProceduralMeshComponent", "GameplayCameras", "AIModule", "StateTreeModule", "GameplayStateTreeModule", "Niagara" });

		PrivateDependencyModuleNames.AddRange(new string[] {
			"LevelSequence", 
			"MovieScene",
			"GameplayAbilities",
			"GameplayTasks",
			"GameplayTags", "EngineCameras", "GameplayStateTreeModule"
		});
	}
}