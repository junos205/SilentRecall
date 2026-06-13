// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SilentRecall : ModuleRules
{
	public SilentRecall(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
       
		PrivateIncludePaths.Add(ModuleDirectory);
       
		// 1. 에디터 전용 모듈을 제외한 순수 런타임/게임 모듈들입니다.
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", 
			"MotionWarping", "UMG", "CableComponent", "ProceduralMeshComponent", 
			"GameplayCameras", "AIModule", "StateTreeModule", "GameplayStateTreeModule", "Niagara" 
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"LevelSequence", 
			"MovieScene",
			"GameplayAbilities",
			"GameplayTasks",
			"GameplayTags", "EngineCameras", "GameplayStateTreeModule"
		});

		// 2. 패키징할 때는 제외되고, 언리얼 에디터로 켤 때만 로드되도록 분리했습니다.
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "DataLayerEditor" });
		}
	}
}