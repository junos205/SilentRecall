// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameplayTagContainer.h"
#include "Data/SRCharacterData.h" // 🌟 EWeaponSlot 열거형 헤더
#include "SRGameInstance.generated.h"

// 무기의 종류와 남은 총알을 기억할 경량 구조체
USTRUCT(BlueprintType)
struct FSRSavedWeaponInfo
{
    GENERATED_BODY()

    UPROPERTY()
    class USRWeaponDataAsset* WeaponData = nullptr;

    UPROPERTY()
    int32 CurrentAmmoInMag = 0;
};

UCLASS()
class SILENTRECALL_API USRGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    // =======================================================================
    // 💾 기존 체크포인트 및 적 관리 데이터
    // =======================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
    int32 ActiveCheckpointIndex = 0;

    // =======================================================================
    // 🌟 [정식 추가] 몇 번 구역의 전투(올킬)까지 완수했는지 박제하는 인덱스 창고!
    // =======================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
    int32 CompletedCombatIndex = 0;
    // =======================================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
    FVector SavedLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
    FRotator SavedRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
    TSet<FName> DefeatedEnemyNames;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
    bool bPendingRespawn = false;

    // =======================================================================
    // 💾 플레이어 상태 백업 주머니 (Attributes & Inventory)
    // =======================================================================
    UPROPERTY()
    float SavedHealth = 200.f;
    UPROPERTY()
    float SavedAP = 100.f;
    UPROPERTY()
    float SavedMana = 50.f;
    UPROPERTY()
    float SavedXP = 0.f;
    UPROPERTY()
    float SavedLevel = 1.f;

    UPROPERTY()
    TMap<EWeaponSlot, FSRSavedWeaponInfo> SavedWeaponLoadout;

    UPROPERTY()
    EWeaponSlot SavedActiveSlot = EWeaponSlot::None;

    UPROPERTY()
    TMap<FGameplayTag, int32> SavedAmmoReserve;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
    TSet<FName> ViewedTutorialIDs;

    /** 🎵 레벨 리셋 시 음악 중단 지점 백업 초 */
    UPROPERTY()
    float SavedBGMPlaybackTime = 0.0f;

    /** 🎵 레벨 리셋 시 몇 번째 곡을 듣고 있었는지 기억하는 장부 (0: 1번 곡, 1: 2번 곡) */
    UPROPERTY()
    int32 SavedBGMTrackIndex = 0;
};