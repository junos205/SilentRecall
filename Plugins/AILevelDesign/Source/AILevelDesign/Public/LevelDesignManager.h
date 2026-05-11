// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelDesignManager.generated.h"

// 🌟 AI가 한 번 생성할 때 만들어진 액터들을 묶어두는 스택 구조체
USTRUCT(BlueprintType)
struct FGenerationStep
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "AI Level Design")
    TArray<AActor*> SpawnedActors;
};

UCLASS()
class AILEVELDESIGN_API ALevelDesignManager : public AActor
{
    GENERATED_BODY()
    
public:	
    ALevelDesignManager();

    static ALevelDesignManager* GetAILevelManager(UObject* WorldContextObject);

    // 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UArrowComponent* DirectionArrow;

    // AI 설정 및 프롬프트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Level Design")
    FString CustomPrompt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Level Design|Grid")
    float VoxelSize = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Level Design|Grid")
    int32 GridWidthX = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Level Design|Grid")
    int32 GridHeightY = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Level Design|Grid")
    int32 GridDepthZ = 10;

    // 에셋 딕셔너리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Level Design|Assets")
    TMap<FString, TSubclassOf<AActor>> AssetDictionary;

    // 현재 관리 중인 전체 액터
    UPROPERTY(VisibleAnywhere, Category = "AI Level Design|State")
    TArray<AActor*> ManagedActors;

    // AI 실행 상태
    UPROPERTY(VisibleAnywhere, Category = "AI Level Design|State")
    bool bIsAIRunning = false;

    // 🌟 작업 스택 (1, 2, 3... 순서대로 묶음 저장)
    UPROPERTY(VisibleAnywhere, Category = "AI Level Design|History")
    TArray<FGenerationStep> HistoryStack;

    // 버튼 (에디터 노출)
    UFUNCTION(CallInEditor, Category = "AI Level Design|Actions")
    void RunAIWorkflow();

    UFUNCTION(CallInEditor, Category = "AI Level Design|Actions")
    void CancelAIWorkflow();

    // 🌟 실행 취소 (Ctrl+Z) 및 전체 초기화 버튼
    UFUNCTION(CallInEditor, Category = "AI Level Design|History")
    void UndoLastGeneration();

    UFUNCTION(CallInEditor, Category = "AI Level Design|History")
    void ClearAllGeneratedLevel();

    void ProcessSpawnData();

private:
    AActor* FindActorByID(const FString& ID);
    FProcHandle AIProcessHandle;
};