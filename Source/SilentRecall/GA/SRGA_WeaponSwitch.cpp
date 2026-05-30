#include "GA/SRGA_WeaponSwitch.h"
#include "Character/SRInventoryComponent.h"
#include "GA/AT/SRAT_Play1PMontageAndWait.h"
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"

USRGA_WeaponSwitch::USRGA_WeaponSwitch()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor; 
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

    // 🟢 [개편] 기존 Unequip 단계를 완전히 건너뛰고, 즉시 무기 스왑 데이터 및 소켓 처리 가동
    if (CachedInventory)
    {
        CachedInventory->PrepareWeaponSwitch();
    }

    PlayEquipStep();
}

void USRGA_WeaponSwitch::PlayEquipStep()
{
    USRWeaponInstance* NewWeapon = CachedInventory ? CachedInventory->GetCurrentActiveWeaponInstance() : nullptr;
    if (!NewWeapon || !NewWeapon->WeaponData || !NewWeapon->WeaponData->EquipMontage)
    {
        OnEquipCompleted();
        return;
    }

    UAnimMontage* EquipMontage = NewWeapon->WeaponData->EquipMontage;

    // 오직 장착 몽타주만 정방향(1.0f)으로 재생합니다.
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