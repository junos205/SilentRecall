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

    // 무기 교체 요청 (단축키/휠)
    UFUNCTION(BlueprintCallable)
    void RequestSwitchWeapon(EWeaponSlot NewSlot);

    UFUNCTION(BlueprintCallable)
    void CycleWeapon(bool bNext);

    // 애니메이션 노티파이용 함수
    UFUNCTION(BlueprintCallable)
    void FinishUnEquip(); // Sheath 애니메이션 종료 시 호출

    UFUNCTION(BlueprintCallable)
    void FinishEquip();   // Equip 애니메이션 종료 시 호출

    FORCEINLINE EWeaponSlot GetCurrentActiveSlot() const { return CurrentActiveSlot; }

    // ⭐️ [복구] 현재 3P 메쉬에 들고 있는 진짜 무기 액터(원본)를 반환
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
        // 현재 슬롯이 Loadout 맵에 존재하는지 확인하고 반환
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

    // 특정 타입(Tag)의 현재 예비 탄약(Reserve) 개수를 반환합니다.
    UFUNCTION(BlueprintPure, Category = "Inventory|Ammo")
    int32 GetReserveAmmo(FGameplayTag AmmoTag) const;
    
public:
    UPROPERTY(BlueprintAssignable)
    FOnWeaponChangedSignature OnWeaponChanged;

protected:
    void BeginUnEquip();

    UPROPERTY()
    TMap<EWeaponSlot, class USRWeaponInstance*> WeaponLoadout;
    
    UPROPERTY()
    TMap<EWeaponSlot, class AActor*> SpawnedWeapons;

    EWeaponSlot CurrentActiveSlot = EWeaponSlot::None;
    EWeaponSlot NextSlotToEquip = EWeaponSlot::None;
    bool bIsSwitchingWeapon = false;

    UPROPERTY()
    TArray<FGameplayAbilitySpecHandle> CurrentGrantedAbilityHandles;

protected:
    // 무기 타입(Tag)별 현재 보유 탄약량 (예: Weapon.Ammo.Rifle -> 120)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Ammo")
    TMap<FGameplayTag, int32> AmmoReserve;

    // 탄약 타입별 최대 소지량 제한
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Ammo")
    TMap<FGameplayTag, int32> MaxAmmoCapacity;

public:
    // 탄약 획득 (탄약 상자 등을 먹었을 때)
    UFUNCTION(BlueprintCallable, Category = "Inventory|Ammo")
    void AddReserveAmmo(FGameplayTag AmmoTag, int32 Amount);

    // 현재 들고 있는 무기 장전
    UFUNCTION(BlueprintCallable, Category = "Inventory|Ammo")
    void ReloadCurrentWeapon();
};