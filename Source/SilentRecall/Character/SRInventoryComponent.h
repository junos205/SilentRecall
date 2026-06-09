// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySystemComponent.h"
#include "Data/SRCharacterData.h"
#include "SRInventoryComponent.generated.h"

// 🌟 [수리 1] UI 동기화 전용 4개짜리 멀티캐스트 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnWeaponHUDChangedSignature, class USRWeaponDataAsset*, NewWeaponData, int32, CurrentAmmo, int32, ReserveAmmo, bool, bIsRanged);

// 🌟 [수리 2] 애니메이션 레이어 링크 및 기존 연동용 1개짜리 멀티캐스트 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponChangedSignature, class USRWeaponDataAsset*, NewWeaponData);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class SILENTRECALL_API USRInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USRInventoryComponent();

    /** 🌟 인벤토리 상태를 게임 인스턴스에 백업 */
    void SaveToGameInstance(class USRGameInstance* GI);

    /** 🌟 게임 인스턴스로부터 인벤토리 상태를 완벽 복구 */
    void LoadFromGameInstance(class USRGameInstance* GI);
    
    // 🌟 [수리 3] 세이브 로딩용 bInitializeQuietly 가드 인자를 헤더에도 정식 매핑 (기본값 false)
    UFUNCTION(BlueprintCallable)
    bool AddWeapon(EWeaponSlot SlotType, class USRWeaponInstance* NewInstance, class AActor* PickedUpWeaponActor, bool bInitializeQuietly = false);

    void RequestSwitchWeapon(EWeaponSlot NewSlot, bool bForceOverride = false);
    void FinishEquip();

    // 🎯 인자를 직접 받아 처리하는 원자적 교체식으로 통합 변경
    void PrepareWeaponSwitch(EWeaponSlot TargetSlot);
    
    UFUNCTION(BlueprintCallable)
    void CycleWeapon(bool bNext);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapon")
    void SetCurrentActiveWeaponVisibility(bool bNewVisibility);

    FORCEINLINE EWeaponSlot GetCurrentActiveSlot() const { return CurrentActiveSlot; }

    FORCEINLINE EWeaponSlot GetNextSlotToEquip() const { return NextSlotToEquip; }
    
    // 현재 3P 메쉬에 들고 있는 진짜 무기 액터(원본)를 반환
    FORCEINLINE class AActor* GetCurrentActiveWeaponActor() const 
    {
        if (CurrentActiveSlot != EWeaponSlot::None && SpawnedWeapons.Contains(CurrentActiveSlot))
        {
            return SpawnedWeapons[CurrentActiveSlot];
        }
        return nullptr;
    }

    UFUNCTION(BlueprintPure, Category = "Inventory|Weapon")
    class USRWeaponInstance* GetCurrentActiveWeaponInstance() const
    {
        if (WeaponLoadout.Contains(CurrentActiveSlot))
        { 
            return WeaponLoadout[CurrentActiveSlot];
        }
        return nullptr;
    }
    
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    class USRWeaponInstance* GetWeaponInSlot(EWeaponSlot SlotType) const
    {
        if (WeaponLoadout.Contains(SlotType))
        {
            return WeaponLoadout[SlotType];
        }
        return nullptr;
    }

    // UI 무전기 작동 신호
    void RefreshWeaponHUD();
  
    int32 GetReserveAmmo(FGameplayTag AmmoTag) const;

    // 🌟 [수리 4] 4개짜리 UI 전용 시그니처 매핑
    UPROPERTY(BlueprintAssignable, Category = "Inventory|UI")
    FOnWeaponHUDChangedSignature OnWeaponHUDChanged;
    
public:
    // 🌟 [수리 5] 기존 1개짜리 시스템 전용 시그니처 매핑 (컴파일 에러 박멸)
    UPROPERTY(BlueprintAssignable)
    FOnWeaponChangedSignature OnWeaponChanged;

protected:
    UPROPERTY()
    TMap<EWeaponSlot, class USRWeaponInstance*> WeaponLoadout;
    
    UPROPERTY()
    TMap<EWeaponSlot, class AActor*> SpawnedWeapons;

    EWeaponSlot CurrentActiveSlot = EWeaponSlot::None;
    EWeaponSlot NextSlotToEquip = EWeaponSlot::None;
    bool bIsSwitchingWeapon = false;
    
    TArray<FGameplayAbilitySpecHandle> CurrentGrantedAbilityHandles;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Ammo")
    TMap<FGameplayTag, int32> AmmoReserve;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Ammo")
    TMap<FGameplayTag, int32> MaxAmmoCapacity;

public:
    UFUNCTION(BlueprintCallable, Category = "Inventory|Ammo")
    void AddReserveAmmo(FGameplayTag AmmoTag, int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Ammo")
    void ReloadCurrentWeapon();
};