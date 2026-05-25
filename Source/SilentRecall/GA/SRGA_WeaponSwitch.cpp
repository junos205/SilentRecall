// Fill out your copyright notice in the Description page of Project Settings.

#include "SRGA_WeaponSwitch.h"
#include "Character/SRInventoryComponent.h"
#include "GA/AT/SRAT_Play1PMontageAndWait.h" // 🟢 유저님의 명품 타이머 태스크
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"

USRGA_WeaponSwitch::USRGA_WeaponSwitch()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;; 
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.WeaponSwitch")));
}

void USRGA_WeaponSwitch::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    CachedInventory = ActorInfo->AvatarActor->FindComponentByClass<USRInventoryComponent>();
    if (!CachedInventory)
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    PlayUnEquipStep();
}

void USRGA_WeaponSwitch::PlayUnEquipStep()
{
    // ⭐️ [해결] 인벤토리가 유저님 변수명에 맞춰 안전하게 보관해둔 진짜 구무기 몽타주를 직접 인출합니다!
    UAnimMontage* UnEquipMontage = CachedInventory->GetWeaponSwitchUnEquipMontage();
    float PlayRate = 1.0f;

    if (UnEquipMontage)
    {
        if (UnEquipMontage->GetName().Contains(TEXT("Equip")) && !UnEquipMontage->GetName().Contains(TEXT("Unequip")))
        {
            PlayRate = -1.0f;
        }

        USRAT_Play1PMontageAndWait* UnEquipTask = USRAT_Play1PMontageAndWait::CreatePlay1PMontageAndWaitProxy(
            this, NAME_None, UnEquipMontage, PlayRate
        );

        if (UnEquipTask)
        {
            UnEquipTask->OnCompleted.AddDynamic(this, &USRGA_WeaponSwitch::OnUnEquipCompleted);
            
            UE_LOG(LogTemp, Warning, TEXT("[스왑GA 🎉] 인벤토리 변수 직결 성공. 몽타주 [%s] 동시 격발 시동!"), *UnEquipMontage->GetName());
            UnEquipTask->ReadyForActivation();
            
            // 애니메이션이 켜진 직후 안전 마디에서 데이터 해제 요청
            CachedInventory->BeginUnEquip();
            return; 
        }
    }
    
    OnUnEquipCompleted();
}

void USRGA_WeaponSwitch::OnUnEquipCompleted()
{
    if (CachedInventory)
    {
        CachedInventory->FinishUnEquip();
    }
    PlayEquipStep();
}

void USRGA_WeaponSwitch::PlayEquipStep()
{
    USRWeaponInstance* NewWeapon = CachedInventory->GetCurrentActiveWeaponInstance();
    if (!NewWeapon || !NewWeapon->WeaponData || !NewWeapon->WeaponData->EquipMontage)
    {
        OnEquipCompleted();
        return;
    }

    UAnimMontage* EquipMontage = NewWeapon->WeaponData->EquipMontage;

    USRAT_Play1PMontageAndWait* EquipTask = USRAT_Play1PMontageAndWait::CreatePlay1PMontageAndWaitProxy(
        this, NAME_None, EquipMontage, 1.0f
    );

    if (EquipTask)
    {
        EquipTask->OnCompleted.AddDynamic(this, &USRGA_WeaponSwitch::OnEquipCompleted);
        EquipTask->ReadyForActivation();
        return;
    }

    OnEquipCompleted();
}

void USRGA_WeaponSwitch::OnEquipCompleted()
{
    if (CachedInventory)
    {
        CachedInventory->FinishEquip();
    }
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USRGA_WeaponSwitch::OnSwitchInterrupted()
{
    if (CachedInventory) { CachedInventory->FinishEquip(); }
    CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}