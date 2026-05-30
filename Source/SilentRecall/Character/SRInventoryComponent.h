// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySystemComponent.h"
#include "Data/SRCharacterData.h"
#include "SRInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponChangedSignature, class USRWeaponDataAsset*, NewWeaponData);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class SILENTRECALL_API USRInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public: 
    USRInventoryComponent();

    // 무기 획득
    UFUNCTION(BlueprintCallable)
    bool AddWeapon(EWeaponSlot SlotType, class USRWeaponInstance* NewInstance, class AActor* PickedUpWeaponActor);

    // 기존에 존재하던 스왑 관련 공용 인터페이스 함수들
    void RequestSwitchWeapon(EWeaponSlot NewSlot);
    void FinishEquip();

    // 🎯 [변경] 쪼개져 있던 장착 해제 단계를 하나로 통합한 함수
    void PrepareWeaponSwitch();
    UFUNCTION(BlueprintCallable)
    void CycleWeapon(bool bNext);

    // ⭐️ [신규 추가] 현재 장착 중인 무기 액터의 가시성(Visibility)을 숨기거나 켜는 기능 (처형용)
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapon")
    void SetCurrentActiveWeaponVisibility(bool bNewVisibility);

    FORCEINLINE EWeaponSlot GetCurrentActiveSlot() const { return CurrentActiveSlot; }

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
  
    int32 GetReserveAmmo(FGameplayTag AmmoTag) const;
    
public:
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