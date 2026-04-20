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

    class USRWeaponInstance* GetCurrentActiveWeaponInstance() const
    {
        // 현재 슬롯이 Loadout 맵에 존재하는지 확인하고 반환
        if (WeaponLoadout.Contains(CurrentActiveSlot))
        {
            return WeaponLoadout[CurrentActiveSlot];
        }
        return nullptr;
    }
    
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
};